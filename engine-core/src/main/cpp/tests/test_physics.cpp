// ============================================================================
//  tests/test_physics.cpp — gravity, AABB resolution, grounded, raycast.
// ============================================================================
#include "anaconda/ecs/Components.hpp"
#include "anaconda/physics/PhysicsSystem.hpp"
#include "test_framework.hpp"

using anaconda::Aabb;
using anaconda::BoxColliderComponent;
using anaconda::PhysicsSystem;
using anaconda::RigidBodyComponent;
using anaconda::TransformComponent;
using anaconda::Vec3;
namespace ecs = anaconda::ecs;

namespace {

constexpr float kEps = 1e-4f;

/// A floor slab: static (collider, no rigidbody), top surface at topY.
ecs::Entity makeStaticFloor(ecs::Registry& world, float centerY,
                            float halfW = 10.0f, float halfH = 0.5f) {
    const ecs::Entity e = world.create();
    TransformComponent tf;
    tf.position = {0.0f, centerY, 0.0f};
    world.add(e, tf);
    BoxColliderComponent col;
    col.halfExtents = {halfW, halfH, 1.0f};
    world.add(e, col);
    return e;
}

/// A dynamic box with collider + rigidbody.
ecs::Entity makeDynamicBox(ecs::Registry& world, Vec3 pos) {
    const ecs::Entity e = world.create();
    TransformComponent tf;
    tf.position = pos;
    world.add(e, tf);
    BoxColliderComponent col;
    col.halfExtents = {0.5f, 0.5f, 0.5f};
    world.add(e, col);
    RigidBodyComponent rb;
    world.add(e, rb);
    return e;
}

}  // namespace

TEST(gravity_integrates_velocity) {
    ecs::Registry world;
    PhysicsSystem physics;

    const ecs::Entity e = makeDynamicBox(world, {0, 50, 0});
    // Move far from any statics → free fall only.

    const float dt = 1.0f / 60.0f;
    for (int i = 0; i < 60; ++i) physics.step(world, dt);   // exactly 1 second

    const auto& rb = world.get<RigidBodyComponent>(e);
    const auto& tf = world.get<TransformComponent>(e);

    // Semi-implicit Euler: after N steps of g·dt → v = g exactly.
    CHECK_NEAR(rb.velocity.y, -9.81f, 1e-3f);
    // Position must be strictly BELOW the start (fell), and by a plausible
    // distance (½gt² ≈ 4.9 m, Euler runs slightly more).
    CHECK(tf.position.y < 50.0f - 4.0f);
    CHECK(tf.position.y > 50.0f - 6.0f);
    CHECK(!rb.grounded);
}

TEST(gravity_scale_zero_means_weightless) {
    ecs::Registry world;
    PhysicsSystem physics;

    const ecs::Entity e = makeDynamicBox(world, {0, 50, 0});
    world.get<RigidBodyComponent>(e).gravityScale = 0.0f;

    const Vec3 start = world.get<TransformComponent>(e).position;
    for (int i = 0; i < 120; ++i) physics.step(world, 1.0f / 60.0f);

    // No gravity, no initial velocity → no motion.
    const Vec3 now = world.get<TransformComponent>(e).position;
    CHECK_NEAR(now.x, start.x, kEps);
    CHECK_NEAR(now.y, start.y, kEps);
    CHECK_NEAR(now.z, start.z, kEps);
}

TEST(body_lands_on_static_ground_and_gets_grounded) {
    ecs::Registry world;
    PhysicsSystem physics;

    // Ground top surface at y = -0.5.
    makeStaticFloor(world, /*centerY=*/-1.0f);
    // Box starts 1.5 above the ground top → falls ~1 m.
    const ecs::Entity box = makeDynamicBox(world, {0.0f, 1.0f, 0.0f});

    // Simulate 3 seconds — plenty to land and settle.
    for (int i = 0; i < 180; ++i) physics.step(world, 1.0f / 60.0f);

    const auto& rb = world.get<RigidBodyComponent>(box);
    const auto& tf = world.get<TransformComponent>(box);

    CHECK(rb.grounded);
    CHECK_NEAR(rb.velocity.y, 0.0f, kEps);           // fall stopped on contact
    // Resting position: box bottom touches ground top → center at y = 0.0
    // (ground top -0.5 + box half 0.5).
    CHECK_NEAR(tf.position.y, 0.0f, 0.05f);
    CHECK_NEAR(tf.position.x, 0.0f, kEps);           // no sideways drift
}

TEST(grounded_clears_once_leaving_ground) {
    ecs::Registry world;
    PhysicsSystem physics;

    makeStaticFloor(world, /*centerY=*/-1.0f);
    const ecs::Entity box = makeDynamicBox(world, {0.0f, 1.0f, 0.0f});
    for (int i = 0; i < 180; ++i) physics.step(world, 1.0f / 60.0f);
    CHECK(world.get<RigidBodyComponent>(box).grounded);

    // Teleport into free space far away → next steps must clear the flag.
    world.get<TransformComponent>(box).position = {0.0f, 40.0f, 0.0f};
    world.get<RigidBodyComponent>(box).velocity = {0, 0, 0};
    physics.step(world, 1.0f / 60.0f);
    CHECK(!world.get<RigidBodyComponent>(box).grounded);
}

TEST(dynamic_boxes_split_overlap_symmetrically) {
    ecs::Registry world;
    PhysicsSystem physics;
    physics.gravity = {0, 0, 0};   // isolate collision response

    // Two dynamic boxes overlapping by 1.0 on X (centers 0.4 apart,
    // combined half-widths = 1.0).
    const ecs::Entity a = makeDynamicBox(world, {-0.2f, 0, 0});
    const ecs::Entity b = makeDynamicBox(world, {0.2f, 0, 0});

    physics.step(world, 1.0f / 60.0f);

    const float xa = world.get<TransformComponent>(a).position.x;
    const float xb = world.get<TransformComponent>(b).position.x;
    CHECK(xb >= xa);   // order preserved (A left of B)
    // Overlap should be gone (or near-zero): distance ≥ combined halves (1.0).
    CHECK(xb - xa >= 1.0f - 0.02f);
    // Symmetric split: midpoint still ~0.
    CHECK_NEAR((xa + xb) * 0.5f, 0.0f, 0.01f);
}

TEST(aabb_respects_transform_scale) {
    ecs::Registry world;
    const ecs::Entity e = world.create();
    TransformComponent tf;
    tf.position = {10.0f, 0.0f, 0.0f};
    tf.scale = {2.0f, 4.0f, 1.0f};
    world.add(e, tf);
    BoxColliderComponent col;
    col.halfExtents = {0.5f, 0.5f, 0.5f};
    world.add(e, col);

    Aabb box;
    CHECK(PhysicsSystem::computeAabb(world, e, box));
    // half = 0.5 * scale = (1, 2, 0.5) around center (10, 0, 0).
    CHECK_NEAR(box.min.x, 9.0f, kEps);
    CHECK_NEAR(box.max.x, 11.0f, kEps);
    CHECK_NEAR(box.min.y, -2.0f, kEps);
    CHECK_NEAR(box.max.y, 2.0f, kEps);
    CHECK_NEAR(box.min.z, -0.5f, kEps);
    CHECK_NEAR(box.max.z, 0.5f, kEps);

    // Negative scale must not invert min/max (uses |scale|).
    world.get<TransformComponent>(e).scale = {-2.0f, -4.0f, 1.0f};
    CHECK(PhysicsSystem::computeAabb(world, e, box));
    CHECK(box.min.x <= box.max.x);
    CHECK(box.min.y <= box.max.y);
}

TEST(raycast_aabb_slab_method) {
    const Aabb box{{-1, -1, -1}, {1, 1, 1}};

    float t = 0.0f;
    // Straight shot along -Z from z = 5 → hit at z = 1 → t = 4.
    CHECK(PhysicsSystem::raycastAabb(box, {0, 0, 5}, {0, 0, -1}, t));
    CHECK_NEAR(t, 4.0f, 1e-4f);

    // Miss: ray passes above the box.
    CHECK(!PhysicsSystem::raycastAabb(box, {0, 5, 5}, {0, 0, -1}, t));

    // Ray pointing away from the box → no hit.
    CHECK(!PhysicsSystem::raycastAabb(box, {0, 0, 5}, {0, 0, 1}, t));

    // Parallel outside a slab → no hit.
    CHECK(!PhysicsSystem::raycastAabb(box, {5, 0, 5}, {0, 0, -1}, t));
}

TEST(stat_counters_report_body_kinds) {
    ecs::Registry world;
    PhysicsSystem physics;
    physics.gravity = {0, 0, 0};

    makeStaticFloor(world, -5.0f);                 // static #1
    makeDynamicBox(world, {0, 0, 0});              // dynamic #1
    makeDynamicBox(world, {3, 0, 0});              // dynamic #2

    // Also: entity with collider but no transform → ignored entirely.
    const ecs::Entity broken = world.create();
    world.add(broken, BoxColliderComponent{});

    physics.step(world, 1.0f / 60.0f);
    CHECK(physics.stats().staticBodies == 1);
    CHECK(physics.stats().dynamicBodies == 2);
}

int main() { return anaconda::test::runAll(); }
