// ===========================================================================
//  anaconda/scene/Picking.hpp — host-testable picking & duplicate helpers.
//  Used by Engine::pickEntity/duplicateEntity on Android and by host tests.
//  No GL dependency.
// ===========================================================================
#pragma once

#include "anaconda/ecs/Components.hpp"
#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/renderer/Camera.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace anaconda::scene {

// ---------------------------------------------------------------------------
// Duplicate — deep copy of all components, name gets " Copy" suffix, position
// nudged by (0.5, 0.5) so the new entity is visible.
// Returns kNullEntity if source is invalid.
// ---------------------------------------------------------------------------
inline ecs::Entity duplicateEntity(ecs::Registry& world, ecs::Entity src) {
    if (!world.valid(src)) return ecs::kNullEntity;
    const ecs::Entity dst = world.create();
    if (auto* tag = world.tryGet<TagComponent>(src)) {
        TagComponent copy = *tag;
        copy.name += " Copy";
        world.add(dst, std::move(copy));
    }
    if (auto* tf = world.tryGet<TransformComponent>(src)) {
        TransformComponent copy = *tf;
        copy.position.x += 0.5f;
        copy.position.y += 0.5f;
        world.add(dst, std::move(copy));
    }
    if (auto* sp = world.tryGet<SpriteComponent>(src)) world.add(dst, *sp);
    if (auto* rb = world.tryGet<RigidBodyComponent>(src)) world.add(dst, *rb);
    if (auto* col = world.tryGet<BoxColliderComponent>(src)) world.add(dst, *col);
    if (auto* sc = world.tryGet<ScriptComponent>(src)) world.add(dst, *sc);
    if (auto* cam = world.tryGet<CameraComponent>(src)) world.add(dst, *cam);
    return dst;
}

// ---------------------------------------------------------------------------
// Sorted draw order — returns entities with (Transform+Sprite) sorted by
// renderLayer ascending (painter's algorithm). Host-testable without GL.
// ---------------------------------------------------------------------------
inline std::vector<ecs::Entity> sortedDrawOrder(ecs::Registry& world) {
    std::vector<std::pair<ecs::Entity, int>> items;
    ecs::view<TransformComponent, SpriteComponent>(
        world, [&](ecs::Entity e, TransformComponent&, SpriteComponent& sp) {
            items.emplace_back(e, sp.renderLayer);
        });
    std::sort(items.begin(), items.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    std::vector<ecs::Entity> out;
    out.reserve(items.size());
    for (auto& p : items) out.push_back(p.first);
    return out;
}

// ---------------------------------------------------------------------------
// Picking — CPU point-in-AABB. Returns the topmost entity (highest layer,
// tie broken by closest center) whose bounds contain the world point derived
// from the screen point via Camera::screenToWorld.
// Bounds: collider AABB if present (scaled), else sprite quad (0.5 * scale * size).
// ---------------------------------------------------------------------------
inline ecs::Entity pickEntityAt(ecs::Registry& world, const Camera& camera,
                                int viewportWidth, int viewportHeight,
                                float screenX, float screenY) {
    if (viewportWidth <= 0 || viewportHeight <= 0) return ecs::kNullEntity;
    const Vec2 worldPt = camera.screenToWorld(screenX, screenY, viewportWidth, viewportHeight);

    ecs::Entity best = ecs::kNullEntity;
    int bestLayer = -1000000;
    float bestDist2 = 1e30f;

    for (const ecs::Entity e : world.aliveEntities()) {
        const auto* tf = world.tryGet<TransformComponent>(e);
        if (tf == nullptr) continue;

        Vec3 half{0.5f, 0.5f, 0.5f};
        int layer = -1;
        if (auto* col = world.tryGet<BoxColliderComponent>(e)) {
            half = col->halfExtents;
            half.x *= std::abs(tf->scale.x);
            half.y *= std::abs(tf->scale.y);
            half.z *= std::abs(tf->scale.z);
            if (auto* sp = world.tryGet<SpriteComponent>(e)) layer = sp->renderLayer;
        } else if (auto* sp = world.tryGet<SpriteComponent>(e)) {
            half.x = 0.5f * std::abs(tf->scale.x) * std::abs(sp->size.x);
            half.y = 0.5f * std::abs(tf->scale.y) * std::abs(sp->size.y);
            half.z = 0.5f * std::abs(tf->scale.z);
            layer = sp->renderLayer;
        } else {
            continue;
        }

        const auto* col = world.tryGet<BoxColliderComponent>(e);
        const float cx = col ? col->center.x * std::abs(tf->scale.x) : 0.0f;
        const float cy = col ? col->center.y * std::abs(tf->scale.y) : 0.0f;
        const float minX = tf->position.x - half.x + cx;
        const float maxX = tf->position.x + half.x + cx;
        const float minY = tf->position.y - half.y + cy;
        const float maxY = tf->position.y + half.y + cy;

        if (worldPt.x < minX || worldPt.x > maxX || worldPt.y < minY || worldPt.y > maxY) continue;

        const float dx = worldPt.x - tf->position.x;
        const float dy = worldPt.y - tf->position.y;
        const float dist2 = dx * dx + dy * dy;

        if (layer > bestLayer || (layer == bestLayer && dist2 < bestDist2)) {
            best = e;
            bestLayer = layer;
            bestDist2 = dist2;
        }
    }
    return best;
}

}  // namespace anaconda::scene
