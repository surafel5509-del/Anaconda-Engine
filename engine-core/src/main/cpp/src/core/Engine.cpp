// ============================================================================
//  core/Engine.cpp — frame loop, run states, editor API, demo scene.
//
//  THREADING CONTRACT (see Engine.hpp):
//    * GL thread:  onSurfaceCreated / onSurfaceChanged / renderFrame / shutdown
//    * Any thread: editor API methods — they lock worldMutex_ themselves.
//    * The render loop holds worldMutex_ across physics + render, so editor
//      reads/writes are always frame-consistent.
// ============================================================================
#include "anaconda/core/Engine.hpp"

#include "anaconda/assets/ImageDecoder.hpp"
#include "anaconda/core/Log.hpp"
#include "anaconda/ecs/Components.hpp"
#include "anaconda/scene/EditorSnapshotCodec.hpp"
#include "anaconda/scene/SceneSerializer.hpp"

#include <memory>
#include <sstream>

#ifndef ANACONDA_VERSION
#define ANACONDA_VERSION "0.1.0-dev"
#endif

namespace anaconda {

namespace {

/// Wall-clock delta with an upper clamp: after a debugger pause or OS stall
/// we must not simulate a giant step (spiral of death / tunneling).
constexpr float kMaxFrameDelta = 0.25f;
constexpr float kTargetFpsEmaAlpha = 0.1f;   // smoothing for the toolbar FPS

}  // namespace

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
Engine& Engine::instance() {
    static Engine engine;   // constructed on first use (first JNI call)
    return engine;
}

Engine::Engine() {
    ANA_LOGI("[Engine] Anaconda-Engine %s constructing (C++%ld)", version(),
             static_cast<long>(__cplusplus));
}

const char* Engine::version() { return ANACONDA_VERSION; }

// ---------------------------------------------------------------------------
// GL thread callbacks
// ---------------------------------------------------------------------------
void Engine::onSurfaceCreated() {
    std::lock_guard<std::mutex> lock(worldMutex_);

    renderer_.onSurfaceCreated();      // (re)compiles shaders, meshes, texture
    assets_.reuploadAllOnGl();         // rebuild GL textures for cached decodes
    buildDemoSceneIfNeeded();          // first launch only

    // Frame clock restarts so the first frame's dt isn't huge.
    lastFrameTime_ = std::chrono::steady_clock::now();
    ANA_LOGI("[Engine] surface created (entities=%zu)", world_.aliveCount());
}

void Engine::onSurfaceChanged(int width, int height) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    viewportWidth_.store(width, std::memory_order_relaxed);
    viewportHeight_.store(height, std::memory_order_relaxed);
    renderer_.onSurfaceChanged(width, height);
}

// ---------------------------------------------------------------------------
// Frame loop (GL thread — called from GLSurfaceView.Renderer.onDrawFrame)
// ---------------------------------------------------------------------------
void Engine::renderFrame() {
    // ---- 1) wall-clock dt --------------------------------------------------
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > kMaxFrameDelta) dt = kMaxFrameDelta;

    // Exponential moving average — stable number for the toolbar chip.
    const float instantFps = 1.0f / dt;
    const float smoothed =
        fps_.load(std::memory_order_relaxed) * (1.0f - kTargetFpsEmaAlpha) +
        instantFps * kTargetFpsEmaAlpha;
    fps_.store(smoothed, std::memory_order_relaxed);

    // ---- 2) deferred GL work (asset uploads, context-recovery re-uploads) --
    // Swapped out under the QUEUE mutex, executed under the WORLD mutex —
    // fixed lock order (queue → world), never the reverse → no deadlock.
    std::vector<RenderCommandQueue::Command> glCommands = renderCommands_.drain();

    // ---- 3) simulate + render under the world lock -------------------------
    std::lock_guard<std::mutex> lock(worldMutex_);

    for (RenderCommandQueue::Command& command : glCommands) {
        if (command) command();
    }

    if (runState_.load(std::memory_order_relaxed) == RunState::Play) {
        physics_.step(world_, dt);
    }

    renderer_.beginFrame(Vec4{0.055f, 0.063f, 0.082f, 1.0f} /* editor bg */);
    renderer_.renderScene(world_, camera_, assets_);
    renderer_.endFrame();
}

// ---------------------------------------------------------------------------
// Lifecycle (UI thread — no GL touched)
// ---------------------------------------------------------------------------
void Engine::onPause() {
    paused_.store(true, std::memory_order_relaxed);
    ANA_LOGI("[Engine] onPause (fps=%.1f)", fps_.load(std::memory_order_relaxed));
}

void Engine::onResume() {
    paused_.store(false, std::memory_order_relaxed);
    // Reset the clock so the first frame after resume isn't penalized by the
    // time spent in the background (dt would clamp anyway, but be explicit).
    lastFrameTime_ = std::chrono::steady_clock::now();
    ANA_LOGI("[Engine] onResume");
}

void Engine::shutdown() {
    std::lock_guard<std::mutex> lock(worldMutex_);
    renderer_.shutdown();
    ANA_LOGI("[Engine] shutdown complete");
}

// ---------------------------------------------------------------------------
// Run state
// ---------------------------------------------------------------------------
void Engine::setRunState(RunState state) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    const RunState previous = runState_.load(std::memory_order_relaxed);
    if (previous == state) return;

    if (state == RunState::Play) {
        // Snapshot the authored scene the FIRST time we enter Play from Edit
        // (Pause → Play must NOT re-snapshot paused values as "original").
        if (previous == RunState::Edit) backupPlayState();
        physics_.step(world_, 0.0f);   // settle nothing; just touch stats
    } else if (state == RunState::Edit) {
        // Stop → roll the world back to the authored state.
        restorePlayState();
    }

    runState_.store(state, std::memory_order_relaxed);
    ANA_LOGI("[Engine] run state %d → %d", static_cast<int>(previous),
             static_cast<int>(state));
}

RunState Engine::runState() const {
    return runState_.load(std::memory_order_relaxed);
}

void Engine::backupPlayState() {
    playBackup_.clear();
    for (const ecs::Entity e : world_.aliveEntities()) {
        auto* tf = world_.tryGet<TransformComponent>(e);
        if (tf == nullptr) continue;
        PlayBackup b;
        b.position = tf->position;
        b.rotation = tf->rotation;
        b.scale = tf->scale;
        if (auto* rb = world_.tryGet<RigidBodyComponent>(e)) {
            b.velocity = rb->velocity;
            b.gravityScale = rb->gravityScale;
            b.useGravity = rb->useGravity;
        }
        playBackup_[static_cast<std::uint32_t>(e)] = b;
    }
    ANA_LOGI("[Engine] play backup captured (%zu entities)", playBackup_.size());
}

void Engine::restorePlayState() {
    for (const auto& [rawEntity, backup] : playBackup_) {
        const ecs::Entity e = static_cast<ecs::Entity>(rawEntity);
        if (!world_.valid(e)) continue;   // entity deleted during play
        if (auto* tf = world_.tryGet<TransformComponent>(e)) {
            tf->position = backup.position;
            tf->rotation = backup.rotation;
            tf->scale = backup.scale;
        }
        if (auto* rb = world_.tryGet<RigidBodyComponent>(e)) {
            rb->velocity = backup.velocity;
            rb->gravityScale = backup.gravityScale;
            rb->useGravity = backup.useGravity;
            rb->grounded = false;
        }
    }
    playBackup_.clear();
    ANA_LOGI("[Engine] play state restored (edit mode)");
}

// ---------------------------------------------------------------------------
// Scene snapshot (UI thread) — line protocol documented in ARCHITECTURE §5.4
// ---------------------------------------------------------------------------
std::string Engine::sceneSnapshot() {
    std::lock_guard<std::mutex> lock(worldMutex_);
    std::string out;
    for (const ecs::Entity e : world_.aliveEntities()) {
        // One codec = one protocol definition for UI reads AND undo restores.
        std::string line = scene::EditorSnapshotCodec::writeEntity(world_, e);
        if (!line.empty()) out += line + '\n';
    }
    return out;
}

int Engine::restoreEntity(const std::string& snapshotLine, std::string* error) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    const ecs::Entity e = scene::EditorSnapshotCodec::parseEntity(
        world_, snapshotLine, error);
    if (e == ecs::kNullEntity) return -1;
    demoSceneBuilt_ = true;
    return static_cast<int>(e);
}

// ---------------------------------------------------------------------------
// Entity editing API (UI thread)
// ---------------------------------------------------------------------------
int Engine::createEntity(const std::string& name) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    const ecs::Entity e = world_.create();
    world_.add(e, TagComponent{name.empty() ? "Entity" : name});
    world_.add(e, TransformComponent{});
    ANA_LOGI("[Engine] created entity %u '%s'", e, name.c_str());
    return static_cast<int>(e);   // C++20: well-defined two's-complement cast
}

void Engine::destroyEntity(std::uint32_t entity) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) {
        ANA_LOGW("[Engine] destroyEntity(%u) — stale handle ignored", entity);
        return;
    }
    playBackup_.erase(entity);
    world_.destroy(entity);
    ANA_LOGI("[Engine] destroyed entity %u", entity);
}

void Engine::setEntityName(std::uint32_t entity, const std::string& name) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    auto* tag = world_.tryGet<TagComponent>(entity);
    if (tag != nullptr) tag->name = name;
}

void Engine::setTransform(std::uint32_t entity, const Vec3& position,
                          const Vec3& rotationRad, const Vec3& scale) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* tf = world_.tryGet<TransformComponent>(entity)) {
        tf->position = position;
        tf->rotation = rotationRad;
        tf->scale = scale;
    }
}

void Engine::setSpriteColor(std::uint32_t entity, const Vec4& color) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* sprite = world_.tryGet<SpriteComponent>(entity)) sprite->color = color;
}

void Engine::setRigidBodyGravityScale(std::uint32_t entity, float scale) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* rb = world_.tryGet<RigidBodyComponent>(entity)) rb->gravityScale = scale;
}

void Engine::setRigidBodyUseGravity(std::uint32_t entity, bool useGravity) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* rb = world_.tryGet<RigidBodyComponent>(entity)) rb->useGravity = useGravity;
}

// ---------------------------------------------------------------------------
// Gestures (UI thread) → semantic camera commands
// ---------------------------------------------------------------------------
void Engine::handleGesture(EditorGesture type, float dx, float dy, float x,
                           float y) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    const int vh = viewportHeight_.load(std::memory_order_relaxed);

    switch (type) {
        case EditorGesture::Pan:
            camera_.panPixels(dx, dy, vh);
            break;
        case EditorGesture::Zoom:
            camera_.zoomBy(dx);   // dx carries the pinch scale factor
            break;
        case EditorGesture::Tap:
            // Phase 3: unproject (x, y) through the camera and ray-test
            // PhysicsSystem::raycastAabb for viewport selection. For now the
            // tap is intentionally ignored (log-only).
            ANA_LOGD("[Engine] tap @ (%.0f, %.0f) — picking lands in Phase 3",
                     x, y);
            (void)x;   // used by ANA_LOGD only on Android builds
            (void)y;
            break;
    }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------
float Engine::fps() const { return fps_.load(std::memory_order_relaxed); }

int Engine::entityCount() {
    std::lock_guard<std::mutex> lock(worldMutex_);
    return static_cast<int>(world_.aliveCount());
}

// ---------------------------------------------------------------------------
// Persistence (Month 2) — scene files are the unit of user work.
// ---------------------------------------------------------------------------
bool Engine::saveScene(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    const bool ok = scene::SceneSerializer::saveToFile(world_, path, error);
    if (ok) {
        ANA_LOGI("[Engine] scene saved → %s (%zu entities)", path.c_str(),
                 world_.aliveCount());
    } else if (error != nullptr) {
        ANA_LOGE("[Engine] scene save failed: %s", error->c_str());
    }
    return ok;
}

bool Engine::loadScene(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(worldMutex_);

    // Leave Play cleanly BEFORE touching the world: restore authored state so
    // the backup map doesn't hold handles into the cleared registry.
    runState_.store(RunState::Edit, std::memory_order_relaxed);
    restorePlayState();          // clears playBackup_
    playBackup_.clear();

    const bool ok = scene::SceneSerializer::loadFromFile(world_, path, error);
    if (ok) {
        demoSceneBuilt_ = true;  // never re-seed the demo over user content
        ANA_LOGI("[Engine] scene loaded ← %s (%zu entities)", path.c_str(),
                 world_.aliveCount());
    } else if (error != nullptr) {
        ANA_LOGE("[Engine] scene load failed: %s", error->c_str());
    }
    return ok;
}

void Engine::newScene() {
    std::lock_guard<std::mutex> lock(worldMutex_);
    runState_.store(RunState::Edit, std::memory_order_relaxed);
    restorePlayState();
    playBackup_.clear();
    world_.clear();
    demoSceneBuilt_ = true;      // File > New must not resurrect the demo scene
    ANA_LOGI("[Engine] new empty scene");
}

// ---------------------------------------------------------------------------
// Asset pipeline (Month 2) — decode on caller thread, GL upload queued.
// ---------------------------------------------------------------------------
std::uint32_t Engine::importTexture(const std::string& path) {
    // Runs on whatever thread the UI called us from (we dispatch to IO in
    // Kotlin); decode is pure CPU — no GL, no world lock needed here.
    std::string decodeError;
    std::optional<assets::Image> image =
        assets::ImageDecoder::decodeFile(path, &decodeError);
    if (!image.has_value()) {
        ANA_LOGE("[Engine] importTexture failed: %s", decodeError.c_str());
        return assets::kInvalidTextureHandle;
    }

    const assets::TextureHandle handle = assets_.createHandle(path);
    if (handle == assets::kInvalidTextureHandle) return handle;

    auto shared = std::make_shared<const assets::Image>(std::move(*image));
    assets_.cacheImage(handle, shared);

    // GL work deferred to the frame thread (documents the exact Phase-2
    // threading rule from ARCHITECTURE §3).
    renderCommands_.push([this, handle] { assets_.uploadFromCacheOnGl(handle); });

    ANA_LOGI("[Engine] imported '%s' → handle %u (%dx%d, upload queued)",
             path.c_str(), handle, shared->width, shared->height);
    return handle;
}

void Engine::setSpriteTexture(std::uint32_t entity,
                              std::uint32_t textureHandle) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* sprite = world_.tryGet<SpriteComponent>(entity)) {
        sprite->textureId = textureHandle;   // resolve() happens on GL thread
    }
}

std::uint32_t Engine::textureHandleForPath(const std::string& path) {
    // AssetManager has its own short-held mutex; the world lock is irrelevant
    // here (and taking it would serialize the UI against the render thread
    // for no reason).
    return assets_.handleForPath(path);
}

// ---------------------------------------------------------------------------
// Demo scene (first launch) — gives every panel something real to show.
// ---------------------------------------------------------------------------
void Engine::buildDemoSceneIfNeeded() {
    if (demoSceneBuilt_ || world_.aliveCount() > 0) return;
    demoSceneBuilt_ = true;

    struct Spec {
        const char* name;
        Vec3 position;
        Vec3 scale;
        Vec4 color;
        bool dynamic;          // rigidbody + collider
        bool staticBody;       // collider only
        float gravityScale;
    };
    static const Spec kSpecs[] = {
        // name          position            scale            color (RGBA)
        {"Ground",   {0.0f, -3.0f, 0.0f},  {10.0f, 0.6f, 1.0f}, {0.18f, 0.55f, 0.34f, 1.0f}, false, true, 0.0f},
        {"Platform", {3.0f, -1.0f, 0.0f},  {3.0f, 0.4f, 1.0f},  {0.22f, 0.44f, 0.86f, 1.0f}, false, true, 0.0f},
        {"Platform", {-3.4f, 0.6f, 0.0f},  {2.6f, 0.4f, 1.0f},  {0.22f, 0.44f, 0.86f, 1.0f}, false, true, 0.0f},
        {"Player",   {0.0f, 2.0f, 0.0f},   {0.9f, 0.9f, 1.0f},  {0.91f, 0.30f, 0.24f, 1.0f}, true,  false, 1.0f},
        {"Crate",    {3.0f, 0.4f, 0.0f},   {0.8f, 0.8f, 1.0f},  {0.85f, 0.65f, 0.20f, 1.0f}, true,  false, 1.0f},
        {"Coin",     {-3.4f, 2.0f, 0.0f},  {0.5f, 0.5f, 1.0f},  {0.96f, 0.85f, 0.25f, 1.0f}, true,  false, 0.45f},
        {"Coin",     {-2.4f, 1.6f, 0.0f},  {0.5f, 0.5f, 1.0f},  {0.96f, 0.85f, 0.25f, 1.0f}, true,  false, 0.45f},
        {"Particle", {1.6f, 2.6f, 0.0f},   {0.35f, 0.35f, 1.0f},{0.75f, 0.35f, 0.90f, 1.0f}, true,  false, 0.15f},
    };

    for (const Spec& s : kSpecs) {
        const ecs::Entity e = world_.create();
        TransformComponent tf;
        tf.position = s.position;
        tf.scale = s.scale;
        world_.add(e, TagComponent{s.name});
        world_.add(e, tf);
        world_.add(e, SpriteComponent{s.color, /*textureId=*/0, {1.0f, 1.0f}, 0});

        BoxColliderComponent col;
        // Collider half-extents follow the visual quad (unit quad × scale).
        col.halfExtents = {0.5f * s.scale.x, 0.5f * s.scale.y, 0.5f * s.scale.z};
        if (s.dynamic || s.staticBody) world_.add(e, col);

        if (s.dynamic) {
            RigidBodyComponent rb;
            rb.gravityScale = s.gravityScale;
            world_.add(e, rb);
        }
    }

    // Frame the action: default camera sits at (0,0,10) looking at origin with
    // half-height 5 — the demo fits comfortably.
    camera_.position = {0.0f, 0.0f, 10.0f};
    camera_.target = {0.0f, 0.0f, 0.0f};
    camera_.orthoHalfHeight = 5.0f;

    ANA_LOGI("[Engine] demo scene built (%zu entities)", world_.aliveCount());
}

}  // namespace anaconda
