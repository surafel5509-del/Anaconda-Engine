// ============================================================================
//  renderer/Texture.cpp — GLES3 texture objects (GL thread only).
// ============================================================================
#include "anaconda/renderer/Texture.hpp"

#include "anaconda/core/Log.hpp"

#include <utility>

namespace anaconda::renderer {

Texture::~Texture() { release(); }

Texture::Texture(Texture&& other) noexcept
    : id_(std::exchange(other.id_, 0)),
      width_(std::exchange(other.width_, 0)),
      height_(std::exchange(other.height_, 0)) {}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        release();
        id_ = std::exchange(other.id_, 0);
        width_ = std::exchange(other.width_, 0);
        height_ = std::exchange(other.height_, 0);
    }
    return *this;
}

bool Texture::createWhite() {
    // 1x1 opaque white: texture() * vColor == vColor. Lets every sprite use
    // the same shader path whether or not it has an imported image (Phase 2).
    const std::uint8_t white[4] = {255, 255, 255, 255};
    return uploadRGBA(white, 1, 1, /*linearFilter=*/true);
}

bool Texture::uploadRGBA(const std::uint8_t* rgbaPixels, int width, int height,
                         bool linearFilter) {
    if (rgbaPixels == nullptr || width <= 0 || height <= 0) {
        ANA_LOGE("[Texture] uploadRGBA invalid args (w=%d h=%d)", width, height);
        return false;
    }
    release();

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, rgbaPixels);

    const GLint filter = linearFilter ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    // Clamp (not repeat) — sprite sheets must never bleed across wrap edges
    // unless the importer metadata explicitly requests REPEAT (Phase 2).
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Unbind texture unit 0 — sprite pass rebinds before every draw anyway.
    glBindTexture(GL_TEXTURE_2D, 0);

    width_ = width;
    height_ = height;

    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ANA_LOGE("[Texture] uploadRGBA GL error 0x%x (%dx%d)", err, width, height);
        release();
        return false;
    }
    return true;
}

void Texture::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(unit));
    glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::release() {
    if (id_ != 0) {
        glDeleteTextures(1, &id_);
        id_ = 0;
    }
    width_ = height_ = 0;
}

}  // namespace anaconda::renderer
