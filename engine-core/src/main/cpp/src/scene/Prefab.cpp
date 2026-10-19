// ===========================================================================
//  scene/Prefab.cpp — prefab save/instantiate (binary blob).
// ===========================================================================
#include "anaconda/scene/Prefab.hpp"

#include "anaconda/ecs/Components.hpp"
#include "anaconda/scene/SceneSerializer.hpp"

#include <fstream>

namespace anaconda::scene {

std::vector<uint8_t> Prefab::saveToBlob(ecs::Registry& world, ecs::Entity root, bool withChildren) {
    // Collect root + descendants
    std::vector<ecs::Entity> toSave;
    toSave.push_back(root);
    if (withChildren) {
        // Simple gather: any entity whose parent chain leads to root
        auto all = world.aliveEntities();
        for (ecs::Entity e : all) {
            if (e == root) continue;
            auto* tf = world.tryGet<TransformComponent>(e);
            if (tf == nullptr) continue;
            ecs::Entity cur = e;
            int depth = 0;
            while (cur != ecs::kNullEntity && depth < 32) {
                auto* t = world.tryGet<TransformComponent>(cur);
                if (t == nullptr) break;
                if (t->parent == static_cast<std::uint32_t>(root)) {
                    toSave.push_back(e);
                    break;
                }
                if (t->parent == 0xFFFFFFFFu) break;
                cur = static_cast<ecs::Entity>(t->parent);
                ++depth;
            }
        }
    }
    ecs::Registry tmp;
    // Clone to temp registry for serialization (preserve IDs via copy)
    for (ecs::Entity e : toSave) {
        ecs::Entity dst = tmp.create();
        if (auto* tag = world.tryGet<TagComponent>(e)) tmp.add(dst, *tag);
        if (auto* tf = world.tryGet<TransformComponent>(e)) tmp.add(dst, *tf);
        if (auto* sp = world.tryGet<SpriteComponent>(e)) tmp.add(dst, *sp);
        if (auto* anim = world.tryGet<SpriteAnimatorComponent>(e)) tmp.add(dst, *anim);
        if (auto* rb = world.tryGet<RigidBodyComponent>(e)) tmp.add(dst, *rb);
        if (auto* col = world.tryGet<BoxColliderComponent>(e)) tmp.add(dst, *col);
        if (auto* mat = world.tryGet<PhysicsMaterialComponent>(e)) tmp.add(dst, *mat);
        if (auto* aud = world.tryGet<AudioSourceComponent>(e)) tmp.add(dst, *aud);
        if (auto* sc = world.tryGet<ScriptComponent>(e)) tmp.add(dst, *sc);
        if (auto* pref = world.tryGet<PrefabComponent>(e)) tmp.add(dst, *pref);
        if (auto* cam = world.tryGet<CameraComponent>(e)) tmp.add(dst, *cam);
        (void)dst;
    }
    return SceneSerializer::serialize(tmp);
}

bool Prefab::saveToFile(ecs::Registry& world, ecs::Entity root, const std::string& path, std::string* error) {
    auto blob = saveToBlob(world, root, true);
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        if (error) *error = "cannot open prefab file for writing: " + path;
        return false;
    }
    out.write(reinterpret_cast<const char*>(blob.data()), blob.size());
    return out.good();
}

ecs::Entity Prefab::instantiateFromBlob(ecs::Registry& world, const std::vector<uint8_t>& blob,
                                         const Vec3& offset, std::string* error) {
    ecs::Registry tmp;
    if (!SceneSerializer::deserialize(tmp, blob, error)) return ecs::kNullEntity;
    auto entities = tmp.aliveEntities();
    if (entities.empty()) return ecs::kNullEntity;

    std::unordered_map<ecs::Entity, ecs::Entity> remap;
    ecs::Entity newRoot = ecs::kNullEntity;
    for (ecs::Entity src : entities) {
        ecs::Entity dst = world.create();
        remap[src] = dst;
        if (newRoot == ecs::kNullEntity) newRoot = dst;
        if (auto* tag = tmp.tryGet<TagComponent>(src)) world.add(dst, *tag);
        if (auto* tf = tmp.tryGet<TransformComponent>(src)) {
            TransformComponent c = *tf;
            c.position = c.position + offset;
            world.add(dst, std::move(c));
        }
        if (auto* sp = tmp.tryGet<SpriteComponent>(src)) world.add(dst, *sp);
        if (auto* anim = tmp.tryGet<SpriteAnimatorComponent>(src)) world.add(dst, *anim);
        if (auto* rb = tmp.tryGet<RigidBodyComponent>(src)) world.add(dst, *rb);
        if (auto* col = tmp.tryGet<BoxColliderComponent>(src)) world.add(dst, *col);
        if (auto* mat = tmp.tryGet<PhysicsMaterialComponent>(src)) world.add(dst, *mat);
        if (auto* aud = tmp.tryGet<AudioSourceComponent>(src)) world.add(dst, *aud);
        if (auto* sc = tmp.tryGet<ScriptComponent>(src)) world.add(dst, *sc);
        if (auto* cam = tmp.tryGet<CameraComponent>(src)) world.add(dst, *cam);
        // Fix parent remap: if old parent was in the prefab, point to new id
        if (auto* tf2 = world.tryGet<TransformComponent>(dst)) {
            auto it = remap.find(static_cast<ecs::Entity>(tf2->parent));
            if (it != remap.end()) tf2->parent = static_cast<std::uint32_t>(it->second);
        }
    }
    return newRoot;
}

ecs::Entity Prefab::instantiateFromFile(ecs::Registry& world, const std::string& path,
                                         const Vec3& offset, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "cannot open prefab file: " + path;
        return ecs::kNullEntity;
    }
    std::vector<uint8_t> blob((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return instantiateFromBlob(world, blob, offset, error);
}

void Prefab::setOverride(ecs::Entity instance, const std::string& field, const std::string& value) {
    // No-op placeholder — real override tracking lives in PrefabComponent map
    (void)instance; (void)field; (void)value;
}
bool Prefab::hasOverride(ecs::Entity instance, const std::string& field) {
    (void)instance; (void)field;
    return false;
}

}  // namespace anaconda::scene
