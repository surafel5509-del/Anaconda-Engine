// ============================================================================
//  tests/test_math.cpp — Vec/Mat foundations (host, no GL).
// ============================================================================
#include "anaconda/math/Math.hpp"
#include "test_framework.hpp"

using anaconda::Mat4;
using anaconda::Vec2;
using anaconda::Vec3;
using anaconda::Vec4;

namespace {

constexpr float kEps = 1e-5f;

}  // namespace

TEST(vec3_basic_algebra) {
    const Vec3 a{1.0f, 2.0f, 3.0f};
    const Vec3 b{4.0f, 5.0f, 6.0f};

    const Vec3 sum = a + b;
    CHECK_NEAR(sum.x, 5.0f, kEps);
    CHECK_NEAR(sum.y, 7.0f, kEps);
    CHECK_NEAR(sum.z, 9.0f, kEps);

    CHECK_NEAR(Vec3::dot(a, b), 32.0f, kEps);  // 4 + 10 + 18

    const Vec3 c = Vec3::cross({1, 0, 0}, {0, 1, 0});
    CHECK_NEAR(c.x, 0.0f, kEps);
    CHECK_NEAR(c.y, 0.0f, kEps);
    CHECK_NEAR(c.z, 1.0f, kEps);  // right-handed basis: X × Y = Z

    const Vec3 n = Vec3{3.0f, 0.0f, 4.0f}.normalized();
    CHECK_NEAR(n.length(), 1.0f, kEps);
    CHECK_NEAR(n.x, 0.6f, kEps);
    CHECK_NEAR(n.y, 0.0f, kEps);
    CHECK_NEAR(Vec3{}.normalized().length(), 0.0f, kEps);  // zero-safe
}

TEST(vec2_and_vec4_smoke) {
    const Vec2 v = Vec2{3.0f, 4.0f};
    CHECK_NEAR(v.length(), 5.0f, kEps);
    CHECK_NEAR(Vec2::dot({1, 2}, {3, 4}), 11.0f, kEps);

    const Vec4 q{Vec3{1, 2, 3}, 1.0f};
    CHECK_NEAR(q.w, 1.0f, kEps);
    CHECK_NEAR(Vec4::dot(q, {1, 1, 1, 1}), 7.0f, kEps);
}

TEST(mat4_identity_multiply_is_noop) {
    const Mat4 id = Mat4::identity();
    const Mat4 m = Mat4::translation({1, 2, 3}) * Mat4::scaling({2, 2, 2});
    const Mat4 r = id * m;
    // Compare against direct element access.
    for (int i = 0; i < 16; ++i) CHECK_NEAR(r.m[i], m.m[i], kEps);
}

TEST(translation_moves_point) {
    const Mat4 t = Mat4::translation({10.0f, -5.0f, 2.0f});
    const Vec3 p = t.transformPoint({1.0f, 1.0f, 1.0f});
    CHECK_NEAR(p.x, 11.0f, kEps);
    CHECK_NEAR(p.y, -4.0f, kEps);
    CHECK_NEAR(p.z, 3.0f, kEps);

    // Direction must IGNORE translation (w = 0 semantics).
    const Vec3 d = t.transformDirection({1.0f, 0.0f, 0.0f});
    CHECK_NEAR(d.x, 1.0f, kEps);
    CHECK_NEAR(d.y, 0.0f, kEps);
    CHECK_NEAR(d.z, 0.0f, kEps);
}

TEST(rotation_z_90_degrees) {
    const Mat4 r = Mat4::rotationZ(anaconda::radians(90.0f));
    const Vec3 p = r.transformPoint({1.0f, 0.0f, 0.0f});
    CHECK_NEAR(p.x, 0.0f, 1e-5f);
    CHECK_NEAR(p.y, 1.0f, 1e-5f);   // +X rotates to +Y (right-handed, CCW)
    CHECK_NEAR(p.z, 0.0f, 1e-5f);
}

TEST(trs_composition_order) {
    // makeModelMatrix = T * R * S → scale happens in local space first.
    const Mat4 model = anaconda::makeModelMatrix({5, 0, 0},
                                                 {0, 0, anaconda::radians(90)},
                                                 {2, 2, 2});
    // Local (1,0,0) → scaled to (2,0,0) → rotated to (0,2,0) → +5 X.
    const Vec3 p = model.transformPoint({1, 0, 0});
    CHECK_NEAR(p.x, 5.0f, 1e-4f);
    CHECK_NEAR(p.y, 2.0f, 1e-4f);
    CHECK_NEAR(p.z, 0.0f, 1e-4f);
}

TEST(ortho_maps_center_to_ndc_origin) {
    const Mat4 o = Mat4::ortho(-10, 10, -5, 5, -100, 100);
    const Vec4 clip = o * Vec4{0, 0, 0, 1};
    CHECK_NEAR(clip.x, 0.0f, kEps);
    CHECK_NEAR(clip.y, 0.0f, kEps);
    CHECK_NEAR(clip.z, 0.0f, kEps);
    CHECK_NEAR(clip.w, 1.0f, kEps);

    // Right edge maps to +1 in clip X.
    const Vec4 right = o * Vec4{10, 0, 0, 1};
    CHECK_NEAR(right.x, 1.0f, kEps);

    // Top edge maps to +1 in clip Y.
    const Vec4 top = o * Vec4{0, 5, 0, 1};
    CHECK_NEAR(top.y, 1.0f, kEps);
}

TEST(lookat_views_world_relative_to_eye) {
    // Camera at +Z looking at origin: world origin should land at z = -5.
    const Mat4 v = Mat4::lookAt({0, 0, 5}, {0, 0, 0}, {0, 1, 0});
    const Vec3 p = v.transformPoint({0, 0, 0});
    CHECK_NEAR(p.x, 0.0f, kEps);
    CHECK_NEAR(p.y, 0.0f, kEps);
    CHECK_NEAR(p.z, -5.0f, kEps);

    // A point BEHIND the camera must move to positive Z (behind clip).
    const Vec3 behind = v.transformPoint({0, 0, 6});
    CHECK(behind.z > 0.0f);
}

TEST(inverse_roundtrip_trs) {
    const Mat4 m = anaconda::makeModelMatrix({3.5f, -2.0f, 7.25f},
                                             {0.4f, -1.1f, 2.0f},
                                             {1.5f, 0.5f, 2.5f});
    const Mat4 inv = m.inverted();
    const Mat4 shouldBeIdentity = m * inv;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            CHECK_NEAR(shouldBeIdentity.at(r, c), r == c ? 1.0f : 0.0f, 1e-4f);
}

TEST(determinant_of_simple_matrices) {
    CHECK_NEAR(Mat4::identity().determinant(), 1.0f, kEps);
    // Scaling matrix determinant = product of scales.
    const Mat4 s = Mat4::scaling({2, 3, 4});
    CHECK_NEAR(s.determinant(), 24.0f, 1e-4f);
    // Translation does not change |det|.
    const Mat4 t = Mat4::translation({99, -99, 99});
    CHECK_NEAR(t.determinant(), 1.0f, kEps);
}

TEST(transpose_roundtrip) {
    const Mat4 m = anaconda::makeModelMatrix({1, 2, 3}, {0.3f, 0.7f, 1.2f},
                                             {1, 2, 1});
    const Mat4 mm = m.transposed().transposed();
    for (int i = 0; i < 16; ++i) CHECK_NEAR(mm.m[i], m.m[i], 1e-5f);
}

TEST(perspective_depth_mapping) {
    const Mat4 p = Mat4::perspective(anaconda::radians(60.0f), 16.0f / 9.0f,
                                     0.1f, 100.0f);
    // Near plane point → clip z ≈ -w (NDC z = -1).
    const Vec4 nearPt = p * Vec4{0, 0, -0.1f, 1};
    CHECK_NEAR(nearPt.z / nearPt.w, -1.0f, 1e-4f);
    // Far plane point → clip z ≈ +w (NDC z = +1).
    const Vec4 farPt = p * Vec4{0, 0, -100.0f, 1};
    CHECK_NEAR(farPt.z / farPt.w, 1.0f, 1e-3f);
    // In front of the camera → positive w (perspective divide valid).
    CHECK(nearPt.w > 0.0f);
}

TEST(degree_radian_conversion) {
    CHECK_NEAR(anaconda::radians(180.0f), anaconda::kPi, 1e-6f);
    CHECK_NEAR(anaconda::degrees(anaconda::kPi / 2.0f), 90.0f, 1e-4f);
    CHECK_NEAR(anaconda::clampf(5.0f, 0.0f, 1.0f), 1.0f, kEps);
    CHECK_NEAR(anaconda::clampf(-5.0f, 0.0f, 1.0f), 0.0f, kEps);
}

int main() { return anaconda::test::runAll(); }
