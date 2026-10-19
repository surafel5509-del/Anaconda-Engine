// ===========================================================================
//  renderer/Atlas.cpp — shelf packer implementation.
// ===========================================================================
#include "anaconda/renderer/Atlas.hpp"

#include <cstring>

namespace anaconda::renderer {

bool Atlas::tryPack(AtlasPage& page, int w, int h, int& outX, int& outY) {
    if (w > page.width || h > page.height) return false;
    // shelf algorithm: new row if needed
    if (page.cursorX + w > page.width) {
        page.shelfY += page.shelfHeight;
        page.cursorX = 0;
        page.shelfHeight = 0;
    }
    if (page.shelfY + h > page.height) return false;
    outX = page.cursorX;
    outY = page.shelfY;
    page.cursorX += w;
    if (h > page.shelfHeight) page.shelfHeight = h;
    return true;
}

const AtlasEntry* Atlas::addImage(std::uint32_t handle, int w, int h,
                                   const std::uint8_t* rgba, const std::string& path) {
    if (w <= 0 || h <= 0 || w > pageSize_ || h > pageSize_) return nullptr;

    // try existing pages
    for (size_t i = 0; i < pages_.size(); ++i) {
        int x = 0, y = 0;
        if (tryPack(pages_[i], w, h, x, y)) {
            // lazily allocate pixels
            if (pages_[i].pixels.empty())
                pages_[i].pixels.assign(pageSize_ * pageSize_ * 4, 0);
            // copy rgba into page pixels (simple row copy)
            for (int row = 0; row < h; ++row) {
                size_t dstOff = ((y + row) * pageSize_ + x) * 4;
                size_t srcOff = row * w * 4;
                std::memcpy(pages_[i].pixels.data() + dstOff, rgba + srcOff, w * 4);
            }
            pages_[i].dirty = true;
            AtlasEntry e;
            e.handle = handle;
            e.page = static_cast<int>(i);
            e.x = x; e.y = y; e.w = w; e.h = h;
            e.uv = Vec4{static_cast<float>(x) / pageSize_, static_cast<float>(y) / pageSize_,
                        static_cast<float>(w) / pageSize_, static_cast<float>(h) / pageSize_};
            e.sourcePath = path;
            auto it = entries_.emplace(handle, std::move(e));
            return &it.first->second;
        }
    }
    // need new page
    pages_.emplace_back(AtlasPage{pageSize_, pageSize_});
    AtlasPage& np = pages_.back();
    np.pixels.assign(pageSize_ * pageSize_ * 4, 0);
    int x = 0, y = 0;
    if (!tryPack(np, w, h, x, y)) return nullptr;
    for (int row = 0; row < h; ++row) {
        size_t dstOff = ((y + row) * pageSize_ + x) * 4;
        size_t srcOff = row * w * 4;
        std::memcpy(np.pixels.data() + dstOff, rgba + srcOff, w * 4);
    }
    np.dirty = true;
    AtlasEntry e;
    e.handle = handle;
    e.page = static_cast<int>(pages_.size() - 1);
    e.x = x; e.y = y; e.w = w; e.h = h;
    e.uv = Vec4{static_cast<float>(x) / pageSize_, static_cast<float>(y) / pageSize_,
                static_cast<float>(w) / pageSize_, static_cast<float>(h) / pageSize_};
    e.sourcePath = path;
    auto it = entries_.emplace(handle, std::move(e));
    return &it.first->second;
}

}  // namespace anaconda::renderer
