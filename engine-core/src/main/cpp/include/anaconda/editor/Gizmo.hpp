// ===========================================================================
//  anaconda/editor/Gizmo.hpp — screen-space gizmo (Month 3).
//
//  Translate/rotate/scale handles for touch. Hit test is in screen pixels:
//  handles are drawn at the selected entity's world position projected to
//  screen, with hit radius 18px (X/Y axes + center). Drag converts pixel
//  delta back to world via Camera::panPixels inverse. All math is host-
//  testable, no GL.
// ===========================================================================
#pragma once

#include "anaconda/ecs/Components.hpp"
#include "anaconda/editor/Grid.hpp"
#include "anaconda/math/Math.hpp"
#include "anaconda/renderer/Camera.hpp"

#include <cmath>

namespace anaconda {

struct GizmoState {
    GizmoMode mode = GizmoMode::Translate;
    GizmoHandle active = GizmoHandle::None;
    bool dragging = false;
    Vec2 dragStartScreen{0, 0};
    Vec3 dragStartWorld{0, 0, 0};
    Vec3 originalPosition{0, 0, 0};
    ecs::Entity target = ecs::kNullEntity;
};

inline float screenDist(Vec2 a, Vec2 b) {
    float dx = a.x - b.x, dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

// Project world position to screen (orthographic). Inverse of Camera::screenToWorld.
inline Vec2 worldToScreen(const Vec3& worldPos, const Camera& cam,
                          int viewportWidth, int viewportHeight) {
    float worldPerPixel = (2.0f * cam.orthoHalfHeight) / static_cast<float>(viewportHeight);
    if (worldPerPixel <= 0.0001f) return {viewportWidth * 0.5f, viewportHeight * 0.5f};
    float sx = (worldPos.x - cam.target.x) / worldPerPixel + viewportWidth * 0.5f;
    float sy = viewportHeight * 0.5f - (worldPos.y - cam.target.y) / worldPerPixel;
    return {sx, sy};
}

// Hit test gizmo handles at screen point. Returns handle or None.
inline GizmoHandle hitTestGizmo(Vec2 screenPoint, const Vec3& entityWorldPos,
                                const Camera& cam, int vpW, int vpH,
                                GizmoMode mode, float hitRadiusPx = 18.0f) {
    if (mode == GizmoMode::None) return GizmoHandle::None;
    Vec2 center = worldToScreen(entityWorldPos, cam, vpW, vpH);
    if (screenDist(screenPoint, center) <= hitRadiusPx) return GizmoHandle::Center;

    // Axis handles offset 32px in screen space for visibility.
    Vec2 xHandle{center.x + 32, center.y};
    Vec2 yHandle{center.x, center.y - 32};
    if (screenDist(screenPoint, xHandle) <= hitRadiusPx) return GizmoHandle::X;
    if (screenDist(screenPoint, yHandle) <= hitRadiusPx) return GizmoHandle::Y;

    // Corner for XY planar drag
    Vec2 xyHandle{center.x + 22, center.y - 22};
    if (mode == GizmoMode::Translate && screenDist(screenPoint, xyHandle) <= hitRadiusPx)
        return GizmoHandle::XY;

    return GizmoHandle::None;
}

// Apply drag delta (in screen pixels) to entity position, with optional snap.
inline Vec3 applyGizmoDrag(const GizmoState& state, Vec2 currentScreen,
                           const Camera& cam, int vpH, const GridSettings& grid) {
    Vec2 deltaScreen{currentScreen.x - state.dragStartScreen.x,
                     currentScreen.y - state.dragStartScreen.y};
    float worldPerPixel = (2.0f * cam.orthoHalfHeight) / static_cast<float>(vpH);
    Vec3 worldDelta{0, 0, 0};
    switch (state.active) {
        case GizmoHandle::X:
            worldDelta.x = deltaScreen.x * worldPerPixel;
            break;
        case GizmoHandle::Y:
            worldDelta.y = -deltaScreen.y * worldPerPixel;
            break;
        case GizmoHandle::Center:
        case GizmoHandle::XY:
            worldDelta.x = deltaScreen.x * worldPerPixel;
            worldDelta.y = -deltaScreen.y * worldPerPixel;
            break;
        default: break;
    }
    Vec3 newPos = state.originalPosition + worldDelta;
    return snapToGrid(newPos, grid);
}

}  // namespace anaconda
