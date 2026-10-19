// ===========================================================================
//  anaconda/animation/AnimatorSystem.hpp — sprite frame animation (Month 4).
//
//  AnimatorComponent tracks time → frame index for a clip looked up from a
//  global ClipLibrary (animation database). Update() advances time by dt and
//  writes the current frame's texture handle / UV into SpriteComponent. Looping
//  and one-shot completion are handled here. Host-testable, no GL.
// ===========================================================================
#pragma once

#include "anaconda/ecs/Components.hpp"
#include "anaconda/ecs/Ecs.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace anaconda::animation {

struct Clip {
    std::string name;
    std::vector<std::uint32_t> frameTextures; // texture handles per frame
    std::vector<Vec4> frameUVs;               // optional per-frame UV
    float fps = 12.0f;
    bool loop = true;
    float duration() const { return frameTextures.empty() ? 0.0f : frameTextures.size() / fps; }
};

class ClipLibrary {
public:
    void addClip(Clip c) { clips_[c.name] = std::move(c); }
    const Clip* find(const std::string& name) const {
        auto it = clips_.find(name);
        return it != clips_.end() ? &it->second : nullptr;
    }
    bool remove(const std::string& name) { return clips_.erase(name) > 0; }
    size_t size() const { return clips_.size(); }
    void clear() { clips_.clear(); }

private:
    std::unordered_map<std::string, Clip> clips_;
};

// Advances animators, writes results into SpriteComponent.
class AnimatorSystem {
public:
    ClipLibrary library;

    void update(ecs::Registry& world, float dt);

    // Convenience: create a simple clip from sequential texture handles.
    void createSimpleClip(const std::string& name, const std::vector<std::uint32_t>& textures,
                          float fps = 12.0f, bool loop = true) {
        Clip c;
        c.name = name;
        c.frameTextures = textures;
        c.fps = fps;
        c.loop = loop;
        library.addClip(std::move(c));
    }
};

}  // namespace anaconda::animation
