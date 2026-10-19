// ===========================================================================
//  anaconda/ecs/Components.hpp
//
//  Component set — plain aggregates, no virtuals, no ownership.
//  Month 3-5 extensions: physics materials, animator, audio source, prefab
//  link, grid-aware and gizmo-friendly fields. Every addition keeps the
//  registry usable immediately via Registry::add<T>/View<T>.
// ===========================================================================
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "anaconda/math/Math.hpp"

namespace anaconda {

// ---------------------------------------------------------------------------
// Identity
// ---------------------------------------------------------------------------
struct TagComponent {
    std::string name = "Entity";
};

// ---------------------------------------------------------------------------
// Transform — EULER RADIANS, column-major TRS.
// ---------------------------------------------------------------------------
struct TransformComponent {
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 rotation{0.0f, 0.0f, 0.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
    std::uint32_t parent = 0xFFFFFFFFu; // kNullEntity = root
};

// ---------------------------------------------------------------------------
// Rendering — textureId 0 = white default.
// ---------------------------------------------------------------------------
struct SpriteComponent {
    Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    std::uint32_t textureId = 0;
    Vec2 size{1.0f, 1.0f};
    int renderLayer = 0;
    Vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f}; // Month 4: atlas UV trim (x,y,w,h)
};

struct SpriteAnimatorComponent {
    std::string currentClip;
    float time = 0.0f;
    float speed = 1.0f;
    bool playing = true;
    bool loop = true;
    int currentFrame = 0;
};

struct AnimationClip {
    std::string name;
    std::vector<int> frames; // indices into atlas or texture handles
    float fps = 12.0f;
    bool loop = true;
    Vec2 pivot{0.5f, 0.5f};
};

// ---------------------------------------------------------------------------
// Physics — Month 3 tuning: friction, restitution, damping, broadphase hints.
// ---------------------------------------------------------------------------
struct RigidBodyComponent {
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    float gravityScale = 1.0f;
    float mass = 1.0f;
    float linearDamping = 0.02f;
    float angularDamping = 0.05f;
    float friction = 0.4f;       // 0 = ice, 1 = sticky
    float restitution = 0.0f;    // 0 = dead, 1 = bouncy
    bool useGravity = true;
    bool grounded = false;
    bool isStatic = false;
    bool isKinematic = false;
};

struct BoxColliderComponent {
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    Vec3 center{0.0f, 0.0f, 0.0f};
    bool isTrigger = false;
    float friction = 0.4f;
    float restitution = 0.0f;
};

struct PhysicsMaterialComponent {
    float friction = 0.4f;
    float restitution = 0.2f;
    float density = 1.0f;
};

// ---------------------------------------------------------------------------
// Audio — Month 2/4: AudioSource for SFX (Phase A over AAudio/OpenSL).
// ---------------------------------------------------------------------------
struct AudioSourceComponent {
    std::string clipPath; // e.g. "audio/jump.ogg"
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    bool playOnAwake = false;
    bool spatialize = false;
};

// ---------------------------------------------------------------------------
// Scripting — Month 4 Lua 5.4 (sandboxed, hot-reloadable).
// ---------------------------------------------------------------------------
struct ScriptComponent {
    std::string assetPath;
    bool enabled = true;
    std::uint32_t luaEnvRef = 0;
    std::string sourceCache; // last loaded source (for hot reload diff)
    int lastErrorLine = 0;
    std::string lastError;
};

// ---------------------------------------------------------------------------
// Prefab — Month 3: link to source prefab + override map.
// ---------------------------------------------------------------------------
struct PrefabComponent {
    std::string prefabPath; // e.g. "prefabs/enemy.prefab"
    std::unordered_map<std::string, std::string> overrides;
    bool isOverride = false;
};

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
struct CameraComponent {
    float fieldOfViewDeg = 60.0f;
    float orthoHalfHeight = 5.0f;
    float zNear = 0.1f;
    float zFar = 100.0f;
    bool primary = false;
    float shakeAmount = 0.0f;
    float shakeDuration = 0.0f;
};

// ---------------------------------------------------------------------------
// Editor global settings (not ECS, but stored in Engine for convenience).
// ---------------------------------------------------------------------------
struct GridSettings {
    bool enabled = true;
    float cellSize = 1.0f;
    bool snapEnabled = false;
    float snapThreshold = 0.1f;
    Vec4 color{0.3f, 0.3f, 0.3f, 0.2f};
};

enum class GizmoMode : int {
    None = 0,
    Translate = 1,
    Rotate = 2,
    Scale = 3,
    Universal = 4,
};

enum class GizmoHandle : int {
    None = 0,
    Center = 1,
    X = 2,
    Y = 3,
    Z = 4,
    XY = 5,
};

}  // namespace anaconda
