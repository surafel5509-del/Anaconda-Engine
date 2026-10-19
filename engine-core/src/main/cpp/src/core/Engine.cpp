// ===========================================================================
//  core/Engine.cpp — frame loop, run states, editor API, demo scene.
//  Months 3-5: fixed-timestep physics, grid/gizmo, prefab, atlas, debug
//  draw, Lua scripting, animator, camera effects, export stubs.
// ===========================================================================
#include "anaconda/core/Engine.hpp"

#include "anaconda/assets/ImageDecoder.hpp"
#include "anaconda/core/Log.hpp"
#include "anaconda/ecs/Components.hpp"
#include "anaconda/editor/Gizmo.hpp"
#include "anaconda/editor/Grid.hpp"
#include "anaconda/scene/EditorSnapshotCodec.hpp"
#include "anaconda/scene/Picking.hpp"
#include "anaconda/scene/Prefab.hpp"
#include "anaconda/scene/SceneSerializer.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <memory>
#include <sstream>

#ifndef ANACONDA_VERSION
#define ANACONDA_VERSION "0.1.0-dev"
#endif

namespace anaconda {

namespace {
constexpr float kMaxFrameDelta = 0.25f;
constexpr float kTargetFpsEmaAlpha = 0.1f;
} // namespace

Engine& Engine::instance() { static Engine engine; return engine; }
Engine::Engine() {
    ANA_LOGI("[Engine] Anaconda-Engine %s constructing (C++%ld)", version(), static_cast<long>(__cplusplus));
    scriptEngine_.initialize();
}
const char* Engine::version() { return ANACONDA_VERSION; }

// ---------------------------------------------------------------------------
// GL thread callbacks
// ---------------------------------------------------------------------------
void Engine::onSurfaceCreated() {
    std::lock_guard<std::mutex> lock(worldMutex_);
    renderer_.onSurfaceCreated();
    assets_.reuploadAllOnGl();
    buildDemoSceneIfNeeded();
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
// Frame loop
// ---------------------------------------------------------------------------
void Engine::renderFrame() {
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;
    if (dt <= 0.0f) dt = 1.0f / 60.0f;
    if (dt > kMaxFrameDelta) dt = kMaxFrameDelta;

    const float instantFps = 1.0f / dt;
    const float smoothed = fps_.load(std::memory_order_relaxed) * (1.0f - kTargetFpsEmaAlpha) + instantFps * kTargetFpsEmaAlpha;
    fps_.store(smoothed, std::memory_order_relaxed);

    // Camera shake update
    if (shakeDuration_ > 0.0f) {
        shakeDuration_ -= dt;
        if (shakeDuration_ <= 0.0f) { shakeAmount_ = 0.0f; shakeDuration_ = 0.0f; }
    }
    if (zoomLerpDuration_ > 0.0f) {
        zoomLerpTime_ += dt;
        float t = std::clamp(zoomLerpTime_ / zoomLerpDuration_, 0.0f, 1.0f);
        camera_.orthoHalfHeight = camera_.orthoHalfHeight * (1.0f - t) + zoomLerpTarget_ * t;
        if (t >= 1.0f) zoomLerpDuration_ = 0.0f;
    }

    std::vector<RenderCommandQueue::Command> glCommands = renderCommands_.drain();
    std::lock_guard<std::mutex> lock(worldMutex_);
    for (auto& c : glCommands) if (c) c();

    if (runState_.load(std::memory_order_relaxed) == RunState::Play) {
        auto t0 = std::chrono::steady_clock::now();
        if (fixedTimestepEnabled_) {
            accumulator_ += dt;
            while (accumulator_ >= kFixedDt) {
                scriptEngine_.update(world_, kFixedDt);
                animator_.update(world_, kFixedDt);
                physics_.step(world_, kFixedDt);
                accumulator_ -= kFixedDt;
            }
        } else {
            scriptEngine_.update(world_, dt);
            animator_.update(world_, dt);
            physics_.step(world_, dt);
        }
        auto t1 = std::chrono::steady_clock::now();
        physicsStats_.lastStepMs = std::chrono::duration<float, std::milli>(t1 - t0).count();
        physicsStats_.contacts = physics_.stats().contacts;
        physicsStats_.broadphasePairs = 0; // filled if broadphase enabled
    } else {
        // Even in Edit, animator can preview
        animator_.update(world_, dt);
    }

    // Shake offset (applied to view, restored after)
    Vec3 shakeOffset{0,0,0};
    if (shakeAmount_ > 0.0f) {
        shakeOffset.x = (std::rand() % 1000 / 1000.0f - 0.5f) * shakeAmount_;
        shakeOffset.y = (std::rand() % 1000 / 1000.0f - 0.5f) * shakeAmount_;
    }
    Vec3 savedPos = camera_.position, savedTarget = camera_.target;
    camera_.position = camera_.position + shakeOffset;
    camera_.target = camera_.target + shakeOffset;

    renderer_.beginFrame(Vec4{0.055f, 0.063f, 0.082f, 1.0f});
    renderer_.renderScene(world_, camera_, assets_);
    if (colliderDebugDraw_) {
        debugDraw_.clear();
        debugDraw_.collectColliders(world_, Vec4{0,1,0,0.9f});
        debugDraw_.collectGrid(grid_, 20.0f, Vec4{0.3f,0.3f,0.3f,0.2f});
        // renderer would draw debug lines if we had a debug shader; for now we just track stats
    }
    renderer_.endFrame();

    camera_.position = savedPos;
    camera_.target = savedTarget;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------
void Engine::onPause() { paused_.store(true, std::memory_order_relaxed); ANA_LOGI("[Engine] onPause"); }
void Engine::onResume() { paused_.store(false, std::memory_order_relaxed); lastFrameTime_ = std::chrono::steady_clock::now(); ANA_LOGI("[Engine] onResume"); }
void Engine::shutdown() { std::lock_guard<std::mutex> lock(worldMutex_); renderer_.shutdown(); scriptEngine_.shutdown(); ANA_LOGI("[Engine] shutdown"); }

// ---------------------------------------------------------------------------
// Run state
// ---------------------------------------------------------------------------
void Engine::setRunState(RunState state) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    RunState prev = runState_.load(std::memory_order_relaxed);
    if (prev == state) return;
    if (state == RunState::Play) {
        if (prev == RunState::Edit) backupPlayState();
        physics_.step(world_, 0.0f);
        accumulator_ = 0.0f;
    } else if (state == RunState::Edit) restorePlayState();
    runState_.store(state, std::memory_order_relaxed);
    ANA_LOGI("[Engine] run state %d -> %d", static_cast<int>(prev), static_cast<int>(state));
}
RunState Engine::runState() const { return runState_.load(std::memory_order_relaxed); }
void Engine::backupPlayState() {
    playBackup_.clear();
    for (auto e : world_.aliveEntities()) {
        auto* tf = world_.tryGet<TransformComponent>(e);
        if (!tf) continue;
        PlayBackup b; b.position = tf->position; b.rotation = tf->rotation; b.scale = tf->scale;
        if (auto* rb = world_.tryGet<RigidBodyComponent>(e)) { b.velocity = rb->velocity; b.gravityScale = rb->gravityScale; b.useGravity = rb->useGravity; }
        playBackup_[static_cast<uint32_t>(e)] = b;
    }
}
void Engine::restorePlayState() {
    for (auto& [raw, b] : playBackup_) {
        ecs::Entity e = static_cast<ecs::Entity>(raw);
        if (!world_.valid(e)) continue;
        if (auto* tf = world_.tryGet<TransformComponent>(e)) { tf->position = b.position; tf->rotation = b.rotation; tf->scale = b.scale; }
        if (auto* rb = world_.tryGet<RigidBodyComponent>(e)) { rb->velocity = b.velocity; rb->gravityScale = b.gravityScale; rb->useGravity = b.useGravity; rb->grounded = false; }
    }
    playBackup_.clear();
}

// ---------------------------------------------------------------------------
// Scene snapshot
// ---------------------------------------------------------------------------
std::string Engine::sceneSnapshot() {
    std::lock_guard<std::mutex> lock(worldMutex_);
    std::string out;
    for (auto e : world_.aliveEntities()) {
        std::string line = scene::EditorSnapshotCodec::writeEntity(world_, e);
        if (!line.empty()) out += line + '\n';
    }
    return out;
}
int Engine::restoreEntity(const std::string& line, std::string* error) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    ecs::Entity e = scene::EditorSnapshotCodec::parseEntity(world_, line, error);
    if (e == ecs::kNullEntity) return -1;
    demoSceneBuilt_ = true;
    return static_cast<int>(e);
}

// ---------------------------------------------------------------------------
// Entity API
// ---------------------------------------------------------------------------
int Engine::createEntity(const std::string& name) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    ecs::Entity e = world_.create();
    world_.add(e, TagComponent{name.empty() ? "Entity" : name});
    world_.add(e, TransformComponent{});
    return static_cast<int>(e);
}
void Engine::destroyEntity(std::uint32_t entity) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    playBackup_.erase(entity);
    world_.destroy(entity);
}
void Engine::setEntityName(std::uint32_t entity, const std::string& name) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* tag = world_.tryGet<TagComponent>(entity)) tag->name = name;
}
void Engine::setTransform(std::uint32_t entity, const Vec3& pos, const Vec3& rot, const Vec3& scale) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* tf = world_.tryGet<TransformComponent>(entity)) {
        Vec3 p = pos;
        if (grid_.snapEnabled) p = snapToGrid(p, grid_);
        tf->position = p; tf->rotation = rot; tf->scale = scale;
    }
}
void Engine::setSpriteColor(std::uint32_t entity, const Vec4& color) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* sp = world_.tryGet<SpriteComponent>(entity)) sp->color = color;
}
void Engine::setRigidBodyGravityScale(std::uint32_t entity, float s) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* rb = world_.tryGet<RigidBodyComponent>(entity)) rb->gravityScale = s;
}
void Engine::setRigidBodyUseGravity(std::uint32_t entity, bool v) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* rb = world_.tryGet<RigidBodyComponent>(entity)) rb->useGravity = v;
}
int Engine::duplicateEntity(std::uint32_t entity) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return -1;
    ecs::Entity src = static_cast<ecs::Entity>(entity);
    ecs::Entity dst = scene::duplicateEntity(world_, src);
    return dst == ecs::kNullEntity ? -1 : static_cast<int>(dst);
}
void Engine::setSpriteSortingOrder(std::uint32_t entity, int layer) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    if (auto* sp = world_.tryGet<SpriteComponent>(entity)) sp->renderLayer = layer;
}
void Engine::handleGesture(EditorGesture type, float dx, float dy, float x, float y) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    int vh = viewportHeight_.load(std::memory_order_relaxed);
    switch (type) {
        case EditorGesture::Pan: camera_.panPixels(dx, dy, vh); break;
        case EditorGesture::Zoom: camera_.zoomBy(dx); break;
        case EditorGesture::Tap: ANA_LOGD("[Engine] tap @ (%.0f, %.0f)", x, y); (void)x; (void)y; break;
    }
}
int Engine::pickEntity(float sx, float sy) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    int vw = viewportWidth_.load(std::memory_order_relaxed);
    int vh = viewportHeight_.load(std::memory_order_relaxed);
    ecs::Entity e = scene::pickEntityAt(world_, camera_, vw, vh, sx, sy);
    return e == ecs::kNullEntity ? -1 : static_cast<int>(e);
}
renderer::Renderer::FrameStats Engine::frameStats() const { std::lock_guard<std::mutex> lock(worldMutex_); return renderer_.stats(); }
float Engine::fps() const { return fps_.load(std::memory_order_relaxed); }
int Engine::entityCount() { std::lock_guard<std::mutex> lock(worldMutex_); return static_cast<int>(world_.aliveCount()); }

// ---------------------------------------------------------------------------
// Month 3: grid & gizmo
// ---------------------------------------------------------------------------
void Engine::setGridEnabled(bool e) { std::lock_guard<std::mutex> lock(worldMutex_); grid_.enabled = e; }
void Engine::setGridSnapEnabled(bool s) { std::lock_guard<std::mutex> lock(worldMutex_); grid_.snapEnabled = s; }
void Engine::setGridCellSize(float sz) { std::lock_guard<std::mutex> lock(worldMutex_); grid_.cellSize = sz > 0.01f ? sz : 1.0f; }
GridSettings Engine::gridSettings() const { std::lock_guard<std::mutex> lock(worldMutex_); return grid_; }
void Engine::setGizmoMode(GizmoMode m) { std::lock_guard<std::mutex> lock(worldMutex_); gizmo_.mode = m; }
GizmoMode Engine::gizmoMode() const { std::lock_guard<std::mutex> lock(worldMutex_); return gizmo_.mode; }
bool Engine::gizmoBeginDrag(float sx, float sy) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (gizmo_.target == ecs::kNullEntity) return false;
    auto* tf = world_.tryGet<TransformComponent>(gizmo_.target);
    if (!tf) return false;
    int vw = viewportWidth_.load(std::memory_order_relaxed);
    int vh = viewportHeight_.load(std::memory_order_relaxed);
    GizmoHandle h = hitTestGizmo({sx, sy}, tf->position, camera_, vw, vh, gizmo_.mode);
    if (h == GizmoHandle::None) return false;
    gizmo_.active = h; gizmo_.dragging = true; gizmo_.dragStartScreen = {sx, sy}; gizmo_.dragStartWorld = tf->position; gizmo_.originalPosition = tf->position;
    return true;
}
void Engine::gizmoDrag(float sx, float sy) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!gizmo_.dragging || gizmo_.target == ecs::kNullEntity) return;
    auto* tf = world_.tryGet<TransformComponent>(gizmo_.target);
    if (!tf) return;
    int vh = viewportHeight_.load(std::memory_order_relaxed);
    tf->position = applyGizmoDrag(gizmo_, {sx, sy}, camera_, vh, grid_);
}
void Engine::gizmoEndDrag() { std::lock_guard<std::mutex> lock(worldMutex_); gizmo_.dragging = false; gizmo_.active = GizmoHandle::None; }
void Engine::frameSelected(std::uint32_t entity) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(entity)) return;
    auto* tf = world_.tryGet<TransformComponent>(entity);
    if (!tf) return;
    camera_.frameRegion(tf->position, std::max({tf->scale.x, tf->scale.y, 1.0f}));
}

// ---------------------------------------------------------------------------
// Physics tuning
// ---------------------------------------------------------------------------
void Engine::setPhysicsMaterial(std::uint32_t e, float f, float r) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(e)) return;
    if (auto* rb = world_.tryGet<RigidBodyComponent>(e)) { rb->friction = f; rb->restitution = r; }
    if (auto* col = world_.tryGet<BoxColliderComponent>(e)) { col->friction = f; col->restitution = r; }
}
void Engine::setColliderDebugDraw(bool v) { std::lock_guard<std::mutex> lock(worldMutex_); colliderDebugDraw_ = v; }
bool Engine::colliderDebugDraw() const { std::lock_guard<std::mutex> lock(worldMutex_); return colliderDebugDraw_; }
void Engine::setFixedTimestepEnabled(bool v) { std::lock_guard<std::mutex> lock(worldMutex_); fixedTimestepEnabled_ = v; }
Engine::PhysicsStats Engine::physicsStats() const { std::lock_guard<std::mutex> lock(worldMutex_); return physicsStats_; }

// ---------------------------------------------------------------------------
// Prefab
// ---------------------------------------------------------------------------
bool Engine::savePrefab(std::uint32_t e, const std::string& path, std::string* err) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(e)) { if (err) *err = "invalid entity"; return false; }
    return scene::Prefab::saveToFile(world_, static_cast<ecs::Entity>(e), path, err);
}
int Engine::instantiatePrefab(const std::string& path, const Vec3& off, std::string* err) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    ecs::Entity e = scene::Prefab::instantiateFromFile(world_, path, off, err);
    return e == ecs::kNullEntity ? -1 : static_cast<int>(e);
}

// ---------------------------------------------------------------------------
// Animation
// ---------------------------------------------------------------------------
bool Engine::createAnimationClip(const std::string& name, const std::vector<std::uint32_t>& frames, float fps, bool loop, std::string* err) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (name.empty() || frames.empty()) { if (err) *err = "invalid clip"; return false; }
    animator_.createSimpleClip(name, frames, fps, loop);
    return true;
}
void Engine::setAnimatorClip(std::uint32_t e, const std::string& clip) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(e)) return;
    if (auto* ac = world_.tryGet<SpriteAnimatorComponent>(static_cast<ecs::Entity>(e))) ac->currentClip = clip;
    else { SpriteAnimatorComponent nac; nac.currentClip = clip; world_.add(static_cast<ecs::Entity>(e), std::move(nac)); }
}
void Engine::setAnimatorPlaying(std::uint32_t e, bool p) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (auto* ac = world_.tryGet<SpriteAnimatorComponent>(static_cast<ecs::Entity>(e))) ac->playing = p;
}
void Engine::setAnimatorSpeed(std::uint32_t e, float s) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (auto* ac = world_.tryGet<SpriteAnimatorComponent>(static_cast<ecs::Entity>(e))) ac->speed = s;
}

// ---------------------------------------------------------------------------
// Scripting
// ---------------------------------------------------------------------------
bool Engine::loadScript(const std::string& path, const std::string& src, std::string* err) { return scriptEngine_.loadFile(path, src, err); }
bool Engine::bindScriptToEntity(std::uint32_t e, const std::string& path, std::string* err) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(e)) { if (err) *err = "invalid entity"; return false; }
    auto* sc = world_.tryGet<ScriptComponent>(static_cast<ecs::Entity>(e));
    if (!sc) { ScriptComponent n; n.assetPath = path; world_.add(static_cast<ecs::Entity>(e), std::move(n)); sc = world_.tryGet<ScriptComponent>(static_cast<ecs::Entity>(e)); }
    else sc->assetPath = path;
    if (!scriptEngine_.isInitialized()) scriptEngine_.initialize();
    return scriptEngine_.bindEntity(static_cast<ecs::Entity>(e), path, err);
}
bool Engine::hotReloadScript(std::uint32_t e, const std::string& src, std::string* err) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(e)) { if (err) *err = "invalid entity"; return false; }
    return scriptEngine_.hotReload(static_cast<ecs::Entity>(e), src, err);
}
std::string Engine::lastScriptError() const { return scriptEngine_.lastError().message; }

// ---------------------------------------------------------------------------
// Camera effects
// ---------------------------------------------------------------------------
void Engine::cameraShake(float a, float d) { std::lock_guard<std::mutex> lock(worldMutex_); shakeAmount_ = a; shakeDuration_ = d; }
void Engine::cameraZoomTo(float target, float duration) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    zoomLerpTarget_ = target; zoomLerpDuration_ = duration > 0.01f ? duration : 0.5f; zoomLerpTime_ = 0;
}

// ---------------------------------------------------------------------------
// Export
// ---------------------------------------------------------------------------
export_cfg::ExportResult Engine::exportProject(const export_cfg::ExportConfig& cfg) {
    export_cfg::ExportResult r;
    if (!export_cfg::isValidPackageName(cfg.packageName)) { r.error = "invalid package name"; return r; }
    r.success = true; r.packageName = cfg.packageName; r.outputPath = "/tmp/" + cfg.packageName + ".apk"; r.outputBytes = 1024;
    ANA_LOGI("[Export] exportProject %s -> %s (target %d)", cfg.packageName.c_str(), r.outputPath.c_str(), static_cast<int>(cfg.target));
    return r;
}
export_cfg::ProjectPackInfo Engine::packProject(const std::string& outPath, std::string* err) {
    export_cfg::ProjectPackInfo info; info.packPath = outPath; info.projectName = "AnacondaProject";
    ANA_LOGI("[Export] packProject -> %s", outPath.c_str());
    (void)err; return info;
}

// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------
bool Engine::saveScene(const std::string& path, std::string* err) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    bool ok = scene::SceneSerializer::saveToFile(world_, path, err);
    if (ok) ANA_LOGI("[Engine] saved %s (%zu)", path.c_str(), world_.aliveCount());
    return ok;
}
bool Engine::loadScene(const std::string& path, std::string* err) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    runState_.store(RunState::Edit, std::memory_order_relaxed); restorePlayState(); playBackup_.clear();
    bool ok = scene::SceneSerializer::loadFromFile(world_, path, err);
    if (ok) { demoSceneBuilt_ = true; ANA_LOGI("[Engine] loaded %s (%zu)", path.c_str(), world_.aliveCount()); }
    return ok;
}
void Engine::newScene() {
    std::lock_guard<std::mutex> lock(worldMutex_);
    runState_.store(RunState::Edit, std::memory_order_relaxed); restorePlayState(); playBackup_.clear(); world_.clear(); demoSceneBuilt_ = true;
}
std::uint32_t Engine::importTexture(const std::string& path) {
    std::string err; auto img = assets::ImageDecoder::decodeFile(path, &err);
    if (!img) { ANA_LOGE("[Engine] importTexture failed: %s", err.c_str()); return assets::kInvalidTextureHandle; }
    auto handle = assets_.createHandle(path);
    if (handle == assets::kInvalidTextureHandle) return handle;
    auto shared = std::make_shared<const assets::Image>(std::move(*img));
    assets_.cacheImage(handle, shared);
    renderCommands_.push([this, handle]{ assets_.uploadFromCacheOnGl(handle); });
    return handle;
}
void Engine::setSpriteTexture(std::uint32_t e, std::uint32_t h) {
    std::lock_guard<std::mutex> lock(worldMutex_);
    if (!world_.valid(e)) return;
    if (auto* sp = world_.tryGet<SpriteComponent>(e)) sp->textureId = h;
}
std::uint32_t Engine::textureHandleForPath(const std::string& p) { return assets_.handleForPath(p); }

// ---------------------------------------------------------------------------
// Demo scene
// ---------------------------------------------------------------------------
void Engine::buildDemoSceneIfNeeded() {
    if (demoSceneBuilt_ || world_.aliveCount() > 0) return;
    demoSceneBuilt_ = true;
    struct Spec { const char* name; Vec3 pos; Vec3 scale; Vec4 color; bool dyn; bool stat; float gs; };
    const Spec specs[] = {
        {"Ground", {0,-3,0}, {10,0.6f,1}, {0.18f,0.55f,0.34f,1}, false, true, 0},
        {"Platform", {3,-1,0}, {3,0.4f,1}, {0.22f,0.44f,0.86f,1}, false, true, 0},
        {"Platform", {-3.4f,0.6f,0}, {2.6f,0.4f,1}, {0.22f,0.44f,0.86f,1}, false, true, 0},
        {"Player", {0,2,0}, {0.9f,0.9f,1}, {0.91f,0.30f,0.24f,1}, true, false, 1},
        {"Crate", {3,0.4f,0}, {0.8f,0.8f,1}, {0.85f,0.65f,0.20f,1}, true, false, 1},
        {"Coin", {-3.4f,2,0}, {0.5f,0.5f,1}, {0.96f,0.85f,0.25f,1}, true, false, 0.45f},
        {"Coin", {-2.4f,1.6f,0}, {0.5f,0.5f,1}, {0.96f,0.85f,0.25f,1}, true, false, 0.45f},
        {"Particle", {1.6f,2.6f,0}, {0.35f,0.35f,1}, {0.75f,0.35f,0.90f,1}, true, false, 0.15f},
    };
    for (auto& s : specs) {
        ecs::Entity e = world_.create();
        TransformComponent tf; tf.position = s.pos; tf.scale = s.scale; world_.add(e, TagComponent{s.name}); world_.add(e, tf);
        world_.add(e, SpriteComponent{s.color,0,{1,1},0});
        BoxColliderComponent col; col.halfExtents = {0.5f*s.scale.x,0.5f*s.scale.y,0.5f*s.scale.z}; if (s.dyn||s.stat) world_.add(e,col);
        if (s.dyn) { RigidBodyComponent rb; rb.gravityScale = s.gs; world_.add(e,rb); }
    }
    camera_.position = {0,0,10}; camera_.target = {0,0,0}; camera_.orthoHalfHeight = 5.0f;
    ANA_LOGI("[Engine] demo built (%zu)", world_.aliveCount());
}

} // namespace anaconda
