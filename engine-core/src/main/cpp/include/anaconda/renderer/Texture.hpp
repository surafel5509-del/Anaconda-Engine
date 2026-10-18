// ============================================================================
//  anaconda/renderer/Texture.hpp
//
//  Minimal GLES3 texture wrapper for Phase 1:
//    * createWhite() — the 1x1 white default bound to unit 0 so the sprite
//      shader always samples a valid texture (tint-only sprites).
//    * uploadRGBA()  — the exact entry the Phase-2 asset pipeline will call
//      after stb_image decodes a PNG/JPG off-thread (the upload itself is
//      queued onto the GL thread — see docs/ARCHITECTURE.md §A.2).
//    * Params: linear filtering + clamp-to-edge (sprite-friendly defaults;
//      atlas modes get NEAREST/REPEAT options with importer metadata).
// ============================================================================
#pragma once

#include <GLES3/gl3.h>

#include <cstdint>

namespace anaconda::renderer {

class Texture {
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    bool createWhite();
    bool uploadRGBA(const std::uint8_t* rgbaPixels, int width, int height,
                    bool linearFilter = true);

    void bind(int unit = 0) const;
    void release();

    [[nodiscard]] GLuint id() const noexcept { return id_; }
    [[nodiscard]] bool valid() const noexcept { return id_ != 0; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }

private:
    GLuint id_ = 0;
    int width_ = 0;
    int height_ = 0;
};

}  // namespace anaconda::renderer
