// ===========================================================================
//  tests/test_picking.cpp — Month 3: picking, duplicate, sorting (host).
// ===========================================================================
#include "anaconda/ecs/Components.hpp"
#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/renderer/Camera.hpp"
#include "anaconda/scene/EditorSnapshotCodec.hpp"
#include "anaconda/scene/Picking.hpp"
#include "anaconda/scene/SceneSerializer.hpp"
#include "test_framework.hpp"

#include <algorithm>

using anaconda::Camera;
using anaconda::SpriteComponent;
using anaconda::TagComponent;
using anaconda::TransformComponent;
using anaconda::Vec2;
using anaconda::Vec3;
namespace ecs = anaconda::ecs;
namespace picking = anaconda::scene;

// Helper to create sprite entity at position with given layer and scale.
static ecs::Entity makeSprite(ecs::Registry& world, const std::string& name,
                              Vec3 pos, Vec3 scale, int layer, Vec2 size = {1, 1}) {
    ecs::Entity e = world.create();
    TagComponent tag; tag.name = name; world.add(e, tag);
    TransformComponent tf; tf.position = pos; tf.scale = scale; world.add(e, tf);
    SpriteComponent sp; sp.renderLayer = layer; sp.size = size; world.add(e, sp);
    return e;
}

TEST(camera_screenToWorld_roundtrip) {
    Camera cam;
    cam.position = {0, 0, 10};
    cam.target = {0, 0, 0};
    cam.orthoHalfHeight = 5.0f;
    // 100x100 viewport: center (50,50) → world (0,0)
    Vec2 w = cam.screenToWorld(50, 50, 100, 100);
    CHECK_NEAR(w.x, 0.0f, 1e-4f);
    CHECK_NEAR(w.y, 0.0f, 1e-4f);
    // Top-left (0,0) → world (-5, 5) for square aspect (h=5,w=5)
    w = cam.screenToWorld(0, 0, 100, 100);
    CHECK_NEAR(w.x, -5.0f, 1e-4f);
    CHECK_NEAR(w.y, 5.0f, 1e-4f);
    // Bottom-right (100,100) → (5, -5)
    w = cam.screenToWorld(100, 100, 100, 100);
    CHECK_NEAR(w.x, 5.0f, 1e-4f);
    CHECK_NEAR(w.y, -5.0f, 1e-4f);
    // Panned camera: target (10,0) shifts world +10
    cam.target.x = 10; cam.position.x = 10;
    w = cam.screenToWorld(50, 50, 100, 100);
    CHECK_NEAR(w.x, 10.0f, 1e-4f);
}

TEST(picking_hits_sprite_and_respects_layer) {
    ecs::Registry world;
    Camera cam;
    cam.orthoHalfHeight = 5.0f;
    // Two overlapping sprites at origin, different layers.
    ecs::Entity low = makeSprite(world, "Low", {0, 0, 0}, {1, 1, 1}, 0);
    ecs::Entity high = makeSprite(world, "High", {0, 0, 0}, {1, 1, 1}, 5);
    // Center of 200x200 viewport = world (0,0)
    ecs::Entity picked = picking::pickEntityAt(world, cam, 200, 200, 100, 100);
    CHECK(picked == high);
    // Empty point should miss.
    ecs::Entity miss = picking::pickEntityAt(world, cam, 200, 200, 10, 10);
    CHECK(miss == ecs::kNullEntity);
    (void)low;
}

TEST(picking_respects_AABB_with_collider) {
    ecs::Registry world;
    Camera cam; cam.orthoHalfHeight = 5.0f;
    ecs::Entity e = world.create();
    TagComponent tag; tag.name = "Box"; world.add(e, tag);
    TransformComponent tf; tf.position = {2, 0, 0}; tf.scale = {1,1,1}; world.add(e, tf);
    SpriteComponent sp; sp.renderLayer = 1; world.add(e, sp);
    anaconda::BoxColliderComponent col; col.halfExtents = {1,1,0.5f}; world.add(e, col);
    float sx = (2.0f - cam.target.x) / 0.05f + 100.0f; // worldPerPixel 0.05 for 200h
    ecs::Entity hit = picking::pickEntityAt(world, cam, 200, 200, sx, 100);
    CHECK(hit == e);
    float sxOut = (3.1f - cam.target.x) / 0.05f + 100.0f;
    CHECK(picking::pickEntityAt(world, cam, 200, 200, sxOut, 100) == ecs::kNullEntity);
}

TEST(duplicate_copies_all_components) {
    ecs::Registry world;
    ecs::Entity src = world.create();
    TagComponent tag; tag.name = "Hero"; world.add(src, tag);
    TransformComponent tf; tf.position = {1,2,3}; tf.scale={2,2,2}; world.add(src, tf);
    SpriteComponent sp; sp.color={0.1f,0.2f,0.3f,1.0f}; sp.textureId=99; sp.renderLayer=3; world.add(src, sp);
    anaconda::RigidBodyComponent rb; rb.gravityScale=1.5f; rb.useGravity=false; world.add(src, rb);
    anaconda::BoxColliderComponent col; col.halfExtents={0.5f,0.5f,0.5f}; world.add(src, col);

    ecs::Entity dst = picking::duplicateEntity(world, src);
    CHECK(dst != ecs::kNullEntity);
    CHECK(world.aliveCount() == 2);
    CHECK(world.get<TagComponent>(dst).name == "Hero Copy");
    CHECK_NEAR(world.get<TransformComponent>(dst).position.x, 1.5f, 1e-4f);
    CHECK(world.get<SpriteComponent>(dst).renderLayer == 3);
    CHECK(world.get<SpriteComponent>(dst).textureId == 99);
    CHECK(world.has<anaconda::RigidBodyComponent>(dst));
    CHECK(world.has<anaconda::BoxColliderComponent>(dst));
    CHECK(world.get<TagComponent>(src).name == "Hero");
}

TEST(sortedDrawOrder_respects_layer) {
    ecs::Registry world;
    makeSprite(world, "A", {0,0,0}, {1,1,1}, 2);
    makeSprite(world, "B", {0,0,0}, {1,1,1}, -1);
    makeSprite(world, "C", {0,0,0}, {1,1,1}, 5);
    auto order = picking::sortedDrawOrder(world);
    CHECK(order.size() == 3);
    CHECK(world.get<TagComponent>(order[0]).name == "B");
    CHECK(world.get<TagComponent>(order[1]).name == "A");
    CHECK(world.get<TagComponent>(order[2]).name == "C");
}

TEST(full_codec_roundtrip_with_layer) {
    ecs::Registry src;
    ecs::Entity srcEnt = src.create();
    TagComponent tag; tag.name="CodecLayer"; src.add(srcEnt, tag);
    TransformComponent tf; tf.position={1,1,0}; src.add(srcEnt, tf);
    SpriteComponent sp; sp.renderLayer=9; sp.textureId=123; src.add(srcEnt, sp);
    std::string line = anaconda::scene::EditorSnapshotCodec::writeEntity(src, srcEnt);
    CHECK(!line.empty());
    CHECK(std::count(line.begin(), line.end(), '|') == 8);
    ecs::Registry dst;
    std::string err;
    ecs::Entity dstEnt = anaconda::scene::EditorSnapshotCodec::parseEntity(dst, line, &err);
    CHECK(dstEnt != ecs::kNullEntity);
    CHECK(err.empty());
    CHECK(dst.get<SpriteComponent>(dstEnt).renderLayer == 9);
    CHECK(dst.get<SpriteComponent>(dstEnt).textureId == 123u);
    // Legacy 8-column line still parses (layer defaults 0)
    std::string legacy = "1|Legacy|0,0,0|0,0,0|1,1,1|1|1,1,1,1|55";
    ecs::Registry dst2;
    ecs::Entity leg = anaconda::scene::EditorSnapshotCodec::parseEntity(dst2, legacy, &err);
    CHECK(leg != ecs::kNullEntity);
    CHECK(dst2.get<SpriteComponent>(leg).renderLayer == 0);
    CHECK(dst2.get<SpriteComponent>(leg).textureId == 55u);
}

TEST(serializer_preserves_layer) {
    ecs::Registry src;
    ecs::Entity ent = src.create();
    TagComponent tag; tag.name="Ser"; src.add(ent, tag);
    TransformComponent tf; src.add(ent, tf);
    SpriteComponent sp; sp.renderLayer = -3; sp.textureId=7; src.add(ent, sp);
    auto blob = anaconda::scene::SceneSerializer::serialize(src);
    ecs::Registry dst;
    CHECK(anaconda::scene::SceneSerializer::deserialize(dst, blob, nullptr));
    auto ents = dst.aliveEntities();
    CHECK(ents.size() == 1);
    CHECK(dst.get<SpriteComponent>(ents[0]).renderLayer == -3);
}

int main() { return anaconda::test::runAll(); }
