// ===========================================================================
//  anaconda/renderer/DebugDraw.hpp — collider/grid/gizmo visualization (M3).
//
//  Builds line lists for debug overlay: AABB wireframes, grid lines, gizmo
//  axes. These are rendered as GL_LINES with a simple color shader (no
//  texture, no blending). The overlay is toggled via Engine::setDebugDraw().
//  Host-testable: line generation is pure math.
// ===========================================================================
#pragma once

#include "anaconda/ecs/Components.hpp"
#include "anaconda/ecs/Ecs.hpp"
#include "anaconda/math/Math.hpp"

#include <vector>

namespace anaconda::renderer {

struct DebugLine {
    Vec3 from, to;
    Vec4 color;
};

class DebugDraw {
public:
    void clear() { lines_.clear(); }
    void addLine(Vec3 a, Vec3 b, Vec4 color = {1, 0, 0, 1}) { lines_.push_back({a, b, color}); }

    // Collect collider wireframes for the whole world.
    void collectColliders(ecs::Registry& world, Vec4 color = {0, 1, 0, 0.9f});

    // Collect grid lines (delegates to editor/Grid.hpp helper, converts to DebugLine).
    void collectGrid(const GridSettings& grid, float extent = 20.0f, Vec4 color = {0.3f, 0.3f, 0.3f, 0.2f});

    const std::vector<DebugLine>& lines() const { return lines_; }
    bool empty() const { return lines_.empty(); }

    // For renderer stats.
    size_t lineCount() const { return lines_.size(); }

private:
    std::vector<DebugLine> lines_;
};

}  // namespace anaconda::renderer
