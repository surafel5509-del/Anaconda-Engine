// ============================================================================
//  tests/test_ecs.cpp — sparse-set registry semantics (host, no GL).
// ============================================================================
#include "anaconda/ecs/Components.hpp"
#include "anaconda/ecs/Ecs.hpp"
#include "test_framework.hpp"

using anaconda::TransformComponent;
using anaconda::Vec3;
namespace ecs = anaconda::ecs;

namespace {
constexpr float kEps = 1e-5f;
}  // namespace

TEST(entity_create_valid_destroy) {
    ecs::Registry world;

    const ecs::Entity a = world.create();
    CHECK(world.valid(a));
    CHECK(world.aliveCount() == 1);

    // Destroy invalidates the handle immediately.
    world.destroy(a);
    CHECK(!world.valid(a));
    CHECK(world.aliveCount() == 0);

    // Double-destroy is a safe no-op.
    world.destroy(a);
    CHECK(world.aliveCount() == 0);
}

TEST(entity_generation_prevents_stale_aliasing) {
    ecs::Registry world;

    const ecs::Entity first = world.create();
    world.destroy(first);

    // The index is recycled but the GENERATION differs → old handle dead.
    const ecs::Entity second = world.create();
    CHECK(ecs::entityIndex(first) == ecs::entityIndex(second));
    CHECK(ecs::entityGeneration(first) != ecs::entityGeneration(second));
    CHECK(world.valid(second));
    CHECK(!world.valid(first));   // ← the entire point of packed generations
}

TEST(null_entity_is_invalid) {
    ecs::Registry world;
    CHECK(!world.valid(ecs::kNullEntity));
    world.destroy(ecs::kNullEntity);   // must not crash
    CHECK(world.aliveCount() == 0);
}

TEST(component_add_get_has_remove) {
    ecs::Registry world;
    const ecs::Entity e = world.create();

    CHECK(!world.has<TransformComponent>(e));

    auto& t = world.add(e, TransformComponent{});
    t.position = {1, 2, 3};
    CHECK(world.has<TransformComponent>(e));
    CHECK_NEAR(world.get<TransformComponent>(e).position.y, 2.0f, kEps);

    // add() again = overwrite in place (same slot, no duplicate storage).
    world.add(e, TransformComponent{});
    CHECK(world.has<TransformComponent>(e));
    CHECK_NEAR(world.get<TransformComponent>(e).position.y, 0.0f, kEps);

    world.remove<TransformComponent>(e);
    CHECK(!world.has<TransformComponent>(e));
}

TEST(optional_component_pointer_access) {
    ecs::Registry world;
    const ecs::Entity e = world.create();

    CHECK(world.tryGet<TransformComponent>(e) == nullptr);
    world.add(e, TransformComponent{});
    CHECK(world.tryGet<TransformComponent>(e) != nullptr);
    // Stale entity → nullptr, never a crash.
    world.destroy(e);
    CHECK(world.tryGet<TransformComponent>(e) == nullptr);
}

TEST(destroy_strips_all_components) {
    ecs::Registry world;
    const ecs::Entity e = world.create();
    world.add(e, TransformComponent{});
    world.add(e, anaconda::SpriteComponent{});

    world.destroy(e);
    const ecs::Entity recycled = world.create();   // same index slot

    // The recycled entity must NOT inherit the destroyed entity's components.
    CHECK(!world.has<TransformComponent>(recycled));
    CHECK(!world.has<anaconda::SpriteComponent>(recycled));
}

TEST(swap_and_pop_keeps_dense_arrays_consistent) {
    ecs::Registry world;
    const ecs::Entity a = world.create();
    const ecs::Entity b = world.create();
    const ecs::Entity c = world.create();

    world.add(a, TransformComponent{Vec3{1, 0, 0}, {}, {}});
    world.add(b, TransformComponent{Vec3{2, 0, 0}, {}, {}});
    world.add(c, TransformComponent{Vec3{3, 0, 0}, {}, {}});

    // Remove the MIDDLE entity → last one must swap into its slot and stay
    // reachable through its sparse entry.
    world.remove<TransformComponent>(b);
    CHECK(!world.has<TransformComponent>(b));
    CHECK(world.has<TransformComponent>(a));
    CHECK(world.has<TransformComponent>(c));
    CHECK_NEAR(world.get<TransformComponent>(a).position.x, 1.0f, kEps);
    CHECK_NEAR(world.get<TransformComponent>(c).position.x, 3.0f, kEps);

    auto& pool = world.poolFor<TransformComponent>();
    CHECK(pool.denseSize() == 2);
}

TEST(view_iterates_only_matching_entities) {
    ecs::Registry world;

    // 3 transforms, 2 sprites.
    const ecs::Entity e1 = world.create();
    world.add(e1, TransformComponent{});
    const ecs::Entity e2 = world.create();
    world.add(e2, TransformComponent{});
    world.add(e2, anaconda::SpriteComponent{});
    const ecs::Entity e3 = world.create();
    world.add(e3, TransformComponent{});
    world.add(e3, anaconda::SpriteComponent{});

    int visited = 0;
    ecs::view<TransformComponent, anaconda::SpriteComponent>(
        world, [&](ecs::Entity, TransformComponent&, anaconda::SpriteComponent&) {
            ++visited;
        });
    CHECK(visited == 2);

    // Single-component view sees all three.
    int transforms = 0;
    ecs::view<TransformComponent>(
        world, [&](ecs::Entity, TransformComponent&) { ++transforms; });
    CHECK(transforms == 3);

    // View respects destructions.
    world.destroy(e2);
    visited = 0;
    ecs::view<TransformComponent, anaconda::SpriteComponent>(
        world, [&](ecs::Entity, TransformComponent&, anaconda::SpriteComponent&) {
            ++visited;
        });
    CHECK(visited == 1);
}

TEST(view_can_mutate_components) {
    ecs::Registry world;
    for (int i = 0; i < 10; ++i) {
        const ecs::Entity e = world.create();
        world.add(e, TransformComponent{Vec3{static_cast<float>(i), 0, 0},
                                        {}, {}});
    }

    // Double every X — views hand out real references.
    ecs::view<TransformComponent>(world, [](ecs::Entity, TransformComponent& t) {
        t.position.x *= 2.0f;
    });

    for (const ecs::Entity e : world.aliveEntities()) {
        // i ranges 0..9 — doubled value must be even.
        const float x = world.get<TransformComponent>(e).position.x;
        CHECK_NEAR(std::fmod(x, 2.0f), 0.0f, kEps);
    }
}

TEST(alive_entities_snapshot_lists_everything) {
    ecs::Registry world;
    std::vector<ecs::Entity> handles;
    for (int i = 0; i < 16; ++i) handles.push_back(world.create());

    world.destroy(handles[5]);
    world.destroy(handles[6]);

    const auto alive = world.aliveEntities();
    CHECK(alive.size() == 14);
    for (const ecs::Entity e : alive) CHECK(world.valid(e));
    for (const ecs::Entity e : alive) {
        CHECK(e != handles[5]);
        CHECK(e != handles[6]);
    }
}

TEST(many_entities_stress) {
    ecs::Registry world;
    constexpr int kCount = 10000;
    for (int i = 0; i < kCount; ++i) {
        const ecs::Entity e = world.create();
        world.add(e, TransformComponent{Vec3{static_cast<float>(i), 0, 0},
                                        {}, {}});
    }
    CHECK(world.aliveCount() == static_cast<std::size_t>(kCount));

    // Destroy every odd entity, then verify the survivors' data integrity.
    const auto all = world.aliveEntities();
    for (std::size_t i = 1; i < all.size(); i += 2) world.destroy(all[i]);
    CHECK(world.aliveCount() == static_cast<std::size_t>(kCount / 2));

    int checked = 0;
    ecs::view<TransformComponent>(world, [&](ecs::Entity e,
                                             TransformComponent& t) {
        // Odd original indices were destroyed → survivors are even.
        CHECK(std::fmod(t.position.x, 2.0f) == 0.0f);
        (void)e;
        ++checked;
    });
    CHECK(checked == kCount / 2);
}

int main() { return anaconda::test::runAll(); }
