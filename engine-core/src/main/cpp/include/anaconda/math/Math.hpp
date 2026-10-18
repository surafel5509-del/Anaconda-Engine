// ============================================================================
//  anaconda/math/Math.hpp
//
//  Header-only math foundation for the engine:
//      Vec2 / Vec3 / Vec4  — POD float vectors (gameplay + graphics)
//      Mat4                — 4x4 COLUMN-MAJOR matrix (OpenGL / GLES layout)
//
//  Conventions (documented once, used everywhere):
//    * Column-major storage m[16]: element(row r, col c) == m[c * 4 + r]
//      → glUniformMatrix4fv(..., GL_FALSE, mat.data()) uploads directly with
//        NO transpose.
//    * Right-handed coordinate system (GLES default clip space).
//    * Angles are RADIANS everywhere in native code; the editor UI converts
//      degrees ⇄ radians at the JNI boundary.
//    * Model matrices compose as  T * Rz * Ry * Rx * S  (scale first in
//      local space, then rotate, then translate — standard TRS).
//
//  Everything is `inline` — zero link dependencies, inlinable, testable on
//  any host (see tests/test_math.cpp).
// ============================================================================
#pragma once

#include <cmath>
#include <cstddef>

namespace anaconda {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = 6.28318530717958647692f;
inline constexpr float kDegToRad = kPi / 180.0f;
inline constexpr float kRadToDeg = 180.0f / kPi;

constexpr float radians(float degrees) noexcept { return degrees * kDegToRad; }
constexpr float degrees(float radians_) noexcept { return radians_ * kRadToDeg; }
constexpr float clampf(float v, float lo, float hi) noexcept {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ---------------------------------------------------------------------------
// Vec2
// ---------------------------------------------------------------------------
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    constexpr Vec2() = default;
    constexpr Vec2(float x_, float y_) noexcept : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const noexcept { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const noexcept { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const noexcept { return {x * s, y * s}; }
    constexpr Vec2 operator/(float s) const noexcept { return {x / s, y / s}; }
    constexpr Vec2 operator-() const noexcept { return {-x, -y}; }
    Vec2& operator+=(const Vec2& o) noexcept { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) noexcept { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(float s) noexcept { x *= s; y *= s; return *this; }
    constexpr bool operator==(const Vec2& o) const noexcept { return x == o.x && y == o.y; }

    [[nodiscard]] float lengthSquared() const noexcept { return x * x + y * y; }
    [[nodiscard]] float length() const noexcept { return std::sqrt(lengthSquared()); }
    [[nodiscard]] Vec2 normalized() const noexcept {
        const float len = length();
        return len > 1e-8f ? Vec2{x / len, y / len} : Vec2{};
    }
    static constexpr float dot(const Vec2& a, const Vec2& b) noexcept { return a.x * b.x + a.y * b.y; }
    static constexpr Vec2 lerp(const Vec2& a, const Vec2& b, float t) noexcept {
        return a + (b - a) * t;
    }
};

// ---------------------------------------------------------------------------
// Vec3
// ---------------------------------------------------------------------------
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(float x_, float y_, float z_) noexcept : x(x_), y(y_), z(z_) {}
    explicit constexpr Vec3(float s) noexcept : x(s), y(s), z(s) {}

    constexpr Vec3 operator+(const Vec3& o) const noexcept { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const noexcept { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator*(float s) const noexcept { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator*(const Vec3& o) const noexcept { return {x * o.x, y * o.y, z * o.z}; }
    constexpr Vec3 operator/(float s) const noexcept { return {x / s, y / s, z / s}; }
    constexpr Vec3 operator-() const noexcept { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) noexcept { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) noexcept { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) noexcept { x *= s; y *= s; z *= s; return *this; }
    constexpr bool operator==(const Vec3& o) const noexcept { return x == o.x && y == o.y && z == o.z; }

    [[nodiscard]] float lengthSquared() const noexcept { return x * x + y * y + z * z; }
    [[nodiscard]] float length() const noexcept { return std::sqrt(lengthSquared()); }
    [[nodiscard]] Vec3 normalized() const noexcept {
        const float len = length();
        return len > 1e-8f ? Vec3{x / len, y / len, z / len} : Vec3{};
    }

    static constexpr float dot(const Vec3& a, const Vec3& b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    static Vec3 cross(const Vec3& a, const Vec3& b) noexcept {
        return {a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
    }
    static constexpr Vec3 lerp(const Vec3& a, const Vec3& b, float t) noexcept {
        return a + (b - a) * t;
    }
    /// Component-wise absolute value (used by physics scale handling).
    static constexpr Vec3 abs(const Vec3& v) noexcept {
        return {v.x < 0 ? -v.x : v.x, v.y < 0 ? -v.y : v.y, v.z < 0 ? -v.z : v.z};
    }
};

// ---------------------------------------------------------------------------
// Vec4
// ---------------------------------------------------------------------------
struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    constexpr Vec4() = default;
    constexpr Vec4(float x_, float y_, float z_, float w_) noexcept
        : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vec4(const Vec3& v, float w_) noexcept : x(v.x), y(v.y), z(v.z), w(w_) {}

    constexpr Vec4 operator+(const Vec4& o) const noexcept {
        return {x + o.x, y + o.y, z + o.z, w + o.w};
    }
    constexpr Vec4 operator*(float s) const noexcept { return {x * s, y * s, z * s, w * s}; }
    constexpr Vec3 xyz() const noexcept { return {x, y, z}; }

    static constexpr float dot(const Vec4& a, const Vec4& b) noexcept {
        return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    }
};

// ---------------------------------------------------------------------------
// Mat4 — column-major (see file header)
// ---------------------------------------------------------------------------
struct Mat4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};

    constexpr Mat4() = default;

    /// Element access (row, col) — bounds are contractually [0,4).
    constexpr float& at(int row, int col) noexcept { return m[col * 4 + row]; }
    constexpr float at(int row, int col) const noexcept { return m[col * 4 + row]; }

    /// Raw pointer for glUniformMatrix4fv(GL_FALSE) — no transpose needed.
    const float* data() const noexcept { return m; }
    float* data() noexcept { return m; }

    static constexpr Mat4 identity() noexcept { return Mat4{}; }

    static constexpr Mat4 translation(const Vec3& t) noexcept {
        Mat4 r;
        r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
        return r;
    }

    static constexpr Mat4 scaling(const Vec3& s) noexcept {
        Mat4 r;
        r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
        return r;
    }

    // NOTE: std::cos/std::sin are not constexpr until C++26 — these are plain
    // static functions (still fully inlinable).
    static Mat4 rotationX(float a) noexcept {
        Mat4 r;
        const float c = std::cos(a), s = std::sin(a);
        r.m[5] = c;  r.m[6] = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 rotationY(float a) noexcept {
        Mat4 r;
        const float c = std::cos(a), s = std::sin(a);
        r.m[0] = c;  r.m[2] = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }

    static Mat4 rotationZ(float a) noexcept {
        Mat4 r;
        const float c = std::cos(a), s = std::sin(a);
        r.m[0] = c; r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    /// Euler (radians) → rotation, applied as R = Rz * Ry * Rx.
    static Mat4 rotationEuler(const Vec3& euler) noexcept {
        return rotationZ(euler.z) * rotationY(euler.y) * rotationX(euler.x);
    }

    /// Standard GL perspective projection (right-handed, clip z ∈ [-1, 1]).
    static Mat4 perspective(float fovyRadians, float aspect, float zNear, float zFar) noexcept {
        Mat4 r;
        const float f = 1.0f / std::tan(fovyRadians * 0.5f);
        r.m[0] = f / aspect;
        r.m[5] = f;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
        r.m[15] = 0.0f;
        return r;
    }

    /// OpenGL-style orthographic projection (clip z ∈ [-1, 1]).
    static Mat4 ortho(float left, float right, float bottom, float top,
                      float zNear, float zFar) noexcept {
        Mat4 r;
        r.m[0] = 2.0f / (right - left);
        r.m[5] = 2.0f / (top - bottom);
        r.m[10] = -2.0f / (zFar - zNear);
        r.m[12] = -(right + left) / (right - left);
        r.m[13] = -(top + bottom) / (top - bottom);
        r.m[14] = -(zFar + zNear) / (zFar - zNear);
        r.m[15] = 1.0f;
        return r;
    }

    /// Right-handed look-at view matrix.
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) noexcept {
        const Vec3 f = (center - eye).normalized();      // forward
        const Vec3 s = Vec3::cross(f, up).normalized();  // right
        const Vec3 u = Vec3::cross(s, f);                // re-orthogonalized up

        Mat4 r;
        r.m[0] = s.x;  r.m[4] = s.y;  r.m[8] = s.z;
        r.m[1] = u.x;  r.m[5] = u.y;  r.m[9] = u.z;
        r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
        r.m[12] = -Vec3::dot(s, eye);
        r.m[13] = -Vec3::dot(u, eye);
        r.m[14] = Vec3::dot(f, eye);
        r.m[15] = 1.0f;
        return r;
    }

    Mat4 operator*(const Mat4& b) const noexcept {
        Mat4 out;
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) sum += at(row, k) * b.at(k, col);
                out.at(row, col) = sum;
            }
        }
        return out;
    }

    /// Transform a 4D column vector: this * v.
    Vec4 operator*(const Vec4& v) const noexcept {
        return {
            m[0] * v.x + m[4] * v.y + m[8] * v.z + m[12] * v.w,
            m[1] * v.x + m[5] * v.y + m[9] * v.z + m[13] * v.w,
            m[2] * v.x + m[6] * v.y + m[10] * v.z + m[14] * v.w,
            m[3] * v.x + m[7] * v.y + m[11] * v.z + m[15] * v.w,
        };
    }

    /// Transform a point (w = 1) — the common game-side helper.
    Vec3 transformPoint(const Vec3& p) const noexcept {
        const Vec4 r = (*this) * Vec4{p, 1.0f};
        return r.xyz();
    }

    /// Transform a direction (w = 0 — ignores translation).
    Vec3 transformDirection(const Vec3& d) const noexcept {
        const Vec4 r = (*this) * Vec4{d, 0.0f};
        return r.xyz();
    }

    [[nodiscard]] Mat4 transposed() const noexcept {
        Mat4 out;
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c) out.at(r, c) = at(c, r);
        return out;
    }

    /// General 4x4 inverse via cofactors (MESA-style formulation).
    /// Returns identity for singular matrices instead of NaN — a game editor
    /// should degrade gracefully, not corrupt the scene.
    [[nodiscard]] Mat4 inverted() const noexcept {
        const float* a = m;
        float inv[16];

        inv[0] = a[5]*a[10]*a[15] - a[5]*a[11]*a[14] - a[9]*a[6]*a[15] + a[9]*a[7]*a[14] + a[13]*a[6]*a[11] - a[13]*a[7]*a[10];
        inv[4] = -a[4]*a[10]*a[15] + a[4]*a[11]*a[14] + a[8]*a[6]*a[15] - a[8]*a[7]*a[14] - a[12]*a[6]*a[11] + a[12]*a[7]*a[10];
        inv[8] = a[4]*a[9]*a[15] - a[4]*a[11]*a[13] - a[8]*a[5]*a[15] + a[8]*a[7]*a[13] + a[12]*a[5]*a[11] - a[12]*a[7]*a[9];
        inv[12] = -a[4]*a[9]*a[14] + a[4]*a[10]*a[13] + a[8]*a[5]*a[14] - a[8]*a[6]*a[13] - a[12]*a[5]*a[10] + a[12]*a[6]*a[9];
        inv[1] = -a[1]*a[10]*a[15] + a[1]*a[11]*a[14] + a[9]*a[2]*a[15] - a[9]*a[3]*a[14] - a[13]*a[2]*a[11] + a[13]*a[3]*a[10];
        inv[5] = a[0]*a[10]*a[15] - a[0]*a[11]*a[14] - a[8]*a[2]*a[15] + a[8]*a[3]*a[14] + a[12]*a[2]*a[11] - a[12]*a[3]*a[10];
        inv[9] = -a[0]*a[9]*a[15] + a[0]*a[11]*a[13] + a[8]*a[1]*a[15] - a[8]*a[3]*a[13] - a[12]*a[1]*a[11] + a[12]*a[3]*a[9];
        inv[13] = a[0]*a[9]*a[14] - a[0]*a[10]*a[13] - a[8]*a[1]*a[14] + a[8]*a[2]*a[13] + a[12]*a[1]*a[10] - a[12]*a[2]*a[9];
        inv[2] = a[1]*a[6]*a[15] - a[1]*a[7]*a[14] - a[5]*a[2]*a[15] + a[5]*a[3]*a[14] + a[13]*a[2]*a[7] - a[13]*a[3]*a[6];
        inv[6] = -a[0]*a[6]*a[15] + a[0]*a[7]*a[14] + a[4]*a[2]*a[15] - a[4]*a[3]*a[14] - a[12]*a[2]*a[7] + a[12]*a[3]*a[6];
        inv[10] = a[0]*a[5]*a[15] - a[0]*a[7]*a[13] - a[4]*a[1]*a[15] + a[4]*a[3]*a[13] + a[12]*a[1]*a[7] - a[12]*a[3]*a[5];
        inv[14] = -a[0]*a[5]*a[14] + a[0]*a[6]*a[13] + a[4]*a[1]*a[14] - a[4]*a[2]*a[13] - a[12]*a[1]*a[6] + a[12]*a[2]*a[5];
        inv[3] = -a[1]*a[6]*a[11] + a[1]*a[7]*a[10] + a[5]*a[2]*a[11] - a[5]*a[3]*a[10] - a[9]*a[2]*a[7] + a[9]*a[3]*a[6];
        inv[7] = a[0]*a[6]*a[11] - a[0]*a[7]*a[10] - a[4]*a[2]*a[11] + a[4]*a[3]*a[10] + a[8]*a[2]*a[7] - a[8]*a[3]*a[6];
        inv[11] = -a[0]*a[5]*a[11] + a[0]*a[7]*a[9] + a[4]*a[1]*a[11] - a[4]*a[3]*a[9] - a[8]*a[1]*a[7] + a[8]*a[3]*a[5];
        inv[15] = a[0]*a[5]*a[10] - a[0]*a[6]*a[9] - a[4]*a[1]*a[10] + a[4]*a[2]*a[9] + a[8]*a[1]*a[6] - a[8]*a[2]*a[5];

        const float det = a[0]*inv[0] + a[1]*inv[4] + a[2]*inv[8] + a[3]*inv[12];
        if (det == 0.0f) return identity();

        Mat4 out;
        const float invDet = 1.0f / det;
        for (int i = 0; i < 16; ++i) out.m[i] = inv[i] * invDet;
        return out;
    }

    [[nodiscard]] float determinant() const noexcept {
        const Mat4& a = *this;
        float inv[16];
        inv[0] = a.m[5]*a.m[10]*a.m[15] - a.m[5]*a.m[11]*a.m[14] - a.m[9]*a.m[6]*a.m[15] + a.m[9]*a.m[7]*a.m[14] + a.m[13]*a.m[6]*a.m[11] - a.m[13]*a.m[7]*a.m[10];
        inv[4] = -a.m[4]*a.m[10]*a.m[15] + a.m[4]*a.m[11]*a.m[14] + a.m[8]*a.m[6]*a.m[15] - a.m[8]*a.m[7]*a.m[14] - a.m[12]*a.m[6]*a.m[11] + a.m[12]*a.m[7]*a.m[10];
        inv[8] = a.m[4]*a.m[9]*a.m[15] - a.m[4]*a.m[11]*a.m[13] - a.m[8]*a.m[5]*a.m[15] + a.m[8]*a.m[7]*a.m[13] + a.m[12]*a.m[5]*a.m[11] - a.m[12]*a.m[7]*a.m[9];
        inv[12] = -a.m[4]*a.m[9]*a.m[14] + a.m[4]*a.m[10]*a.m[13] + a.m[8]*a.m[5]*a.m[14] - a.m[8]*a.m[6]*a.m[13] - a.m[12]*a.m[5]*a.m[10] + a.m[12]*a.m[6]*a.m[9];
        return a.m[0]*inv[0] + a.m[1]*inv[4] + a.m[2]*inv[8] + a.m[3]*inv[12];
    }
};

/// Convenience: full TRS model matrix used by the sprite renderer.
inline Mat4 makeModelMatrix(const Vec3& position, const Vec3& eulerRadians,
                            const Vec3& scale) noexcept {
    return Mat4::translation(position) * Mat4::rotationEuler(eulerRadians) *
           Mat4::scaling(scale);
}

}  // namespace anaconda
