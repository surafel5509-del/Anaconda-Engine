// ===========================================================================
//  renderer/DebugDraw.cpp — collider/grid collection.
// ===========================================================================
#include "anaconda/renderer/DebugDraw.hpp"

#include "anaconda/physics/PhysicsSystem.hpp"

namespace anaconda::renderer {

void DebugDraw::collectColliders(ecs::Registry& world, Vec4 color) {
    ecs::view<TransformComponent, BoxColliderComponent>(
        world, [&](ecs::Entity e, TransformComponent& tf, BoxColliderComponent& col) {
            Aabb box;
            if (!PhysicsSystem::computeAabb(world, e, box)) return;
            // box already world-space
            Vec3 mn = box.min, mx = box.max;
            // bottom rect
            addLine({mn.x, mn.y, 0}, {mx.x, mn.y, 0}, color);
            addLine({mx.x, mn.y, 0}, {mx.x, mx.y, 0}, color);
            addLine({mx.x, mx.y, 0}, {mn.x, mx.y, 0}, color);
            addLine({mn.x, mx.y, 0}, {mn.x, mn.y, 0}, color);
            (void)tf; (void)col;
        });
}

void DebugDraw::collectGrid(const GridSettings& grid, float extent, Vec4 color) {
    if (!grid.enabled) return;
    int count = static_cast<int>(std::ceil(extent / grid.cellSize));
    for (int i = -count; i <= count; ++i) {
        float c = i * grid.cellSize;
        addLine({c, -extent, 0}, {c, extent, 0}, color);
        addLine({-extent, c, 0}, {extent, c, 0}, color);
    }
}

}  // namespace anaconda::renderer
