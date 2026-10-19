// ===========================================================================
//  anaconda/renderer/Atlas.hpp — texture atlas (Month 4).
//
//  Packs individual sprite images into atlas pages (2048x2048 default) for
//  batched rendering: one draw call per atlas page instead of per sprite.
//  Packing is simple shelf algorithm (fast, good enough for 2D). UV rects
//  are stored per source texture handle and later fed to the sprite shader
//  via per-instance UV uniform (or instanced VBO in Month 4+).
//
//  Host-testable (no GL): packing is pure math, GL upload is deferred to
//  Engine::renderFrame via RenderCommandQueue (same as individual textures).
// ===========================================================================
#pragma once

#include "anaconda/math/Math.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace anaconda::renderer {

struct AtlasEntry {
    std::uint32_t handle = 0; // original texture handle
    int page = 0;
    int x = 0, y = 0, w = 0, h = 0;
    Vec4 uv{0, 0, 1, 1}; // normalized UV rect within page
    std::string sourcePath;
};

struct AtlasPage {
    int width = 2048, height = 2048;
    int shelfY = 0, shelfHeight = 0, cursorX = 0;
    std::vector<std::uint8_t> pixels; // RGBA backing (page * 4 bytes)
    std::uint32_t glTexture = 0;      // GL name, 0 = not yet uploaded
    bool dirty = false;
};

class Atlas {
public:
    explicit Atlas(int pageSize = 2048) : pageSize_(pageSize) { pages_.emplace_back(AtlasPage{pageSize, pageSize}); }

    // Pack an RGBA image into the atlas. Returns entry with UV, or nullptr if too large.
    const AtlasEntry* addImage(std::uint32_t handle, int w, int h,
                              const std::uint8_t* rgba, const std::string& path = "");

    const AtlasEntry* findByHandle(std::uint32_t handle) const {
        auto it = entries_.find(handle);
        return it != entries_.end() ? &it->second : nullptr;
    }

    const AtlasEntry* findByPath(const std::string& path) const {
        for (auto& kv : entries_) if (kv.second.sourcePath == path) return &kv.second;
        return nullptr;
    }

    size_t pageCount() const { return pages_.size(); }
    size_t entryCount() const { return entries_.size(); }

    const AtlasPage& page(int idx) const { return pages_[idx]; }
    AtlasPage& page(int idx) { return pages_[idx]; }

    void clear() { pages_.clear(); entries_.clear(); pages_.emplace_back(AtlasPage{pageSize_, pageSize_}); }

    // For renderer: which page contains a handle's UV (for batching).
    int pageForHandle(std::uint32_t handle) const {
        auto e = findByHandle(handle);
        return e ? e->page : -1;
    }

private:
    bool tryPack(AtlasPage& page, int w, int h, int& outX, int& outY);

    int pageSize_;
    std::vector<AtlasPage> pages_;
    std::unordered_map<std::uint32_t, AtlasEntry> entries_;
};

}  // namespace anaconda::renderer
