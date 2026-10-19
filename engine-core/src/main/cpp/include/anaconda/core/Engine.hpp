// ===========================================================================
//  anaconda/core/Engine.hpp — top-level façade, Months 1-5.
//
//  Months 3-5 additions:
//    * Fixed-timestep physics (60 Hz accumulator, decoupled from render)
//    * Grid & gizmo state (editor snapping, translate/rotate/scale handles)
//    * Prefab save/load/instantiate
//    * Lua 5.4 ScriptEngine (per-scene, sandboxed, hot-reloadable)
//    * AnimatorSystem + ClipLibrary (sprite frame animation)
//    * Atlas texture packing (batching)
//    * DebugDraw (collider wireframes, grid overlay)
//    * Export config (target/package/version, validation)
//    * Enhanced profiling (physics ms, script ms, draw calls)
// ===========================================================================
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "anaconda/animation/AnimatorSystem.hpp"
#include "anaconda/assets/AssetManager.hpp"
#include "anaconda/core/RenderCommandQueue.hpp"
#include "anaconda/ecs/Components.hpp"
#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/editor/Gizmo.hpp"
#include "anaconda/editor/Grid.hpp"
#include "anaconda/export/ExportTypes.hpp"
#include "anaconda/math/Math.hpp"
#include "anaconda/physics/Broadphase.hpp"
#include "anaconda/physics/PhysicsSystem.hpp"
#include "anaconda/renderer/Atlas.hpp"
#include "anaconda/renderer/Camera.hpp"
#include "anaconda/renderer/DebugDraw.hpp"
#include "anaconda/renderer/Renderer.hpp"
#include "anaconda/script/ScriptEngine.hpp"

namespace anaconda {

enum class RunState : int { Edit = 0, Play = 1, Pause = 2 };
enum class EditorGesture : int { Pan = 0, Zoom = 1, Tap = 2 };

class Engine final {
public:
    static Engine& instance();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // ---- GL thread ---------------------------------------------------------
    void onSurfaceCreated();
    void onSurfaceChanged(int width, int height);
    void renderFrame();

    // ---- lifecycle ----------------------------------------------------------
    void onPause();
    void onResume();
    void shutdown();

    // ---- editor API (any thread, mutex protected) ---------------------------
    void setRunState(RunState state);
    [[nodiscard]] RunState runState() const;
    std::string sceneSnapshot();
    int createEntity(const std::string& name);
    void destroyEntity(std::uint32_t entity);
    void setEntityName(std::uint32_t entity, const std::string& name);
    void setTransform(std::uint32_t entity, const Vec3& position,
                      const Vec3& rotationRad, const Vec3& scale);
    void setSpriteColor(std::uint32_t entity, const Vec4& color);
    void setRigidBodyGravityScale(std::uint32_t entity, float scale);
    void setRigidBodyUseGravity(std::uint32_t entity, bool useGravity);
    int duplicateEntity(std::uint32_t entity);
    void setSpriteSortingOrder(std::uint32_t entity, int layer);
    void handleGesture(EditorGesture type, float dx, float dy, float x, float y);
    int pickEntity(float screenX, float screenY);
    [[nodiscard]] renderer::Renderer::FrameStats frameStats() const;

    // ---- Month 3: grid & gizmo ---------------------------------------------
    void setGridEnabled(bool enabled);
    void setGridSnapEnabled(bool snap);
    void setGridCellSize(float size);
    GridSettings gridSettings() const;
    void setGizmoMode(GizmoMode mode);
    GizmoMode gizmoMode() const;
    // Gizmo drag API (screen pixels)
    bool gizmoBeginDrag(float screenX, float screenY);
    void gizmoDrag(float screenX, float screenY);
    void gizmoEndDrag();
    // Frame selected entity (camera lerp)
    void frameSelected(std::uint32_t entity);

    // ---- Month 3: physics tuning --------------------------------------------
    void setPhysicsMaterial(std::uint32_t entity, float friction, float restitution);
    void setColliderDebugDraw(bool enabled);
    bool colliderDebugDraw() const;
    void setFixedTimestepEnabled(bool enabled);
    struct PhysicsStats {
        float lastStepMs = 0.0f;
        int broadphasePairs = 0;
        int contacts = 0;
    };
    PhysicsStats physicsStats() const;

    // ---- Month 3: prefab ----------------------------------------------------
    bool savePrefab(std::uint32_t entity, const std::string& path, std::string* error = nullptr);
    int instantiatePrefab(const std::string& path, const Vec3& offset = {0,0,0}, std::string* error = nullptr);

    // ---- Month 4: animation -------------------------------------------------
    bool createAnimationClip(const std::string& name, const std::vector<std::uint32_t>& frames,
                             float fps, bool loop, std::string* error = nullptr);
    void setAnimatorClip(std::uint32_t entity, const std::string& clipName);
    void setAnimatorPlaying(std::uint32_t entity, bool playing);
    void setAnimatorSpeed(std::uint32_t entity, float speed);

    // ---- Month 4: scripting -------------------------------------------------
    bool loadScript(const std::string& assetPath, const std::string& source, std::string* error = nullptr);
    bool bindScriptToEntity(std::uint32_t entity, const std::string& assetPath, std::string* error = nullptr);
    bool hotReloadScript(std::uint32_t entity, const std::string& newSource, std::string* error = nullptr);
    std::string lastScriptError() const;

    // ---- Month 4: camera effects --------------------------------------------
    void cameraShake(float amount, float duration);
    void cameraZoomTo(float targetHalfHeight, float duration);

    // ---- Month 5: export ----------------------------------------------------
    export_cfg::ExportResult exportProject(const export_cfg::ExportConfig& config);
    export_cfg::ProjectPackInfo packProject(const std::string& outputPath, std::string* error = nullptr);

    // ---- persistence --------------------------------------------------------
    bool saveScene(const std::string& path, std::string* error = nullptr);
    bool loadScene(const std::string& path, std::string* error = nullptr);
    void newScene();
    int restoreEntity(const std::string& snapshotLine, std::string* error = nullptr);

    // ---- assets -------------------------------------------------------------
    std::uint32_t importTexture(const std::string& path);
    void setSpriteTexture(std::uint32_t entity, std::uint32_t textureHandle);
    std::uint32_t textureHandleForPath(const std::string& path);
    renderer::Atlas& atlas() { return atlas_; }

    [[nodiscard]] float fps() const;
    [[nodiscard]] int entityCount();
    static const char* version();

private:
    Engine();
    void buildDemoSceneIfNeeded();
    void backupPlayState();
    void restorePlayState();

    mutable std::mutex worldMutex_;
    ecs::Registry world_;
    PhysicsSystem physics_;
    BroadphaseGrid broadphase_{2.0f};
    renderer::Renderer renderer_;
    renderer::DebugDraw debugDraw_;
    renderer::Atlas atlas_{2048};
    animation::AnimatorSystem animator_;
    script::ScriptEngine scriptEngine_;
    Camera camera_;

    assets::AssetManager assets_;
    RenderCommandQueue renderCommands_;

    std::atomic<RunState> runState_{RunState::Edit};
    std::atomic<float> fps_{0.0f};
    std::atomic<int> viewportWidth_{0};
    std::atomic<int> viewportHeight_{0};
    std::atomic<bool> paused_{false};

    // Fixed timestep
    bool fixedTimestepEnabled_ = true;
    float accumulator_ = 0.0f;
    static constexpr float kFixedDt = 1.0f / 60.0f;
    PhysicsStats physicsStats_{};

    // Grid & gizmo
    GridSettings grid_{};
    GizmoState gizmo_{};
    bool colliderDebugDraw_ = false;

    // Camera effects
    float shakeAmount_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float zoomLerpTarget_ = 5.0f;
    float zoomLerpDuration_ = 0.0f;
    float zoomLerpTime_ = 0.0f;

    struct PlayBackup {
        Vec3 position, rotation, scale;
        Vec3 velocity;
        float gravityScale = 1.0f;
        bool useGravity = true;
    };
    std::unordered_map<std::uint32_t, PlayBackup> playBackup_;
    std::chrono::steady_clock::time_point lastFrameTime_{};
    bool demoSceneBuilt_ = false;
};

}  // namespace anaconda
