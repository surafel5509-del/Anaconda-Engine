// ============================================================================
//  scene/EditorSnapshotCodec.cpp — editor line protocol write/parse.
// ============================================================================
#include "anaconda/scene/EditorSnapshotCodec.hpp"

#include "anaconda/core/Log.hpp"
#include "anaconda/ecs/Components.hpp"

#include <cmath>
#include <cstdlib>
#include <sstream>

namespace anaconda::scene {

namespace {

constexpr int kFlagSprite = 1;
constexpr int kFlagRigidBody = 2;
constexpr int kFlagCollider = 4;

/// "1.5,-2,3.5" → Vec3. Strict: all three must parse.
bool parseVec3(const std::string& s, Vec3& out) {
    std::istringstream in(s);
    char c1 = 0, c2 = 0;
    if (!(in >> out.x >> c1 >> out.y >> c2 >> out.z)) return false;
    return c1 == ',' && c2 == ',';
}

bool parseVec4(const std::string& s, Vec4& out) {
    std::istringstream in(s);
    char c1 = 0, c2 = 0, c3 = 0;
    if (!(in >> out.x >> c1 >> out.y >> c2 >> out.z >> c3 >> out.w)) return false;
    return c1 == ',' && c2 == ',' && c3 == ',';
}

std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : s) {
        if (c == delim) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    parts.push_back(current);
    return parts;
}

}  // namespace

// ---------------------------------------------------------------------------
std::string EditorSnapshotCodec::writeEntity(const ecs::Registry& world,
                                             ecs::Entity entity) {
    if (!world.valid(entity)) return {};

    const auto* tf = world.tryGet<TransformComponent>(entity);
    if (tf == nullptr) return {};   // protocol requires a transform

    const auto* tag = world.tryGet<TagComponent>(entity);
    const auto* sprite = world.tryGet<SpriteComponent>(entity);
    const auto* rb = world.tryGet<RigidBodyComponent>(entity);
    const auto* col = world.tryGet<BoxColliderComponent>(entity);

    int flags = 0;
    if (sprite != nullptr) flags |= kFlagSprite;
    if (rb != nullptr) flags |= kFlagRigidBody;
    if (col != nullptr) flags |= kFlagCollider;

    std::string name = tag != nullptr ? tag->name : "Entity";
    for (char& c : name) {
        if (c == '|' || c == '\n' || c == '\r') c = '/';
    }

    std::ostringstream out;
    out.precision(4);
    out << std::fixed;
    out << static_cast<std::uint32_t>(entity) << '|' << name << '|'
        << tf->position.x << ',' << tf->position.y << ',' << tf->position.z
        << '|'
        << tf->rotation.x << ',' << tf->rotation.y << ',' << tf->rotation.z
        << '|'
        << tf->scale.x << ',' << tf->scale.y << ',' << tf->scale.z << '|'
        << flags << '|';
    if (sprite != nullptr) {
        out << sprite->color.x << ',' << sprite->color.y << ','
            << sprite->color.z << ',' << sprite->color.w;
    } else {
        out << "1,1,1,1";
    }
    out << '|' << (sprite != nullptr ? sprite->textureId : 0u);
    out << '|' << (sprite != nullptr ? sprite->renderLayer : 0);
    // Month 3-4 extended: uvRect | physics | anim | script
    Vec4 uv{0,0,1,1}; if(sprite) uv=sprite->uvRect;
    out << '|' << uv.x << ',' << uv.y << ',' << uv.z << ',' << uv.w;
    float friction=0.2f, restitution=0, damping=0; if(rb){ friction=rb->friction; restitution=rb->restitution; damping=rb->linearDamping; } else if(col){ friction=col->friction; restitution=col->restitution; }
    out << '|' << friction << ',' << restitution << ',' << damping;
    std::string animClip; if(world.tryGet<SpriteAnimatorComponent>(entity)) animClip=world.tryGet<SpriteAnimatorComponent>(entity)->currentClip;
    out << '|' << animClip;
    std::string scriptPath; if(world.tryGet<ScriptComponent>(entity)) scriptPath=world.tryGet<ScriptComponent>(entity)->assetPath;
    out << '|' << scriptPath;
    return out.str();
}

// ---------------------------------------------------------------------------
ecs::Entity EditorSnapshotCodec::parseEntity(ecs::Registry& world,
                                             const std::string& line,
                                             std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error != nullptr) *error = why;
        ANA_LOGW("[Snapshot] parseEntity failed: %s", why.c_str());
        return ecs::kNullEntity;
    };

    const std::vector<std::string> p = split(line, '|');
    if (p.size() < 8) {
        return fail("expected 8 columns, got " + std::to_string(p.size()));
    }

    // Column 0 (id) intentionally ignored — new handle minted below.
    Vec3 position, rotation, scale;
    Vec4 color{1, 1, 1, 1};
    if (!parseVec3(p[2], position)) return fail("bad position column");
    if (!parseVec3(p[3], rotation)) return fail("bad rotation column");
    if (!parseVec3(p[4], scale)) return fail("bad scale column");

    char* end = nullptr;
    const long flags = std::strtol(p[5].c_str(), &end, 10);
    if (end == nullptr || *end != '\0') return fail("bad flags column");
    if (!parseVec4(p[6], color)) return fail("bad color column");

    std::uint32_t textureHandle = 0;
    {
        const char* t = p[7].c_str();
        char* tend = nullptr;
        const unsigned long th = std::strtoul(t, &tend, 10);
        if (tend == t || (tend != nullptr && *tend != '\0')) {
            return fail("bad texture column");
        }
        textureHandle = static_cast<std::uint32_t>(th);
    }

    int renderLayer = 0;
    if (p.size() >= 9) {
        char* rend = nullptr;
        const long rl = std::strtol(p[8].c_str(), &rend, 10);
        if (rend == nullptr || *rend != '\0') return fail("bad layer column");
        renderLayer = static_cast<int>(rl);
    }
    Vec4 uvRect{0,0,1,1};
    if (p.size() >= 10) { Vec4 uv; if(parseVec4(p[9], uv)) uvRect=uv; }
    Vec3 phys{0.2f,0,0}; // friction, restitution, damping
    if (p.size() >= 11) {
        std::istringstream pin(p[10]); char c1=0,c2=0;
        float f=0,r=0,d=0; if(pin>>f>>c1>>r>>c2>>d) phys={f,r,d};
    }
    std::string animClip = p.size() >= 12 ? p[11] : "";
    std::string scriptPath = p.size() >= 13 ? p[12] : "";

    const ecs::Entity e = world.create();

    TagComponent tag;
    tag.name = p[1].empty() ? "Entity" : p[1];
    world.add(e, std::move(tag));

    TransformComponent tf;
    tf.position = position;
    tf.rotation = rotation;
    tf.scale = scale;
    world.add(e, tf);

    if (flags & kFlagSprite) {
        SpriteComponent sp;
        sp.color = color;
        sp.textureId = textureHandle;
        sp.renderLayer = renderLayer;
        sp.uvRect = uvRect;
        world.add(e, sp);
    }
    if (flags & kFlagRigidBody) {
        RigidBodyComponent rb;
        rb.friction = phys.x; rb.restitution = phys.y; rb.linearDamping = phys.z;
        world.add(e, rb);
    }
    if (flags & kFlagCollider) {
        BoxColliderComponent col;
        col.halfExtents = {0.5f * std::abs(scale.x), 0.5f * std::abs(scale.y),
                           0.5f * std::abs(scale.z)};
        col.friction = phys.x; col.restitution = phys.y;
        world.add(e, col);
    }
    if (!animClip.empty()) {
        SpriteAnimatorComponent an; an.currentClip = animClip; world.add(e, an);
    }
    if (!scriptPath.empty()) {
        ScriptComponent sc; sc.assetPath = scriptPath; world.add(e, std::move(sc));
    }

    if (error != nullptr) error->clear();
    return e;
}

}  // namespace anaconda::scene
