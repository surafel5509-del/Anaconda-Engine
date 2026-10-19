// ============================================================================
//  anaconda/assets/AssetManager.hpp
//
//  Bridges FILE-SPACE assets and GL objects.
//
//  Why indirection? The editor assigns a texture to a sprite from the UI
//  thread IMMEDIATELY after import, but the GL texture only exists after the
//  render thread drains the upload command. So:
//
//      importTexture(path)  →  TextureHandle (stable, uint32, starts at 1)
//      (queued) GL upload   →  handle → GLuint recorded
//      SpriteComponent.textureId holds the HANDLE
//      Renderer, on GL thread: resolve(handle) → GLuint (0 = pending/failed
//        → binds the white default, so sprites render instantly either way)
//
//  Thread-safety: createHandle/completeFromDecode/path lookup take a short
//  mutex (imports run on IO threads); resolve() runs on the GL thread and
//  takes the same mutex — map lookups are microseconds, contention negligible.
//
//  Lives with the renderer (GLES dependency) → Android-only CMake target.
// ============================================================================
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "anaconda/assets/ImageDecoder.hpp"
#include "anaconda/core/Log.hpp"
#include "anaconda/renderer/Texture.hpp"

namespace anaconda::assets {

using TextureHandle = std::uint32_t;
inline constexpr TextureHandle kInvalidTextureHandle = 0;

class AssetManager {
public:
    /// IO thread. Registers (or reuses) a handle for `path`. Handle 0 is
    /// never returned — 0 means "no texture / use white" everywhere.
    TextureHandle createHandle(const std::string& path) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto existing = byPath_.find(path);
        if (existing != byPath_.end()) return existing->second;

        const TextureHandle handle = nextHandle_++;
        byPath_[path] = handle;
        pathByHandle_[handle] = path;
        return handle;
    }

    /// GL thread — called from the queued upload command. Moves the finished
    /// GL texture in and records the mapping.
    void completeUpload(TextureHandle handle, renderer::Texture&& texture) {
        if (handle == kInvalidTextureHandle) return;
        const int w = texture.width();      // read BEFORE the move
        const int h = texture.height();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            textures_[handle] = std::move(texture);
            failed_.erase(handle);
        }
        ANA_LOGI("[Assets] handle %u upload complete (%dx%d)", handle, w, h);
    }

    /// GL thread — decode/upload failed: mark so resolve() stops hoping.
    void failUpload(TextureHandle handle) {
        std::lock_guard<std::mutex> lock(mutex_);
        failed_.insert(handle);
        ANA_LOGW("[Assets] handle %u marked failed (white fallback)", handle);
    }

    /// GL thread (render pass). Returns the GL name, or 0 when the texture is
    /// still uploading or failed — caller binds the white default for 0.
    [[nodiscard]] GLuint resolve(TextureHandle handle) const {
        if (handle == kInvalidTextureHandle) return 0;
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = textures_.find(handle);
        if (it == textures_.end()) return 0;   // pending or failed → white
        return it->second.id();
    }

    [[nodiscard]] bool isReady(TextureHandle handle) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return textures_.count(handle) > 0;
    }

    [[nodiscard]] bool hasFailed(TextureHandle handle) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return failed_.count(handle) > 0;
    }

    /// UI thread — asset browser "is this file imported?" queries.
    [[nodiscard]] TextureHandle handleForPath(const std::string& path) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = byPath_.find(path);
        return it != byPath_.end() ? it->second : kInvalidTextureHandle;
    }

    [[nodiscard]] std::size_t textureCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return textures_.size();
    }

    /// IO thread — cache the decoded RGBA for this handle. The cache is the
    /// single CPU-side copy: the GL upload command and EGL-context-recovery
    /// re-uploads both reference this shared image (no per-upload copies).
    void cacheImage(TextureHandle handle, std::shared_ptr<const Image> image) {
        if (handle == kInvalidTextureHandle || !image) return;
        std::lock_guard<std::mutex> lock(mutex_);
        decoded_[handle] = std::move(image);
    }

    [[nodiscard]] std::shared_ptr<const Image> cachedImage(
        TextureHandle handle) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto it = decoded_.find(handle);
        return it != decoded_.end() ? it->second : nullptr;
    }

    /// GL thread — (re)create the GL texture from the cached decode. Used by
    /// the queued upload command AND by reuploadAllOnGl after context loss.
    void uploadFromCacheOnGl(TextureHandle handle) {
        auto image = cachedImage(handle);
        if (!image || !image->valid()) {
            failUpload(handle);
            return;
        }
        renderer::Texture texture;
        if (texture.uploadRGBA(image->rgba.data(), image->width, image->height,
                               /*linearFilter=*/true)) {
            completeUpload(handle, std::move(texture));
        } else {
            failUpload(handle);
        }
    }

    /// GL thread, inside Engine::onSurfaceCreated — old GL names died with the
    /// context; rebuild every cached asset so loaded scenes keep their art.
    void reuploadAllOnGl() {
        std::vector<std::pair<TextureHandle, std::shared_ptr<const Image>>> batch;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            batch.assign(decoded_.begin(), decoded_.end());
        }
        if (batch.empty()) return;
        for (const auto& [handle, _] : batch) uploadFromCacheOnGl(handle);
        ANA_LOGI("[Assets] re-uploaded %zu texture(s) after surface creation",
                 batch.size());
    }

    /// GL thread (context loss / shutdown): drop all GL objects. Handles stay
    /// valid — decoded CPU pixels remain cached for re-upload.
    void releaseAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        textures_.clear();
        failed_.clear();
    }

private:
    mutable std::mutex mutex_;
    TextureHandle nextHandle_ = 1;   // 0 reserved (== "no texture")
    std::unordered_map<std::string, TextureHandle> byPath_;
    std::unordered_map<TextureHandle, std::string> pathByHandle_;
    std::unordered_map<TextureHandle, renderer::Texture> textures_;
    std::unordered_map<TextureHandle, std::shared_ptr<const Image>> decoded_;
    std::unordered_set<TextureHandle> failed_;
};

}  // namespace anaconda::assets
