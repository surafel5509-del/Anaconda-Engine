// ===========================================================================
//  tests/test_prefab.cpp — prefab save/load/instantiate (host).
// ===========================================================================
#include "anaconda/scene/Prefab.hpp"
#include "anaconda/ecs/Components.hpp"
#include "test_framework.hpp"

using anaconda::TagComponent;
using anaconda::TransformComponent;
using anaconda::SpriteComponent;
namespace ecs = anaconda::ecs;
namespace scene = anaconda::scene;

TEST(prefab_save_and_instantiate) {
    ecs::Registry world;
    ecs::Entity root = world.create();
    world.add(root, TagComponent{"Root"});
    TransformComponent tf; tf.position = {1,2,0}; world.add(root, tf);
    SpriteComponent sp; sp.textureId = 42; sp.renderLayer = 5; world.add(root, sp);

    auto blob = scene::Prefab::saveToBlob(world, root, false);
    CHECK(!blob.empty());

    ecs::Registry dst;
    std::string err;
    ecs::Entity inst = scene::Prefab::instantiateFromBlob(dst, blob, {10,0,0}, &err);
    CHECK(inst != ecs::kNullEntity);
    CHECK(err.empty());
    CHECK(dst.aliveCount() == 1);
    auto& ntf = dst.get<TransformComponent>(inst);
    CHECK_NEAR(ntf.position.x, 11.0f, 1e-4f);
    CHECK_NEAR(ntf.position.y, 2.0f, 1e-4f);
    CHECK(dst.get<SpriteComponent>(inst).textureId == 42u);
    CHECK(dst.get<SpriteComponent>(inst).renderLayer == 5);
}

TEST(prefab_with_children) {
    ecs::Registry world;
    ecs::Entity parent = world.create();
    world.add(parent, TagComponent{"Parent"});
    TransformComponent ptf; ptf.position = {0,0,0}; world.add(parent, ptf);
    ecs::Entity child = world.create();
    world.add(child, TagComponent{"Child"});
    TransformComponent ctf; ctf.position = {1,0,0}; ctf.parent = static_cast<uint32_t>(parent); world.add(child, ctf);
    SpriteComponent sp; world.add(child, sp);

    auto blob = scene::Prefab::saveToBlob(world, parent, true);
    ecs::Registry dst;
    scene::Prefab::instantiateFromBlob(dst, blob, {}, nullptr);
    CHECK(dst.aliveCount() == 2);
}

int main() { return anaconda::test::runAll(); }
