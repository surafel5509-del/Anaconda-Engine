// ============================================================================
//  anaconda/scene/SceneSerializer.hpp
//
//  Versioned BINARY scene format (".scene") — the Month-2 persistence layer.
//
//  Design:
//    * Buffer-first API: serialize() → bytes, deserialize(bytes) — file I/O
//      is a thin wrapper, so roundtrip tests run on any host with no disk.
//    * Explicit little-endian, fixed-width encoding (uint32/f32) — identical
//      on arm64/armeabi-v7a/x86_64 CI. v1 targets Android + CI hosts only;
//      a byte-swap layer is deferred until a big-endian target exists.
//    * Forward-compatible flags: each component carries a presence bit. A
//      future format bump appends fields behind the version check — v1
//      readers refuse v2+ files with a clear error instead of misreading.
//    * Round-trips EVERY Phase-1/2 component (Tag, Transform, Sprite incl.
//      texture handle, RigidBody, BoxCollider, Script, Camera).
//
//  Layout (v1):
//    header : u32 magic 'ANSC' | u32 version | u32 entityCount | u32 reserved
//    entity : u32 componentFlags | per-flag payloads (see flags below)
// ============================================================================
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "anaconda/ecs/Ecs.hpp"

namespace anaconda::scene {

class SceneSerializer {
public:
    // 'ANSC' as little-endian bytes 0x43 0x53 0x4E 0x41 → "ANSC" on disk.
    static constexpr std::uint32_t kMagic = 0x43534E41u;
    static constexpr std::uint32_t kVersion = 1;

    // Component presence bits (per entity, u32).
    enum ComponentFlag : std::uint32_t {
        kHasTag = 1u << 0,
        kHasTransform = 1u << 1,
        kHasSprite = 1u << 2,
        kHasRigidBody = 1u << 3,
        kHasCollider = 1u << 4,
        kHasScript = 1u << 5,
        kHasCamera = 1u << 6,
    };

    /// Registry → portable bytes. Never throws; always returns a valid blob.
    static std::vector<std::uint8_t> serialize(const ecs::Registry& world);

    /// Bytes → Registry. CLEARS the destination first (all-or-nothing: on
    /// failure the destination is left empty and `error` explains why —
    /// a half-loaded scene is never a valid editor state).
    static bool deserialize(ecs::Registry& world,
                            const std::vector<std::uint8_t>& data,
                            std::string* error = nullptr);

    // ---- file wrappers ----------------------------------------------------
    static bool saveToFile(const ecs::Registry& world, const std::string& path,
                           std::string* error = nullptr);
    static bool loadFromFile(ecs::Registry& world, const std::string& path,
                             std::string* error = nullptr);

    /// Hard limits — malformed/hostile files must not OOM the editor.
    static constexpr std::uint32_t kMaxEntities = 1'000'000;
    static constexpr std::uint32_t kMaxStringBytes = 64u * 1024u;
};

}  // namespace anaconda::scene
