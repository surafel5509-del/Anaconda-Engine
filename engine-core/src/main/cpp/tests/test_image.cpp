// ============================================================================
//  tests/test_image.cpp — Month-2 asset decoding (host, stb_image, no GL).
// ============================================================================
#include "anaconda/assets/ImageDecoder.hpp"
#include "test_framework.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace assets = anaconda::assets;

namespace {

// A REAL 1x1 opaque-red PNG (70 bytes), generated independently with zlib —
// decoding it on CI proves stb_image works inside the NDK/CMake build.
const std::vector<std::uint8_t> kRedPng = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, 0x00, 0x00, 0x00,
    0x0D, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8, 0xCF, 0xC0, 0xF0,
    0x1F, 0x00, 0x05, 0x00, 0x01, 0xFF, 0x56, 0xC7, 0x2F, 0x0D, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82,
};

}  // namespace

TEST(decodes_embedded_png_to_rgba) {
    std::string error;
    auto image = assets::ImageDecoder::decodeMemory(kRedPng.data(),
                                                    kRedPng.size(), &error);
    CHECK(image.has_value());
    if (!image.has_value()) return;

    CHECK(image->valid());
    CHECK(image->width == 1);
    CHECK(image->height == 1);
    CHECK(image->rgba.size() == 4u);
    CHECK(image->rgba[0] == 255);   // R
    CHECK(image->rgba[1] == 0);     // G
    CHECK(image->rgba[2] == 0);     // B
    CHECK(image->rgba[3] == 255);   // A
}

TEST(roundtrip_png_through_file) {
    const char* path = "/tmp/anaconda_engine_test_pixel.png";
    std::FILE* f = std::fopen(path, "wb");
    CHECK(f != nullptr);
    if (f == nullptr) return;
    std::fwrite(kRedPng.data(), 1, kRedPng.size(), f);
    std::fclose(f);

    std::string error;
    auto image = assets::ImageDecoder::decodeFile(path, &error);
    CHECK(image.has_value());
    if (image.has_value()) {
        CHECK(image->valid());
        CHECK(image->rgba[0] == 255);
        CHECK(image->rgba[3] == 255);
    }
    std::remove(path);
}

TEST(rejects_garbage_buffer_with_reason) {
    const std::uint8_t junk[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
                                 0x07, 0x08, 0x09, 0x0A, 0x0B};
    std::string error;
    auto image = assets::ImageDecoder::decodeMemory(junk, sizeof(junk), &error);
    CHECK(!image.has_value());
    CHECK(!error.empty());   // stbi_failure_reason() surfaced to the editor
}

TEST(rejects_tiny_buffer) {
    std::string error;
    const std::uint8_t tiny[] = {0x89, 0x50};
    auto image = assets::ImageDecoder::decodeMemory(tiny, sizeof(tiny), &error);
    CHECK(!image.has_value());
    CHECK(error.find("too small") != std::string::npos);
}

TEST(webp_gets_actionable_error) {
    std::string error;
    auto image = assets::ImageDecoder::decodeFile(
        "/any/path/texture.webp", &error);
    CHECK(!image.has_value());
    CHECK(error.find("PNG") != std::string::npos);   // tells the user what TO do
}

TEST(missing_file_fails_cleanly) {
    std::string error;
    auto image = assets::ImageDecoder::decodeFile(
        "/tmp/no_such_texture_12345.png", &error);
    CHECK(!image.has_value());
    CHECK(!error.empty());
}

TEST(empty_path_fails) {
    std::string error;
    auto image = assets::ImageDecoder::decodeFile("", &error);
    CHECK(!image.has_value());
    CHECK(error.find("path") != std::string::npos);
}

int main() { return anaconda::test::runAll(); }
