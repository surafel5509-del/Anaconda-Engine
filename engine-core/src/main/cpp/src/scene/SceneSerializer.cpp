// ============================================================================
//  scene/SceneSerializer.cpp — encode/decode implementation (host-testable).
// ============================================================================
#include "anaconda/scene/SceneSerializer.hpp"

#include "anaconda/core/Log.hpp"
#include "anaconda/ecs/Components.hpp"

#include <cstdio>
#include <cstring>

namespace anaconda::scene {

namespace {

// ---------------------------------------------------------------------------
// Bounds-checked little-endian cursor writers/readers.
// ---------------------------------------------------------------------------
struct Writer {
    std::vector<std::uint8_t> out;

    void u32(std::uint32_t v) {
        out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFFu));
    }
    void i32(std::int32_t v) { u32(static_cast<std::uint32_t>(v)); }
    void f32(float v) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        u32(bits);
    }
    void u8(std::uint8_t v) { out.push_back(v); }
    void str(const std::string& s) {
        u32(static_cast<std::uint32_t>(s.size()));
        out.insert(out.end(), s.begin(), s.end());
    }
    void vec3(const Vec3& v) { f32(v.x); f32(v.y); f32(v.z); }
    void vec4(const Vec4& v) { f32(v.x); f32(v.y); f32(v.z); f32(v.w); }
};

struct Reader {
    const std::vector<std::uint8_t>& in;
    std::size_t pos = 0;
    std::string err;

    bool fail(const std::string& why) {
        if (err.empty()) err = why;
        return false;
    }

    bool need(std::size_t bytes) {
        if (err.empty() == false) return false;
        if (pos + bytes > in.size()) {
            return fail("truncated scene data (need " + std::to_string(bytes) +
                        " bytes at offset " + std::to_string(pos) + " of " +
                        std::to_string(in.size()) + ")");
        }
        return true;
    }

    bool u32(std::uint32_t& v) {
        if (!need(4)) return false;
        v = static_cast<std::uint32_t>(in[pos]) |
            (static_cast<std::uint32_t>(in[pos + 1]) << 8) |
            (static_cast<std::uint32_t>(in[pos + 2]) << 16) |
            (static_cast<std::uint32_t>(in[pos + 3]) << 24);
        pos += 4;
        return true;
    }
    bool i32(std::int32_t& v) {
        std::uint32_t raw = 0;
        if (!u32(raw)) return false;
        std::memcpy(&v, &raw, sizeof(v));
        return true;
    }
    bool f32(float& v) {
        std::uint32_t bits = 0;
        if (!u32(bits)) return false;
        std::memcpy(&v, &bits, sizeof(v));
        return true;
    }
    bool u8(std::uint8_t& v) {
        if (!need(1)) return false;
        v = in[pos++];
        return true;
    }
    bool str(std::string& s) {
        std::uint32_t len = 0;
        if (!u32(len)) return false;
        if (len > SceneSerializer::kMaxStringBytes) {
            return fail("string length " + std::to_string(len) +
                        " exceeds limit");
        }
        if (!need(len)) return false;
        s.assign(reinterpret_cast<const char*>(in.data() + pos), len);
        pos += len;
        return true;
    }
    bool vec3(Vec3& v) { return f32(v.x) && f32(v.y) && f32(v.z); }
    bool vec4(Vec4& v) {
        return f32(v.x) && f32(v.y) && f32(v.z) && f32(v.w);
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// serialize
// ---------------------------------------------------------------------------
std::vector<std::uint8_t> SceneSerializer::serialize(const ecs::Registry& world) {
    Writer w;
    w.u32(kMagic);
    w.u32(kVersion);

    const std::vector<ecs::Entity> entities = world.aliveEntities();
    w.u32(static_cast<std::uint32_t>(entities.size()));
    w.u32(0);   // reserved for future header fields (flags, AABB, …)

    for (const ecs::Entity e : entities) {
        const auto* tag = world.tryGet<TagComponent>(e);
        const auto* tf = world.tryGet<TransformComponent>(e);
        const auto* sprite = world.tryGet<SpriteComponent>(e);
        const auto* rb = world.tryGet<RigidBodyComponent>(e);
        const auto* col = world.tryGet<BoxColliderComponent>(e);
        const auto* script = world.tryGet<ScriptComponent>(e);
        const auto* cam = world.tryGet<CameraComponent>(e);

        std::uint32_t flags = 0;
        if (tag) flags |= kHasTag;
        if (tf) flags |= kHasTransform;
        if (sprite) flags |= kHasSprite;
        if (rb) flags |= kHasRigidBody;
        if (col) flags |= kHasCollider;
        if (script) flags |= kHasScript;
        if (cam) flags |= kHasCamera;
        w.u32(flags);

        if (tag) w.str(tag->name);
        if (tf) {
            w.vec3(tf->position);
            w.vec3(tf->rotation);
            w.vec3(tf->scale);
            w.u32(tf->parent);   // kNullEntity for roots (Phase-3 hierarchy)
        }
        if (sprite) {
            w.vec4(sprite->color);
            w.u32(sprite->textureId);   // asset handle (AssetManager indirection)
            w.f32(sprite->size.x);
            w.f32(sprite->size.y);
            w.i32(sprite->renderLayer);
        }
        if (rb) {
            w.vec3(rb->velocity);
            w.f32(rb->gravityScale);
            w.f32(rb->mass);
            w.u8(rb->useGravity ? 1 : 0);
            w.u8(rb->isStatic ? 1 : 0);
            w.u8(rb->grounded ? 1 : 0);   // restored, but harmless on load
            w.u8(0);                      // padding (keep field groups 4-aligned)
        }
        if (col) {
            w.vec3(col->halfExtents);
            w.vec3(col->center);
            w.u8(col->isTrigger ? 1 : 0);
            w.u8(0);
            w.u8(0);
            w.u8(0);
        }
        if (script) {
            w.str(script->assetPath);
            w.u8(script->enabled ? 1 : 0);
            w.u8(0);
            w.u8(0);
            w.u8(0);
            w.u32(script->luaEnvRef);   // LUA_NOREF until Phase 4 binds it
        }
        if (cam) {
            w.f32(cam->fieldOfViewDeg);
            w.f32(cam->orthoHalfHeight);
            w.f32(cam->zNear);
            w.f32(cam->zFar);
            w.u8(cam->primary ? 1 : 0);
            w.u8(0);
            w.u8(0);
            w.u8(0);
        }
    }
    return std::move(w.out);
}

// ---------------------------------------------------------------------------
// deserialize
// ---------------------------------------------------------------------------
bool SceneSerializer::deserialize(ecs::Registry& world,
                                  const std::vector<std::uint8_t>& data,
                                  std::string* error) {
    auto setError = [&](const std::string& why) {
        if (error != nullptr) *error = why;
        // ALL-OR-NOTHING: clear on ANY failure, including mid-parse — a
        // half-loaded scene must never be observable by the editor.
        world.clear();
        ANA_LOGE("[Scene] deserialize failed: %s", why.c_str());
        return false;
    };

    world.clear();   // fresh destination before the header is even read

    Reader r{data, 0, {}};
    std::uint32_t magic = 0, version = 0, count = 0, reserved = 0;
    if (!r.u32(magic)) return setError(r.err.empty() ? "empty scene file" : r.err);
    if (magic != kMagic) {
        char hex[16] = {0};
        std::snprintf(hex, sizeof(hex), "0x%08X", magic);
        return setError(std::string("not an Anaconda scene (bad magic ") +
                        hex + ")");
    }
    if (!r.u32(version)) return setError(r.err);
    if (version != kVersion) {
        return setError("scene format v" + std::to_string(version) +
                        " is not supported (engine reads v" +
                        std::to_string(kVersion) + ")");
    }
    if (!r.u32(count)) return setError(r.err);
    if (!r.u32(reserved)) return setError(r.err);
    if (count > kMaxEntities) {
        return setError("entity count " + std::to_string(count) +
                        " exceeds safety limit");
    }

    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t flags = 0;
        if (!r.u32(flags)) return setError(r.err);

        // A file with NO recognizable component bits is corrupt — entities
        // must at least carry something (an empty flags word is 1-in-4B per
        // entity only if malicious; treat as corrupt).
        const std::uint32_t known = kHasTag | kHasTransform | kHasSprite |
                                    kHasRigidBody | kHasCollider |
                                    kHasScript | kHasCamera;
        if ((flags & known) == 0) {
            return setError("entity " + std::to_string(i) +
                            " has no component flags (corrupt stream)");
        }

        const ecs::Entity e = world.create();

        if (flags & kHasTag) {
            TagComponent tag;
            if (!r.str(tag.name)) return setError(r.err);
            world.add(e, std::move(tag));
        }
        if (flags & kHasTransform) {
            TransformComponent tf;
            if (!r.vec3(tf.position) || !r.vec3(tf.rotation) ||
                !r.vec3(tf.scale) || !r.u32(tf.parent)) {
                return setError(r.err);
            }
            world.add(e, tf);
        }
        if (flags & kHasSprite) {
            SpriteComponent sp;
            if (!r.vec4(sp.color) || !r.u32(sp.textureId) ||
                !r.f32(sp.size.x) || !r.f32(sp.size.y) ||
                !r.i32(sp.renderLayer)) {
                return setError(r.err);
            }
            world.add(e, sp);
        }
        if (flags & kHasRigidBody) {
            RigidBodyComponent rb;
            std::uint8_t b0 = 0, b1 = 0, b2 = 0, pad = 0;
            if (!r.vec3(rb.velocity) || !r.f32(rb.gravityScale) ||
                !r.f32(rb.mass) || !r.u8(b0) || !r.u8(b1) || !r.u8(b2) ||
                !r.u8(pad)) {
                return setError(r.err);
            }
            rb.useGravity = b0 != 0;
            rb.isStatic = b1 != 0;
            rb.grounded = b2 != 0;
            world.add(e, rb);
        }
        if (flags & kHasCollider) {
            BoxColliderComponent col;
            std::uint8_t b0 = 0, p1 = 0, p2 = 0, p3 = 0;
            if (!r.vec3(col.halfExtents) || !r.vec3(col.center) ||
                !r.u8(b0) || !r.u8(p1) || !r.u8(p2) || !r.u8(p3)) {
                return setError(r.err);
            }
            col.isTrigger = b0 != 0;
            world.add(e, col);
        }
        if (flags & kHasScript) {
            ScriptComponent sc;
            std::uint8_t b0 = 0, p1 = 0, p2 = 0, p3 = 0;
            if (!r.str(sc.assetPath) || !r.u8(b0) || !r.u8(p1) || !r.u8(p2) ||
                !r.u8(p3) || !r.u32(sc.luaEnvRef)) {
                return setError(r.err);
            }
            sc.enabled = b0 != 0;
            world.add(e, std::move(sc));
        }
        if (flags & kHasCamera) {
            CameraComponent cam;
            std::uint8_t b0 = 0, p1 = 0, p2 = 0, p3 = 0;
            if (!r.f32(cam.fieldOfViewDeg) || !r.f32(cam.orthoHalfHeight) ||
                !r.f32(cam.zNear) || !r.f32(cam.zFar) || !r.u8(b0) ||
                !r.u8(p1) || !r.u8(p2) || !r.u8(p3)) {
                return setError(r.err);
            }
            cam.primary = b0 != 0;
            world.add(e, cam);
        }
    }

    if (error != nullptr) error->clear();
    ANA_LOGI("[Scene] loaded %u entities (%zu bytes)", count, data.size());
    return true;
}

// ---------------------------------------------------------------------------
// File wrappers (C stdio — no iostream bloat in the NDK build)
// ---------------------------------------------------------------------------
bool SceneSerializer::saveToFile(const ecs::Registry& world,
                                 const std::string& path,
                                 std::string* error) {
    const std::vector<std::uint8_t> blob = serialize(world);
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) {
        if (error != nullptr) *error = "cannot open for writing: " + path;
        return false;
    }
    const std::size_t written =
        std::fwrite(blob.data(), 1, blob.size(), f);
    const bool ok = (written == blob.size()) && (std::fclose(f) == 0);
    if (!ok && error != nullptr) *error = "short write: " + path;
    return ok;
}

bool SceneSerializer::loadFromFile(ecs::Registry& world,
                                   const std::string& path,
                                   std::string* error) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) {
        world.clear();
        if (error != nullptr) *error = "cannot open: " + path;
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0 || size > 64L * 1024L * 1024L) {   // 64 MB sanity cap
        std::fclose(f);
        world.clear();
        if (error != nullptr) *error = "scene file size invalid";
        return false;
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
    const std::size_t read = size > 0 ? std::fread(data.data(), 1, data.size(), f) : 0;
    std::fclose(f);
    if (read != data.size()) {
        world.clear();
        if (error != nullptr) *error = "short read: " + path;
        return false;
    }
    return deserialize(world, data, error);
}

}  // namespace anaconda::scene
