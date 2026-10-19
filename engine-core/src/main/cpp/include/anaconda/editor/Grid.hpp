// ===========================================================================
//  anaconda/editor/Grid.hpp — editor grid & snapping (Month 3).
//
//  Pure math, header-only, host-testable. Grid is infinite in XY, drawn as
//  lines at cellSize intervals. Snap rounds a world position to the nearest
//  grid point when snapEnabled. Used by Engine during transform edits and
//  gizmo drags.
// ===========================================================================
#pragma once

#include "anaconda/ecs/Components.hpp"
#include "anaconda/math/Math.hpp"

namespace anaconda {

inline Vec3 snapToGrid(const Vec3& pos, const GridSettings& grid) {
    if (!grid.snapEnabled || grid.cellSize <= 0.001f) return pos;
    auto snap = [&](float v) {
        return std::round(v / grid.cellSize) * grid.cellSize;
    };
    return {snap(pos.x), snap(pos.y), snap(pos.z)};
}

inline Vec2 snapToGrid2(const Vec2& pos, const GridSettings& grid) {
    if (!grid.snapEnabled || grid.cellSize <= 0.001f) return pos;
    return {std::round(pos.x / grid.cellSize) * grid.cellSize,
            std::round(pos.y / grid.cellSize) * grid.cellSize};
}

// Grid line generation for debug draw (returns line segments as Vec3 pairs).
// Caller decides viewport culling; we generate lines for a fixed extent.
inline std::vector<std::pair<Vec3, Vec3>> generateGridLines(const GridSettings& grid,
                                                              float extent = 20.0f) {
    std::vector<std::pair<Vec3, Vec3>> lines;
    if (!grid.enabled || grid.cellSize <= 0.001f) return lines;
    int count = static_cast<int>(std::ceil(extent / grid.cellSize));
    for (int i = -count; i <= count; ++i) {
        float c = i * grid.cellSize;
        lines.emplace_back(Vec3{c, -extent, 0}, Vec3{c, extent, 0});
        lines.emplace_back(Vec3{-extent, c, 0}, Vec3{extent, c, 0});
    }
    return lines;
}

}  // namespace anaconda
