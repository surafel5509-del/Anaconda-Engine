// ============================================================================
//  renderer/Renderer.cpp — frame orchestration + scene draw (GL thread only).
// ============================================================================
#include "anaconda/renderer/Renderer.hpp"

#include "anaconda/core/Log.hpp"
#include "anaconda/renderer/Shaders.hpp"

namespace anaconda::renderer {

bool Renderer::onSurfaceCreated() {
    // Surface (and possibly EGL context) was recreated — old GL names are
    // meaningless. Release defensively (no-ops if the context is truly dead:
    // deleting unknown names is silently ignored by GLES).
    shutdown();

    ANA_LOGI("[Renderer] surface created (GL: %s | GLSL: %s)",
             reinterpret_cast<const char*>(glGetString(GL_VERSION)),
             reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION)));

    // ---- global state policy ---------------------------------------------
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // Phase 1 is 2D: painter's order = hierarchy order. Depth testing would
    // fight semi-transparent sprites (z-fighting at z=0), so it stays OFF
    // until the 3D path lands with proper sorting (Month 4+).
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);   // quad winding kept irrelevant; both faces OK

    // ---- resources --------------------------------------------------------
    if (!spriteShader_.compile(shaders::kSpriteVertex, shaders::kSpriteFragment,
                               "sprite")) {
        ANA_LOGE("[Renderer] sprite shader failed — viewport will be blank");
        return false;
    }
    quad_ = Mesh::createQuad();
    if (!quad_.valid()) {
        ANA_LOGE("[Renderer] quad mesh creation failed");
        return false;
    }
    if (!whiteTexture_.createWhite()) {
        ANA_LOGE("[Renderer] default white texture failed");
        return false;
    }

    spriteShader_.use();
    spriteShader_.setInt("uTexture", 0);   // sampler → texture unit 0

    ANA_LOGI("[Renderer] ready (quad VAO=%u, white tex=%u)", quad_.id(),
             whiteTexture_.id());
    return true;
}

void Renderer::onSurfaceChanged(int width, int height) {
    width_ = width > 0 ? width : 1;
    height_ = height > 0 ? height : 1;
    // glViewport takes effect for subsequent draw calls on this thread.
    glViewport(0, 0, width_, height_);
    stats_.width = width_;
    stats_.height = height_;
    ANA_LOGI("[Renderer] viewport %dx%d", width_, height_);
}

void Renderer::beginFrame(const Vec4& clearColor) {
    stats_.drawCalls = 0;
    stats_.sprites = 0;

    glViewport(0, 0, width_ > 0 ? width_ : 1, height_ > 0 ? height_ : 1);
    glClearColor(clearColor.x, clearColor.y, clearColor.z, clearColor.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::renderScene(ecs::Registry& world, const Camera& camera) {
    if (!ready()) return;

    const float aspect =
        height_ > 0 ? static_cast<float>(width_) / static_cast<float>(height_)
                    : 1.0f;

    spriteShader_.use();
    const Mat4 proj = camera.projection(aspect);
    const Mat4 view = camera.view();
    spriteShader_.setMat4("uProj", proj.data());
    spriteShader_.setMat4("uView", view.data());
    spriteShader_.setInt("uTexture", 0);

    // Iterate entities that can actually draw. View picks the smaller of the
    // two pools — with 10k transforms and 3 sprites it scans 3 entries.
    ecs::view<TransformComponent, SpriteComponent>(world,
        [&](ecs::Entity, TransformComponent& transform, SpriteComponent& sprite) {
            const Vec3 worldScale{transform.scale.x * sprite.size.x,
                                  transform.scale.y * sprite.size.y,
                                  transform.scale.z};
            const Mat4 model = makeModelMatrix(transform.position,
                                               transform.rotation, worldScale);
            spriteShader_.setMat4("uModel", model.data());

            // Per-entity tint rides in a uniform (no VBO re-upload needed).
            spriteShader_.setVec4("uTint", sprite.color.x, sprite.color.y,
                                  sprite.color.z, sprite.color.w);

            // Bind the entity's texture, or the 1x1 white default.
            if (sprite.textureId != 0) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, sprite.textureId);
            } else {
                whiteTexture_.bind(0);
            }

            quad_.draw();
            ++stats_.drawCalls;
            ++stats_.sprites;
        });
}

void Renderer::endFrame() {
    // Statistics are read by Engine::fps()/JNI after the frame; nothing to do
    // here in Phase 1 (swap buffers is owned by GLSurfaceView). Kept as an
    // explicit seam for future GPU fence/timestamp bookkeeping.
}

void Renderer::shutdown() {
    spriteShader_.release();
    quad_.release();
    whiteTexture_.release();
}

}  // namespace anaconda::renderer
