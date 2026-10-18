// ============================================================================
//  anaconda/physics/PhysicsSystem.hpp
//
//  Phase-1 physics: deterministic, allocation-light, fully host-testable.
//
//  Per step (fixed dt supplied by the caller):
//    1. Integrate velocities under gravity for every Transform+RigidBody.
//    2. Build AABBs from Transform+BoxCollider (statics = collider w/o body).
//    3. Resolve overlaps along the axis of minimum penetration
//       (X/Y only in 2D mode — prevents absurd Z pops on flat sprites).
//    4. Contacts against statics along +Y set `grounded` and zero the fall
//       velocity — jump-ready straight out of the box.
//
//  Deliberately NOT here (roadmap): friction/restitution solver, joints,
//  continuous detection, spatial hashing — Months 3–4 (docs/ROADMAP.md).
// ============================================================================
#pragma once

#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/math/Math.hpp"

namespace anaconda {

/// World-space axis-aligned bounding box.
struct Aabb {
    Vec3 min{};
    Vec3 max{};

    [[nodiscard]] bool overlaps(const Aabb& o) const noexcept {
        return min.x <= o.max.x && max.x >= o.min.x &&
               min.y <= o.max.y && max.y >= o.min.y &&
               min.z <= o.max.z && max.z >= o.min.z;
    }
    [[nodiscard]] Vec3 center() const noexcept { return (min + max) * 0.5f; }
    [[nodiscard]] Vec3 halfExtents() const noexcept { return (max - min) * 0.5f; }
};

class PhysicsSystem {
public:
    /// World gravity (m/s²). Phase-1 default = classic earth fall on Y.
    Vec3 gravity{0.0f, -9.81f, 0.0f};

    /// 2D mode (default): collisions only resolve on X/Y — sprites live on a
    /// flat plane and must never be shoved along Z. Set false for true 3D.
    bool is2D = true;

    /// Integrate + collide the whole world. `dt` in seconds (clamped by the
    /// caller to avoid spiral-of-death after a stall).
    void step(ecs::Registry& world, float dt);

    /// Compute the world AABB of an entity's BoxCollider (scaled/offset by
    /// its Transform). Returns false when required components are missing.
    static bool computeAabb(const ecs::Registry& world, ecs::Entity e, Aabb& out);

    /// One-shot query used by tests, picking (Phase 3) and the debug draw.
    static bool raycastAabb(const Aabb& box, const Vec3& origin,
                            const Vec3& dir, float& tHit);

    struct Stats {
        int dynamicBodies = 0;
        int staticBodies = 0;
        int contacts = 0;
    };
    [[nodiscard]] const Stats& stats() const noexcept { return stats_; }

private:
    Stats stats_{};
};

}  // namespace anaconda
