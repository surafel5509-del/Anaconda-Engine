// ============================================================================
//  assets/ImageDecoder.cpp — stb_image implementation TU.
//
//  STB_IMAGE_IMPLEMENTATION lives HERE and ONLY here — every other engine
//  file includes stb_image.h as declarations only (or not at all).
// ============================================================================
#include "anaconda/assets/ImageDecoder.hpp"

#include "anaconda/core/Log.hpp"

#include <cctype>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
// Collapse stb's API surface to what the pipeline needs:
#define STBI_FAILURE_USERMSG
#include "stb_image.h"

namespace anaconda::assets {

namespace {

std::optional<Image> fromStbi(const std::string& context,
                              std::uint8_t* pixels, int w, int h,
                              std::string* error) {
    if (pixels == nullptr || w <= 0 || h <= 0) {
        const char* reason = stbi_failure_reason();
        if (error != nullptr) {
            *error = context + ": " +
                     (reason != nullptr ? reason : "unknown decode failure");
        }
        stbi_image_free(pixels);   // no-op for nullptr, safe anyway
        return std::nullopt;
    }

    Image image;
    image.width = w;
    image.height = h;
    const std::size_t byteCount =
        static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4u;
    image.rgba.resize(byteCount);
    std::memcpy(image.rgba.data(), pixels, byteCount);
    stbi_image_free(pixels);

    if (!image.valid()) {
        if (error != nullptr) *error = context + ": size overflow";
        return std::nullopt;
    }
    return image;
}

}  // namespace

std::optional<Image> ImageDecoder::decodeFile(const std::string& path,
                                              std::string* error) {
    if (path.empty()) {
        if (error != nullptr) *error = "empty path";
        return std::nullopt;
    }
    // WebP fast-fail with actionable messaging (stb has no WebP support).
    const auto dot = path.find_last_of('.');
    if (dot != std::string::npos) {
        std::string ext = path.substr(dot + 1);
        for (char& c : ext) c = static_cast<char>(std::tolower(c));
        if (ext == "webp") {
            if (error != nullptr) {
                *error =
                    "WebP is not supported yet — re-export the image as PNG "
                    "or JPEG (" + path + ")";
            }
            return std::nullopt;
        }
    }

    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load(path.c_str(), &w, &h, &comp, 4 /* RGBA */);
    return fromStbi("decodeFile(" + path + ")", pixels, w, h, error);
}

std::optional<Image> ImageDecoder::decodeMemory(const std::uint8_t* data,
                                                std::size_t size,
                                                std::string* error) {
    if (data == nullptr || size < 8) {
        if (error != nullptr) *error = "buffer too small";
        return std::nullopt;
    }
    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load_from_memory(
        data, static_cast<int>(size), &w, &h, &comp, 4 /* RGBA */);
    return fromStbi("decodeMemory", pixels, w, h, error);
}

}  // namespace anaconda::assets
