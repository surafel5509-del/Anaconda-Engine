// ============================================================================
//  anaconda/ecs/Components.hpp
//
//  Phase-1 component set. Components are plain aggregates — no virtuals, no
//  ownership of engine resources (renderer/physics/script systems interpret
//  the data).
//
//  Extending: add a struct here (or in a domain header), and it is usable
//  immediately with Registry::add<T>/View<T>. Serialization (Month 2) will
//  register each component with a stable name + field reflection table.
// ============================================================================
#pragma once

#include <cstdint>
#include <string>

#include "anaconda/math/Math.hpp"

namespace anaconda {

// ---------------------------------------------------------------------------
// Identity ------------------------------------------------------------------
// ---------------------------------------------------------------------------
struct TagComponent {
    std::string name = "Entity";
};

// ---------------------------------------------------------------------------
// Transform -----------------------------------------------------------------
// rotation is EULER ANGLES IN RADIANS (UI sends degrees, JNI converts).
// World matrix = T * Rz * Ry * Rx * S  (see makeModelMatrix()).
// ---------------------------------------------------------------------------
struct TransformComponent {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};

    /// Optional parent index (raw entity); kNullEntity = root. Phase 3
    /// activates hierarchical propagation — stored now so the layout is final.
    std::uint32_t parent = 0xFFFFFFFFu;
};

// ---------------------------------------------------------------------------
// Rendering -----------------------------------------------------------------
// textureId == 0 → renderer binds its 1x1 white default (tint-only sprites).
// ---------------------------------------------------------------------------
struct SpriteComponent {
    Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::uint32_t textureId = 0;   // GL name (Phase 2 fills via asset pipeline)
    Vec2 size{1.0f, 1.0f};         // multiplies Transform.scale
    int renderLayer = 0;           // sort key (Phase 2 batching)
};

// ---------------------------------------------------------------------------
// Physics -------------------------------------------------------------------
// ---------------------------------------------------------------------------
struct RigidBodyComponent {
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    float gravityScale = 1.0f;     // 0 = float in space; negative = rise
    float mass = 1.0f;             // used by dynamic-vs-dynamic split (Phase 3)
    bool useGravity = true;
    bool grounded = false;         // written by PhysicsSystem each step
    bool isStatic = false;         // Phase 3: kinematic/static body kinds
};

/// Axis-aligned box collider. Extents are HALF-sizes in local space, scaled
/// component-wise by Transform.scale, offset by `center` (also scaled).
struct BoxColliderComponent {
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    Vec3 center{0.0f, 0.0f, 0.0f};
    bool isTrigger = false;        // detect-only, no resolution (Phase 3 hooks)
};

// ---------------------------------------------------------------------------
// Scripting (dormant until Phase 4 — layout frozen now) ---------------------
// ---------------------------------------------------------------------------
struct ScriptComponent {
    std::string assetPath;         // e.g. "scripts/player.lua"
    bool enabled = true;
    std::uint32_t luaEnvRef = 0;   // Lua registry ref (LUA_NOREF = 0 sentinel)
};

// ---------------------------------------------------------------------------
// Camera (scene camera entities — Phase 3; primary editor camera is engine-
// owned today, but the component exists so exported scenes can carry cameras).
// ---------------------------------------------------------------------------
struct CameraComponent {
    float fieldOfViewDeg = 60.0f;
    float orthoHalfHeight = 5.0f;
    float zNear = 0.1f;
    float zFar = 100.0f;
    bool primary = false;          // exported player uses first primary camera
};

}  // namespace anaconda
