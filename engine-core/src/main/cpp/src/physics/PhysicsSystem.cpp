// ============================================================================
//  physics/PhysicsSystem.cpp — integrate + AABB resolve (host-testable, no GL).
// ============================================================================
#include "anaconda/physics/PhysicsSystem.hpp"

#include "anaconda/core/Log.hpp"
#include "anaconda/ecs/Components.hpp"

#include <cmath>
#include <vector>

namespace anaconda {

namespace {

/// Cap falling speed so a long frame stall doesn't tunnel bodies through
/// thin ground (cheap substitute until continuous detection lands).
constexpr float kMaxFallSpeed = 60.0f;

/// Resolve overlap `mtv` of dynamic entity `dyn` away from `otherCenter`.
void applyCorrection(TransformComponent& tf, const Vec3& mtv, bool splitWithOther,
                     const Vec3& otherCenter, const Vec3& dynCenter) {
    Vec3 correction = mtv;
    if (splitWithOther) {
        correction = mtv * 0.5f;
    }
    tf.position += correction;
    (void)otherCenter;
    (void)dynCenter;
}

}  // namespace

// ---------------------------------------------------------------------------
// World AABB from Transform + BoxCollider
// ---------------------------------------------------------------------------
bool PhysicsSystem::computeAabb(const ecs::Registry& world, ecs::Entity e,
                                Aabb& out) {
    const auto* tf = world.tryGet<TransformComponent>(e);
    const auto* col = world.tryGet<BoxColliderComponent>(e);
    if (tf == nullptr || col == nullptr) return false;

    // Negative scale flips the box around its center — |scale| keeps min<=max.
    const Vec3 absScale = Vec3::abs(tf->scale);
    const Vec3 worldHalf{col->halfExtents.x * absScale.x,
                         col->halfExtents.y * absScale.y,
                         col->halfExtents.z * absScale.z};
    // Collider center offset lives in LOCAL space → rotate? Phase-1 colliders
    // are unrotated AABBs (rotation ignored on purpose — documented limitation
    // until OBB support in Month 4). Offset is scaled only.
    const Vec3 worldCenter = tf->position +
                             Vec3{col->center.x * absScale.x,
                                  col->center.y * absScale.y,
                                  col->center.z * absScale.z};

    out.min = worldCenter - worldHalf;
    out.max = worldCenter + worldHalf;
    return true;
}

// ---------------------------------------------------------------------------
// Ray vs AABB (slab method) — used by tests + Phase-3 viewport picking.
// ---------------------------------------------------------------------------
bool PhysicsSystem::raycastAabb(const Aabb& box, const Vec3& origin,
                                const Vec3& dir, float& tHit) {
    float tMin = -1e30f, tMax = 1e30f;

    const float o[3] = {origin.x, origin.y, origin.z};
    const float d[3] = {dir.x, dir.y, dir.z};
    const float mn[3] = {box.min.x, box.min.y, box.min.z};
    const float mx[3] = {box.max.x, box.max.y, box.max.z};

    for (int axis = 0; axis < 3; ++axis) {
        if (std::fabs(d[axis]) < 1e-8f) {
            if (o[axis] < mn[axis] || o[axis] > mx[axis]) return false;
            continue;
        }
        float t1 = (mn[axis] - o[axis]) / d[axis];
        float t2 = (mx[axis] - o[axis]) / d[axis];
        if (t1 > t2) std::swap(t1, t2);
        tMin = t1 > tMin ? t1 : tMin;
        tMax = t2 < tMax ? t2 : tMax;
        if (tMin > tMax) return false;
    }
    if (tMax < 0.0f) return false;          // box behind the ray
    tHit = tMin >= 0.0f ? tMin : tMax;      // origin inside → exit distance
    return true;
}

// ---------------------------------------------------------------------------
// step()
// ---------------------------------------------------------------------------
void PhysicsSystem::step(ecs::Registry& world, float dt) {
    stats_ = {};

    if (dt <= 0.0f) return;
    // Safety clamp (the Engine already clamps wall-clock dt; belt & braces).
    if (dt > 0.1f) dt = 0.1f;

    // -------------------------------------------------------------------------
    // 1) Velocity integration (gravity + semi-implicit Euler)
    // -------------------------------------------------------------------------
    ecs::view<TransformComponent, RigidBodyComponent>(
        world, [&](ecs::Entity, TransformComponent& tf, RigidBodyComponent& rb) {
            ++stats_.dynamicBodies;

            // Fresh contact flag every step — the contact pass below re-sets
            // it to true when a supporting surface is found this frame.
            rb.grounded = false;

            if (rb.useGravity) {
                rb.velocity += gravity * (rb.gravityScale * dt);
            }

            // Terminal velocity clamp (vertical axis).
            if (rb.velocity.y < -kMaxFallSpeed) rb.velocity.y = -kMaxFallSpeed;

            tf.position += rb.velocity * dt;
        });

    // -------------------------------------------------------------------------
    // 2) Gather AABBs:  statics = Transform+Collider WITHOUT a RigidBody
    //                   dynamics = Transform+Collider+RigidBody
    // -------------------------------------------------------------------------
    struct Body {
        ecs::Entity entity;
        Aabb box;
        bool isStatic;
    };
    std::vector<Body> bodies;
    bodies.reserve(world.aliveCount());

    std::vector<ecs::Entity> all = world.aliveEntities();
    for (const ecs::Entity e : all) {
        Aabb box;
        if (!computeAabb(world, e, box)) continue;
        const bool isDynamic = world.has<RigidBodyComponent>(e);
        bodies.push_back({e, box, !isDynamic});
        if (!isDynamic) ++stats_.staticBodies;
    }

    // -------------------------------------------------------------------------
    // 3) Narrowphase + resolution (brute-force; grid broadphase = Month 3)
    // -------------------------------------------------------------------------
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            Body& a = bodies[i];
            Body& b = bodies[j];
            if (a.isStatic && b.isStatic) continue;
            if (!a.box.overlaps(b.box)) continue;
            ++stats_.contacts;

            // --- minimum translation vector on candidate axes ---------------
            const float overlaps[3] = {
                (a.box.max.x < b.box.max.x ? a.box.max.x : b.box.max.x) -
                (a.box.min.x > b.box.min.x ? a.box.min.x : b.box.min.x),
                (a.box.max.y < b.box.max.y ? a.box.max.y : b.box.max.y) -
                (a.box.min.y > b.box.min.y ? a.box.min.y : b.box.min.y),
                (a.box.max.z < b.box.max.z ? a.box.max.z : b.box.max.z) -
                (a.box.min.z > b.box.min.z ? a.box.min.z : b.box.min.z),
            };
            if (overlaps[0] <= 0.0f || overlaps[1] <= 0.0f) continue;
            if (!is2D && overlaps[2] <= 0.0f) continue;

            // Pick least-penetration axis (skip Z entirely in 2D mode).
            int axis = 1;
            float smallest = overlaps[1];
            if (overlaps[0] < smallest) { smallest = overlaps[0]; axis = 0; }
            if (!is2D && overlaps[2] < smallest) { smallest = overlaps[2]; axis = 2; }

            const Vec3 aCenter = a.box.center();
            const Vec3 bCenter = b.box.center();

            // Direction: push A to the side away from B (stable when centers
            // coincide → default +axis).
            float sign = 1.0f;
            const float diff = axis == 0 ? (aCenter.x - bCenter.x)
                             : axis == 1 ? (aCenter.y - bCenter.y)
                                         : (aCenter.z - bCenter.z);
            if (diff < 0.0f) sign = -1.0f;

            Vec3 mtv{0, 0, 0};
            if (axis == 0) mtv.x = smallest * sign;
            else if (axis == 1) mtv.y = smallest * sign;
            else mtv.z = smallest * sign;

            // --- apply -------------------------------------------------------
            const bool aStatic = a.isStatic;
            const bool bStatic = b.isStatic;

            if (aStatic && bStatic) continue;   // unreachable (filtered above)

            auto* tfA = world.tryGet<TransformComponent>(a.entity);
            auto* tfB = world.tryGet<TransformComponent>(b.entity);

            if (aStatic && !bStatic) {
                // Move B fully away from A, opposite to A→B direction.
                if (tfB) {
                    applyCorrection(*tfB, mtv * -1.0f, /*split=*/false, aCenter,
                                    bCenter);
                    // B's upward push is -mtv.y — grounded when B (above the
                    // static) is supported from below: -mtv.y > 0 ⇔ mtv.y < 0.
                    auto* rbB = world.tryGet<RigidBodyComponent>(b.entity);
                    if (rbB && axis == 1 && mtv.y < 0.0f) {
                        rbB->grounded = true;
                        if (rbB->velocity.y < 0.0f) rbB->velocity.y = 0.0f;
                    }
                }
            } else if (bStatic && !aStatic) {
                if (tfA) {
                    applyCorrection(*tfA, mtv, /*split=*/false, bCenter, aCenter);
                    // A's push is mtv itself — grounded when mtv pushes up.
                    auto* rbA = world.tryGet<RigidBodyComponent>(a.entity);
                    if (rbA && axis == 1 && mtv.y > 0.0f) {
                        rbA->grounded = true;
                        if (rbA->velocity.y < 0.0f) rbA->velocity.y = 0.0f;
                    }
                }
            } else {
                // Both dynamic → split the correction symmetrically.
                if (tfA) applyCorrection(*tfA, mtv, true, bCenter, aCenter);
                if (tfB) applyCorrection(*tfB, mtv * -1.0f, true, aCenter, bCenter);
            }

            // Sequential resolution: keep cached boxes fresh so later pairs
            // in this same pass test against the corrected geometry.
            computeAabb(world, a.entity, a.box);
            computeAabb(world, b.entity, b.box);
        }
    }
}

}  // namespace anaconda
