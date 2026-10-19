// ===========================================================================
//  anaconda/scene/Prefab.hpp — prefab save/load + instantiate (Month 3).
//
//  Prefabs are entity subtrees serialized to JSON-ish text (human-editable,
//  diffable, git-friendly). Format is line-delimited JSON per entity, but
//  for Phase 3 we reuse the binary SceneSerializer for the subtree: a prefab
//  file is a SceneSerializer blob containing 1..N entities with parent links.
//  Instantiate clones the subtree with fresh IDs and optional position offset.
//
//  Overrides: when a prefab instance is dirty-edited, we store a map from
//  component field path → override JSON value. Re-applying a prefab update
//  (e.g. designer tweaks the source) merges non-overridden fields.
// ===========================================================================
#pragma once

#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/math/Math.hpp"
#include "anaconda/scene/SceneSerializer.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace anaconda::scene {

class Prefab {
public:
    // Save a single entity (and optionally its descendants via parent chain)
    // to a binary blob. If withChildren is true, we collect all entities
    // whose ancestor chain leads to root.
    static std::vector<uint8_t> saveToBlob(ecs::Registry& world, ecs::Entity root,
                                           bool withChildren = true);

    // Save blob to file. Returns true on success, error message on failure.
    static bool saveToFile(ecs::Registry& world, ecs::Entity root,
                           const std::string& path, std::string* error = nullptr);

    // Load blob from file and instantiate at given world position offset.
    // Returns instantiated root entity, or kNullEntity on failure.
    static ecs::Entity instantiateFromFile(ecs::Registry& world,
                                           const std::string& path,
                                           const Vec3& offset = {0, 0, 0},
                                           std::string* error = nullptr);

    static ecs::Entity instantiateFromBlob(ecs::Registry& world,
                                           const std::vector<uint8_t>& blob,
                                           const Vec3& offset = {0, 0, 0},
                                           std::string* error = nullptr);

    // Override tracking helpers (stored in PrefabComponent).
    static void setOverride(ecs::Entity instance, const std::string& field,
                            const std::string& value);
    static bool hasOverride(ecs::Entity instance, const std::string& field);
};

}  // namespace anaconda::scene
