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

#include "anaconda/core/Log.hpp"
#include "anaconda/ecs/Components.hpp"

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

    // ---- 2) simulate + render under the world lock -------------------------
    std::lock_guard<std::mutex> lock(worldMutex_);

    if (runState_.load(std::memory_order_relaxed) == RunState::Play) {
        physics_.step(world_, dt);
    }

    renderer_.beginFrame(Vec4{0.055f, 0.063f, 0.082f, 1.0f} /* editor bg */);
    renderer_.renderScene(world_, camera_);
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
    std::ostringstream out;
    out.precision(4);
    out << std::fixed;

    for (const ecs::Entity e : world_.aliveEntities()) {
        if (!world_.valid(e)) continue;

        const auto* tf = world_.tryGet<TransformComponent>(e);
        if (tf == nullptr) continue;
        const auto* tag = world_.tryGet<TagComponent>(e);
        const auto* sprite = world_.tryGet<SpriteComponent>(e);
        const auto* rb = world_.tryGet<RigidBodyComponent>(e);
        const auto* col = world_.tryGet<BoxColliderComponent>(e);

        int flags = 0;
        if (sprite != nullptr) flags |= 1;
        if (rb != nullptr) flags |= 2;
        if (col != nullptr) flags |= 4;

        // Sanitize the delimiter out of names so the protocol can't be broken
        // by a user typing '|' in the inspector.
        std::string name = tag != nullptr ? tag->name : "Entity";
        for (char& c : name) {
            if (c == '|' || c == '\n' || c == '\r') c = '/';
        }

        out << static_cast<std::uint32_t>(e) << '|' << name << '|'
            << tf->position.x << ',' << tf->position.y << ',' << tf->position.z
            << '|'
            << tf->rotation.x << ',' << tf->rotation.y << ',' << tf->rotation.z
            << '|'
            << tf->scale.x << ',' << tf->scale.y << ',' << tf->scale.z << '|'
            << flags << '|';
        if (sprite != nullptr) {
            out << sprite->color.x << ',' << sprite->color.y << ','
                << sprite->color.z << ',' << sprite->color.w;
        } else {
            out << "1,1,1,1";
        }
        out << '\n';
    }
    return out.str();
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
