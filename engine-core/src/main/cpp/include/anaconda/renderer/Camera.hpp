// ============================================================================
//  anaconda/renderer/Camera.hpp
//
//  Editor/game camera. Header-only — no GL dependency (pure math), so it is
//  usable in host tests and by the future picking code.
//
//  Phase-1 behavior:
//    * Default mode is ORTHOGRAPHIC (2D): `orthoHalfHeight` = visible world
//      half-height (5 units); aspect comes from the surface size.
//    * Pan/zoom helpers consume pixel gestures from the JNI bridge and
//      convert with the current viewport height (single source of truth for
//      world-per-pixel lives here).
//    * PERSPECTIVE mode is implemented and unit-covered for Month-3 3D work;
//      orbit controls arrive with viewport gizmos.
// ============================================================================
#pragma once

#include "anaconda/math/Math.hpp"

namespace anaconda {

class Camera {
public:
    enum class Mode { Orthographic, Perspective };

    Mode mode = Mode::Orthographic;

    /// View direction anchor: view = lookAt(position, target, up).
    Vec3 position{0.0f, 0.0f, 10.0f};
    Vec3 target{0.0f, 0.0f, 0.0f};
    Vec3 worldUp{0.0f, 1.0f, 0.0f};

    // Orthographic parameters (world units).
    float orthoHalfHeight = 5.0f;

    // Perspective parameters.
    float fovDegrees = 60.0f;
    float zNear = 0.1f;
    float zFar = 200.0f;

    // ---- matrices ---------------------------------------------------------
    [[nodiscard]] Mat4 view() const {
        return Mat4::lookAt(position, target, worldUp);
    }

    [[nodiscard]] Mat4 projection(float aspect) const {
        if (mode == Mode::Perspective) {
            return Mat4::perspective(radians(fovDegrees), aspect, zNear, zFar);
        }
        const float h = orthoHalfHeight;
        const float w = h * aspect;
        // Symmetric frustum centered on the view target's XY? We keep the
        // frustum centered at world origin axes for predictable pan math —
        // camera position only affects the VIEW matrix (lookAt), so pan moves
        // both together. Symmetric bounds are therefore correct:
        return Mat4::ortho(-w, w, -h, h, -zFar, zFar);
    }

    // ---- gesture helpers (pixels in, world units out) ---------------------

    /// Pan the camera by a screen-space pixel delta (content follows finger).
    void panPixels(float dxPixels, float dyPixels, int viewportHeightPx) {
        if (viewportHeightPx <= 0) return;
        const float worldPerPixel =
            (2.0f * orthoHalfHeight) / static_cast<float>(viewportHeightPx);
        // Screen X grows right → dragging right must move the camera left.
        position.x -= dxPixels * worldPerPixel;
        target.x -= dxPixels * worldPerPixel;
        // Screen Y grows DOWN → dragging down must move the camera up (+Y).
        position.y += dyPixels * worldPerPixel;
        target.y += dyPixels * worldPerPixel;
    }

    /// Pinch zoom: scaleFactor > 1 = fingers apart = zoom in (smaller world).
    void zoomBy(float scaleFactor, float minHalfHeight = 0.5f,
                float maxHalfHeight = 60.0f) {
        if (scaleFactor <= 1e-4f) return;
        orthoHalfHeight = clampf(orthoHalfHeight / scaleFactor, minHalfHeight,
                                 maxHalfHeight);
    }

    /// Fit a world-space region into the current aspect (frame-selected, Phase 3).
    void frameRegion(const Vec3& center, float halfSize) {
        target = center;
        position = {center.x, center.y, center.z + (zFar * 0.4f)};
        orthoHalfHeight = clampf(halfSize * 1.25f, 0.5f, 60.0f);
    }

    /// Convert a screen pixel (origin top-left) to a world XY point (orthographic).
    /// Uses the current target + orthoHalfHeight + viewport dimensions.
    [[nodiscard]] Vec2 screenToWorld(float screenX, float screenY,
                                    int viewportWidth, int viewportHeight) const {
        if (viewportWidth <= 0 || viewportHeight <= 0) return {target.x, target.y};
        const float worldPerPixel =
            (2.0f * orthoHalfHeight) / static_cast<float>(viewportHeight);
        const float wx = target.x + (screenX - viewportWidth * 0.5f) * worldPerPixel;
        const float wy = target.y + (viewportHeight * 0.5f - screenY) * worldPerPixel;
        return {wx, wy};
    }
};

}  // namespace anaconda
