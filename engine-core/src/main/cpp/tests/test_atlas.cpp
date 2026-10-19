// ===========================================================================
//  tests/test_atlas.cpp — texture atlas packing (host).
// ===========================================================================
#include "anaconda/renderer/Atlas.hpp"
#include "test_framework.hpp"

using anaconda::renderer::Atlas;

TEST(atlas_packs_sprites) {
    Atlas atlas(64);
    std::vector<uint8_t> rgba(32*32*4, 128);
    auto* e1 = atlas.addImage(1, 32, 32, rgba.data(), "a.png");
    CHECK(e1 != nullptr);
    CHECK(e1->page == 0);
    auto* e2 = atlas.addImage(2, 32, 32, rgba.data(), "b.png");
    CHECK(e2 != nullptr);
    CHECK(atlas.pageCount() == 1);
    CHECK(atlas.entryCount() == 2);
    CHECK(atlas.findByHandle(1) == e1);
    CHECK(atlas.findByPath("b.png") == e2);
}

TEST(atlas_overflow_creates_new_page) {
    Atlas atlas(64);
    std::vector<uint8_t> big(64*64*4, 255);
    auto* e1 = atlas.addImage(1, 64, 64, big.data());
    CHECK(e1 != nullptr);
    CHECK(e1->page == 0);
    auto* e2 = atlas.addImage(2, 32, 32, big.data());
    CHECK(e2 != nullptr);
    CHECK(e2->page == 1); // new page needed
    CHECK(atlas.pageCount() == 2);
}

TEST(atlas_rejects_oversized) {
    Atlas atlas(64);
    std::vector<uint8_t> huge(128*128*4, 0);
    auto* e = atlas.addImage(99, 128, 128, huge.data());
    CHECK(e == nullptr);
}

int main() { return anaconda::test::runAll(); }
