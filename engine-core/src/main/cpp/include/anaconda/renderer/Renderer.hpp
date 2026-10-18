// ============================================================================
//  anaconda/renderer/Renderer.hpp
//
//  The GLES3 pipeline façade — the ONLY place (together with Shader/Mesh/
//  Texture) that issues GL calls, so a future Vulkan backend replaces exactly
//  this subsystem behind the same API.
//
//  Responsibilities:
//    * Own GL state policy: blending ON (sprite alpha), depth OFF in Phase-1
//      2D mode (painter's algorithm by hierarchy order; depth sorting lands
//      with 3D in Month 4+).
//    * Own the built-in sprite program, the unit quad mesh, and the white
//      default texture.
//    * renderScene(): iterate View<Transform, Sprite>, build per-entity model
//      matrices, draw. Per-entity uniform upload is fine for Phase 1;
//      atlas instanced batching is Month 4 (docs/ROADMAP.md).
//    * Frame statistics surfaced to the editor toolbar (draw calls, sprites).
//
//  THREADING: every method must run on the GL thread with a current context.
// ============================================================================
#pragma once

#include "anaconda/ecs/Components.hpp"
#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/math/Math.hpp"
#include "anaconda/renderer/Camera.hpp"
#include "anaconda/renderer/Mesh.hpp"
#include "anaconda/renderer/Shader.hpp"
#include "anaconda/renderer/Texture.hpp"

namespace anaconda::renderer {

class Renderer {
public:
    struct FrameStats {
        int drawCalls = 0;
        int sprites = 0;
        int width = 0;
        int height = 0;
    };

    /// Compile shaders, create meshes/textures. Safe to call again — the
    /// previous GL context's objects are released first (surface recreation).
    /// Returns false if compilation failed (viewport shows clear color only).
    bool onSurfaceCreated();

    void onSurfaceChanged(int width, int height);

    /// Clear + prepare per-frame state.
    void beginFrame(const Vec4& clearColor);

    /// Draw every (Transform, Sprite) entity in the registry.
    void renderScene(ecs::Registry& world, const Camera& camera);

    /// Bookkeeping end of frame (stats snapshot).
    void endFrame();

    /// Free GL objects (GL thread only). Idempotent.
    void shutdown();

    [[nodiscard]] const FrameStats& stats() const noexcept { return stats_; }
    [[nodiscard]] bool ready() const noexcept { return spriteShader_.valid(); }

    /// Default texture id for SpriteComponent.textureId == 0.
    [[nodiscard]] GLuint defaultTextureId() const noexcept {
        return whiteTexture_.id();
    }

private:
    Shader spriteShader_;
    Mesh quad_;
    Texture whiteTexture_;
    FrameStats stats_{};
    int width_ = 0;
    int height_ = 0;
};

}  // namespace anaconda::renderer
