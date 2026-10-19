// ============================================================================
//  tests/test_scene.cpp — Month-2 scene serialization (host, no GL).
// ============================================================================
#include "anaconda/ecs/Components.hpp"
#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/scene/EditorSnapshotCodec.hpp"
#include "anaconda/scene/SceneSerializer.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

using anaconda::BoxColliderComponent;
using anaconda::CameraComponent;
using anaconda::RigidBodyComponent;
using anaconda::ScriptComponent;
using anaconda::SpriteComponent;
using anaconda::TagComponent;
using anaconda::TransformComponent;
using anaconda::Vec3;
using anaconda::Vec4;
namespace ecs = anaconda::ecs;
namespace scene = anaconda::scene;

namespace {
constexpr float kEps = 1e-5f;

/// A representative "kitchen sink" entity exercising every component type.
ecs::Entity makeRichEntity(ecs::Registry& world, const std::string& name) {
    const ecs::Entity e = world.create();

    TagComponent tag;
    tag.name = name;
    world.add(e, tag);

    TransformComponent tf;
    tf.position = {1.25f, -2.5f, 3.75f};
    tf.rotation = {0.1f, -0.2f, 0.3f};
    tf.scale = {2.0f, 0.5f, 1.5f};
    tf.parent = 0x00ABCDEFu;
    world.add(e, tf);

    SpriteComponent sp;
    sp.color = {0.9f, 0.2f, 0.4f, 0.8f};
    sp.textureId = 0x00C0FFEEu;   // asset handle must survive the trip
    sp.size = {1.5f, 2.5f};
    sp.renderLayer = 7;
    world.add(e, sp);

    RigidBodyComponent rb;
    rb.velocity = {0.5f, -9.8f, 0.0f};
    rb.gravityScale = 1.5f;
    rb.mass = 2.25f;
    rb.useGravity = false;
    rb.isStatic = true;
    rb.grounded = true;
    world.add(e, rb);

    BoxColliderComponent col;
    col.halfExtents = {0.4f, 0.6f, 0.8f};
    col.center = {0.1f, 0.2f, 0.3f};
    col.isTrigger = true;
    world.add(e, col);

    ScriptComponent sc;
    sc.assetPath = "scripts/player.lua";
    sc.enabled = false;
    sc.luaEnvRef = 42;
    world.add(e, sc);

    CameraComponent cam;
    cam.fieldOfViewDeg = 45.0f;
    cam.orthoHalfHeight = 3.5f;
    cam.zNear = 0.05f;
    cam.zFar = 250.0f;
    cam.primary = true;
    world.add(e, cam);

    return e;
}
}  // namespace

TEST(roundtrip_preserves_every_component_field) {
    ecs::Registry src;
    makeRichEntity(src, "Hero | with | pipes");

    // Second, minimal entity (transform only) to prove flags differ per row.
    const ecs::Entity minimal = src.create();
    TransformComponent bare;
    bare.position = {-10.0f, 0.0f, 0.0f};
    src.add(minimal, bare);

    const auto blob = scene::SceneSerializer::serialize(src);
    CHECK(blob.size() > 64);

    ecs::Registry dst;
    std::string error;
    CHECK(scene::SceneSerializer::deserialize(dst, blob, &error));
    CHECK(error.empty());
    CHECK(dst.aliveCount() == 2);

    // Find the rich entity by its tag name.
    ecs::Entity found = ecs::kNullEntity;
    for (const ecs::Entity e : dst.aliveEntities()) {
        if (dst.has<TagComponent>(e) &&
            dst.get<TagComponent>(e).name == "Hero | with | pipes") {
            found = e;
        }
    }
    CHECK(found != ecs::kNullEntity);
    if (found == ecs::kNullEntity) return;   // fail fast, avoid noise below

    const auto& tf = dst.get<TransformComponent>(found);
    CHECK_NEAR(tf.position.x, 1.25f, kEps);
    CHECK_NEAR(tf.position.y, -2.5f, kEps);
    CHECK_NEAR(tf.position.z, 3.75f, kEps);
    CHECK_NEAR(tf.rotation.y, -0.2f, kEps);
    CHECK_NEAR(tf.scale.x, 2.0f, kEps);
    CHECK(tf.parent == 0x00ABCDEFu);

    const auto& sp = dst.get<SpriteComponent>(found);
    CHECK_NEAR(sp.color.w, 0.8f, kEps);
    CHECK(sp.textureId == 0x00C0FFEEu);
    CHECK_NEAR(sp.size.y, 2.5f, kEps);
    CHECK(sp.renderLayer == 7);

    const auto& rb = dst.get<RigidBodyComponent>(found);
    CHECK_NEAR(rb.velocity.y, -9.8f, kEps);
    CHECK_NEAR(rb.gravityScale, 1.5f, kEps);
    CHECK_NEAR(rb.mass, 2.25f, kEps);
    CHECK(!rb.useGravity);
    CHECK(rb.isStatic);
    CHECK(rb.grounded);

    const auto& col = dst.get<BoxColliderComponent>(found);
    CHECK_NEAR(col.halfExtents.z, 0.8f, kEps);
    CHECK_NEAR(col.center.y, 0.2f, kEps);
    CHECK(col.isTrigger);

    const auto& sc = dst.get<ScriptComponent>(found);
    CHECK(sc.assetPath == "scripts/player.lua");
    CHECK(!sc.enabled);
    CHECK(sc.luaEnvRef == 42u);

    const auto& cam = dst.get<CameraComponent>(found);
    CHECK_NEAR(cam.fieldOfViewDeg, 45.0f, kEps);
    CHECK_NEAR(cam.zFar, 250.0f, kEps);
    CHECK(cam.primary);

    // Minimal entity: no sprite/script added by accident.
    int transformOnly = 0;
    ecs::view<TransformComponent>(
        dst, [&](ecs::Entity e, TransformComponent&) {
            if (!dst.has<TagComponent>(e)) {
                ++transformOnly;
                CHECK_NEAR(dst.get<TransformComponent>(e).position.x, -10.0f, kEps);
            }
        });
    CHECK(transformOnly == 1);
}

TEST(empty_scene_roundtrip) {
    ecs::Registry src;
    const auto blob = scene::SceneSerializer::serialize(src);
    CHECK(blob.size() == 16);   // header only: magic+version+count+reserved

    ecs::Registry dst;
    dst.create();               // destination has content that must be wiped
    dst.create();
    CHECK(scene::SceneSerializer::deserialize(dst, blob, nullptr));
    CHECK(dst.aliveCount() == 0);
}

TEST(rejects_bad_magic_and_clears_destination) {
    ecs::Registry world;
    world.create();
    world.create();
    CHECK(world.aliveCount() == 2);

    std::vector<std::uint8_t> junk(64, 0x42);
    std::string error;
    CHECK(!scene::SceneSerializer::deserialize(world, junk, &error));
    CHECK(!error.empty());
    // ALL-OR-NOTHING: a failed load never leaves a partial world behind.
    CHECK(world.aliveCount() == 0);
}

TEST(rejects_future_version) {
    ecs::Registry src;
    src.create();
    auto blob = scene::SceneSerializer::serialize(src);
    // Bump the version field (bytes 4..7) to 999.
    blob[4] = 999 % 256;
    blob[5] = 0;
    blob[6] = 0;
    blob[7] = 0;

    ecs::Registry dst;
    std::string error;
    CHECK(!scene::SceneSerializer::deserialize(dst, blob, &error));
    CHECK(error.find("not supported") != std::string::npos);
}

TEST(rejects_truncated_stream) {
    ecs::Registry src;
    makeRichEntity(src, "TruncationTester");
    auto blob = scene::SceneSerializer::serialize(src);
    CHECK(blob.size() > 100);

    blob.resize(blob.size() / 2);   // cut mid-entity
    ecs::Registry dst;
    std::string error;
    CHECK(!scene::SceneSerializer::deserialize(dst, blob, &error));
    CHECK(error.find("truncated") != std::string::npos);
    CHECK(dst.aliveCount() == 0);
}

TEST(rejects_empty_buffer) {
    ecs::Registry dst;
    dst.create();
    std::string error;
    const std::vector<std::uint8_t> nothing;
    CHECK(!scene::SceneSerializer::deserialize(dst, nothing, &error));
    CHECK(!error.empty());
    CHECK(dst.aliveCount() == 0);
}

TEST(entity_count_scales) {
    ecs::Registry src;
    for (int i = 0; i < 500; ++i) {
        const ecs::Entity e = src.create();
        TransformComponent tf;
        tf.position = {static_cast<float>(i), 0, 0};
        src.add(e, tf);
        if (i % 2 == 0) {
            SpriteComponent sp;
            sp.textureId = static_cast<std::uint32_t>(i + 1);
            src.add(e, sp);
        }
    }

    ecs::Registry dst;
    CHECK(scene::SceneSerializer::deserialize(
        dst, scene::SceneSerializer::serialize(src), nullptr));
    CHECK(dst.aliveCount() == 500);

    int sprites = 0;
    ecs::view<SpriteComponent>(
        dst, [&](ecs::Entity, SpriteComponent& sp) {
            CHECK(sp.textureId != 0);
            ++sprites;
        });
    CHECK(sprites == 250);
}

TEST(file_wrapper_roundtrip) {
    const char* path = "/tmp/anaconda_engine_test_scene.bin";

    ecs::Registry src;
    makeRichEntity(src, "FileEntity");
    std::string error;
    CHECK(scene::SceneSerializer::saveToFile(src, path, &error));
    CHECK(error.empty());

    ecs::Registry dst;
    CHECK(scene::SceneSerializer::loadFromFile(dst, path, &error));
    CHECK(dst.aliveCount() == 1);
    CHECK(dst.get<TagComponent>(dst.aliveEntities()[0]).name == "FileEntity");

    // Missing file → clean failure + cleared destination.
    CHECK(!scene::SceneSerializer::loadFromFile(
        dst, "/tmp/definitely_missing_scene_xyz.bin", &error));
    CHECK(!error.empty());
    CHECK(dst.aliveCount() == 0);

    std::remove(path);
}

TEST(snapshot_codec_roundtrip_restores_all_flags) {
    ecs::Registry src;
    const ecs::Entity rich = makeRichEntity(src, "Codec | Hero");

    const std::string line =
        scene::EditorSnapshotCodec::writeEntity(src, rich);
    CHECK(!line.empty());
    // 8 columns: id, name, pos, rot, scale, flags, color, texture
    CHECK(std::count(line.begin(), line.end(), '|') == 8); // 9 columns incl. layer

    // Parse into a DIFFERENT registry (simulates undo-of-delete).
    ecs::Registry dst;
    std::string error;
    const ecs::Entity restored =
        scene::EditorSnapshotCodec::parseEntity(dst, line, &error);
    CHECK(restored != ecs::kNullEntity);
    if (restored == ecs::kNullEntity) return;
    CHECK(error.empty());
    CHECK(dst.aliveCount() == 1);

    // New id minted (never reuses the source handle in the same world —
    // here cross-registry, but the parse ignores the line's id regardless).
    const auto& tf = dst.get<TransformComponent>(restored);
    CHECK_NEAR(tf.position.x, 1.25f, 1e-3f);   // protocol rounds to 4 decimals
    CHECK_NEAR(tf.scale.z, 1.5f, 1e-3f);

    // Flags rebuilt the optional components.
    CHECK(dst.has<TagComponent>(restored));
    // '|' inside names is SANITIZED to '/' by the writer (protocol safety).
    CHECK(dst.get<TagComponent>(restored).name == "Codec / Hero");
    CHECK(dst.has<SpriteComponent>(restored));
    CHECK(dst.has<RigidBodyComponent>(restored));
    CHECK(dst.has<BoxColliderComponent>(restored));
    CHECK(!dst.has<ScriptComponent>(restored));     // NOT in the protocol
    CHECK(!dst.has<CameraComponent>(restored));

    // Texture handle survived (asset assignment persists through undo).
    CHECK(dst.get<SpriteComponent>(restored).textureId == 0x00C0FFEEu);
    CHECK_NEAR(dst.get<SpriteComponent>(restored).color.w, 0.8f, 1e-3f);

    // Collider half-extents derived from scale (0.5 * |s|).
    CHECK_NEAR(dst.get<BoxColliderComponent>(restored).halfExtents.x, 1.0f,
               1e-3f);
}

TEST(snapshot_codec_rejects_malformed_lines) {
    ecs::Registry world;
    std::string error;

    CHECK(scene::EditorSnapshotCodec::parseEntity(world, "", &error) ==
          ecs::kNullEntity);
    CHECK(scene::EditorSnapshotCodec::parseEntity(
              world, "1|Name|1,2,3", &error) == ecs::kNullEntity);
    CHECK(scene::EditorSnapshotCodec::parseEntity(
              world, "1|Name|not_a_vec|0,0,0|1,1,1|0|1,1,1,1|0",
              &error) == ecs::kNullEntity);
    CHECK(world.aliveCount() == 0);   // failures mint nothing

    // Invalid source entity → empty line.
    CHECK(scene::EditorSnapshotCodec::writeEntity(world, ecs::kNullEntity)
          .empty());
}

int main() { return anaconda::test::runAll(); }
