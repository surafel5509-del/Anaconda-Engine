// ============================================================================
//  anaconda/core/Engine.hpp
//
//  Top-level engine façade — the object behind every JNI call.
//
//  Lifecycle (mirrors GLSurfaceView.Renderer on the Kotlin side):
//      onSurfaceCreated()   GL thread — (re)build all GL resources
//      onSurfaceChanged()   GL thread — viewport/camera aspect
//      renderFrame()        GL thread — dt → physics → render, every vsync
//      onPause/onResume     UI thread — logging + state only (no GL)
//      shutdown()           GL thread — release everything
//
//  Editor API (sceneSnapshot, setTransform, createEntity, ...) is callable
//  from ANY thread: all world reads/writes are serialized by worldMutex_.
//  The render loop takes the same mutex for the whole physics+render section,
//  so the UI always observes fully consistent frames (no torn transforms).
//
//  RunState::Play snapshots every Transform + velocity first; RunState::Edit
//  restores it — the classic "Stop button puts the scene back" editor UX.
// ============================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "anaconda/ecs/Components.hpp"
#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/math/Math.hpp"
#include "anaconda/physics/PhysicsSystem.hpp"
#include "anaconda/renderer/Camera.hpp"
#include "anaconda/renderer/Renderer.hpp"

namespace anaconda {

/// Editor run modes. Values are part of the JNI contract (do not reorder).
enum class RunState : int {
    Edit = 0,   // authoring; physics frozen
    Play = 1,   // simulation running
    Pause = 2,  // simulation frozen mid-play (transforms kept)
};

/// Semantic gestures forwarded from Kotlin (docs/ARCHITECTURE.md §5.5).
enum class EditorGesture : int {
    Pan = 0,    // dx/dy in viewport pixels
    Zoom = 1,   // dx = scale factor (>1 zooms in)
    Tap = 2,    // x/y in viewport pixels (reserved for Phase-3 picking)
};

class Engine final {
public:
    /// Function-local singleton: constructed on first JNI call, destroyed at
    /// process teardown. No static-init-order fiasco.
    static Engine& instance();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // ---- GL thread ---------------------------------------------------------
    void onSurfaceCreated();
    void onSurfaceChanged(int width, int height);
    void renderFrame();

    // ---- lifecycle (UI thread) --------------------------------------------
    void onPause();
    void onResume();
    void shutdown();

    // ---- editor API (any thread, world mutex protected) -------------------
    void setRunState(RunState state);
    [[nodiscard]] RunState runState() const;

    /// Serializes all live entities for the editor hierarchy/inspector:
    /// one line each:  id|name|px,py,pz|rx,ry,rz|sx,sy,sz|flags|cr,cg,cb,ca
    /// flags: bit0 sprite, bit1 rigidbody, bit2 box collider.
    std::string sceneSnapshot();

    int createEntity(const std::string& name);
    void destroyEntity(std::uint32_t entity);
    void setEntityName(std::uint32_t entity, const std::string& name);
    void setTransform(std::uint32_t entity, const Vec3& position,
                      const Vec3& rotationRad, const Vec3& scale);
    void setSpriteColor(std::uint32_t entity, const Vec4& color);
    void setRigidBodyGravityScale(std::uint32_t entity, float scale);
    void setRigidBodyUseGravity(std::uint32_t entity, bool useGravity);

    void handleGesture(EditorGesture type, float dx, float dy, float x, float y);

    [[nodiscard]] float fps() const;
    [[nodiscard]] int entityCount();

    static const char* version();

private:
    Engine();

    // All helpers below require worldMutex_ held by the caller.
    void buildDemoSceneIfNeeded();
    void backupPlayState();
    void restorePlayState();

    mutable std::mutex worldMutex_;
    ecs::Registry world_;
    PhysicsSystem physics_;
    renderer::Renderer renderer_;
    Camera camera_;

    std::atomic<RunState> runState_{RunState::Edit};
    std::atomic<float> fps_{0.0f};
    std::atomic<int> viewportWidth_{0};
    std::atomic<int> viewportHeight_{0};
    std::atomic<bool> paused_{false};

    /// Play-mode rollback table (entity → pre-play transform + velocity).
    struct PlayBackup {
        Vec3 position, rotation, scale;
        Vec3 velocity;
        float gravityScale = 1.0f;
        bool useGravity = true;
    };
    std::unordered_map<std::uint32_t, PlayBackup> playBackup_;

    std::chrono::steady_clock::time_point lastFrameTime_{};
    bool demoSceneBuilt_ = false;   // worldMutex_ guarded
};

}  // namespace anaconda
