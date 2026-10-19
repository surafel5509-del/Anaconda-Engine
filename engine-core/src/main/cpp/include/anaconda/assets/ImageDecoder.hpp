// ============================================================================
//  anaconda/assets/ImageDecoder.hpp
//
//  CPU-side image decoding for the asset pipeline, backed by vendored
//  stb_image (third_party/stb/stb_image.h — public domain, v2.30).
//
//  Deliberately GL-free so it builds on ANY host — the CI decodes a real PNG
//  in tests/test_image.cpp without an Android device.
//
//  Output contract: ALWAYS 8-bit RGBA (stbi desired_channels=4), row-major,
//  top-left origin — exactly what GLES glTexImage2D(GL_RGBA) consumes.
//
//  Supported formats: PNG, JPEG, BMP, GIF (first frame), TGA, PSD, HDR-ish
//  LDR fallbacks. The editor's SAF picker advertises PNG/JPEG/WebP; WebP is
//  rejected here with a clear error (convert-to-PNG messaging) until a WebP
//  decoder lands (see docs/ASSET_PIPELINE_AND_EXPORT.md).
// ============================================================================
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace anaconda::assets {

struct Image {
    std::vector<std::uint8_t> rgba;   // width * height * 4 bytes
    int width = 0;
    int height = 0;

    [[nodiscard]] bool valid() const noexcept {
        return width > 0 && height > 0 &&
               rgba.size() == static_cast<std::size_t>(width) * height * 4u;
    }
};

class ImageDecoder {
public:
    /// Decode a file from disk. On failure returns std::nullopt and (when
    /// provided) fills `error` with a human-readable reason for the editor
    /// toast/console.
    static std::optional<Image> decodeFile(const std::string& path,
                                           std::string* error = nullptr);

    /// Decode from a memory buffer (tests, content-resolver bytes later).
    static std::optional<Image> decodeMemory(const std::uint8_t* data,
                                             std::size_t size,
                                             std::string* error = nullptr);
};

}  // namespace anaconda::assets
