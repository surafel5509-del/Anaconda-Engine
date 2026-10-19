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

#include "anaconda/assets/AssetManager.hpp"
#include "anaconda/core/RenderCommandQueue.hpp"
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

    /// Month 3: duplicate an entity (deep copy of all components). New name
    /// gets \" Copy\" suffix. Returns new id or -1 if source is stale.
    int duplicateEntity(std::uint32_t entity);
    /// Month 3: sprite draw order (lower = behind). No-op if entity has no sprite.
    void setSpriteSortingOrder(std::uint32_t entity, int layer);

    void handleGesture(EditorGesture type, float dx, float dy, float x, float y);

    /// Viewport picking: screen point (pixels, origin top-left) → topmost
    /// entity whose AABB contains the world point, or -1 when none.
    int pickEntity(float screenX, float screenY);

    [[nodiscard]] renderer::Renderer::FrameStats frameStats() const;

    // ---- persistence (Month 2 — docs/ASSET_PIPELINE_AND_EXPORT.md) --------
    /// Serialize the whole world to `path`. false + `error` on failure.
    bool saveScene(const std::string& path, std::string* error = nullptr);
    /// Replace the world with `path`'s contents. All-or-nothing: failure
    /// leaves an EMPTY world (never a half-loaded scene).
    bool loadScene(const std::string& path, std::string* error = nullptr);
    /// File > New: stop play mode, drop everything, empty world.
    void newScene();

    /// Rebuild an entity from an editor snapshot line (undo-of-delete path).
    /// Returns the NEW entity id, or -1 when the line is malformed.
    int restoreEntity(const std::string& snapshotLine, std::string* error = nullptr);

    // ---- asset pipeline (Month 2) -----------------------------------------
    /// Decode `path` (RGBA via stb_image) on the CALLER's thread and queue
    /// the GL upload for the next frame. Returns a stable TextureHandle
    /// immediately (0 = decode failed). Safe from any thread.
    std::uint32_t importTexture(const std::string& path);
    /// Point a sprite at an imported asset handle (0 → white default).
    void setSpriteTexture(std::uint32_t entity, std::uint32_t textureHandle);

    /// Asset browser: handle previously minted for a file path (0 = never
    /// imported in this session). Queries the AssetManager only — no world
    /// lock required.
    std::uint32_t textureHandleForPath(const std::string& path);

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

    // Month-2 asset pipeline: file → handle → (queued) GL upload. See
    // AssetManager.hpp for the indirection rationale.
    assets::AssetManager assets_;
    RenderCommandQueue renderCommands_;

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
