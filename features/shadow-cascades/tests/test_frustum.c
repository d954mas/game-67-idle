#include "unity.h"
#include "features/shadow_cascades/frustum.h"

#include <math.h>
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

/* Column-major GL matrices, as cglm builds them for the renderer. */
static void mul(float out[16], const float a[16], const float b[16]) {
    float r[16];
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row) {
            r[4 * c + row] = 0.0F;
            for (int k = 0; k < 4; ++k) r[4 * c + row] += a[4 * k + row] * b[4 * c + k];
        }
    memcpy(out, r, sizeof r);
}

static void apply(const float m[16], const float p[4], float out[4]) {
    for (int row = 0; row < 4; ++row) out[row] = m[row] * p[0] + m[4 + row] * p[1] + m[8 + row] * p[2] + m[12 + row] * p[3];
}

static void normalize3(float v[3]) {
    const float length = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    for (int i = 0; i < 3; ++i) v[i] /= length;
}

static void cross3(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

static void look_at(float m[16], const float eye[3], const float target[3], const float up[3]) {
    float f[3] = {target[0] - eye[0], target[1] - eye[1], target[2] - eye[2]}, s[3], u[3];
    normalize3(f);
    cross3(f, up, s);
    normalize3(s);
    cross3(s, f, u);
    const float r[16] = {s[0], u[0], -f[0], 0, s[1], u[1], -f[1], 0, s[2], u[2], -f[2], 0,
        -(s[0] * eye[0] + s[1] * eye[1] + s[2] * eye[2]), -(u[0] * eye[0] + u[1] * eye[1] + u[2] * eye[2]),
        f[0] * eye[0] + f[1] * eye[1] + f[2] * eye[2], 1};
    memcpy(m, r, sizeof r);
}

static void perspective(float m[16], float fov_y, float aspect, float n, float f) {
    const float t = 1.0F / tanf(0.5F * fov_y);
    const float r[16] = {t / aspect, 0, 0, 0, 0, t, 0, 0, 0, 0, (f + n) / (n - f), -1, 0, 0, 2.0F * f * n / (n - f), 0};
    memcpy(m, r, sizeof r);
}

static void ortho(float m[16], float half, float n, float f) {
    const float r[16] = {1.0F / half, 0, 0, 0, 0, 1.0F / half, 0, 0, 0, 0, -2.0F / (f - n), 0, 0, 0, -(f + n) / (f - n), 1};
    memcpy(m, r, sizeof r);
}

static void view_proj(float vp[16], const float eye[3], const float target[3], float fov_y, float aspect, float n, float f) {
    float view[16], proj[16];
    look_at(view, eye, target, (const float[3]){0.0F, 1.0F, 0.0F});
    perspective(proj, fov_y, aspect, n, f);
    mul(vp, proj, view);
}

static void camera(frustum_t *out) {
    float vp[16];
    view_proj(vp, (const float[3]){0.0F, 2.0F, 10.0F}, (const float[3]){0.0F, 2.0F, 0.0F}, 1.0F, 16.0F / 9.0F, 0.1F, 100.0F);
    frustum_from_view_proj(out, vp);
}

static bool sees_point(const frustum_t *f, float x, float y, float z) {
    return frustum_sees_box(f, (float[3]){x, y, z}, (float[3]){x, y, z}, 0.0F);
}

/* A point is kept exactly when its clip position lies inside every clip bound. */
static void test_planes_match_the_clip_volume(void) {
    float vp[16];
    view_proj(vp, (const float[3]){3.0F, 5.0F, 12.0F}, (const float[3]){-2.0F, 0.0F, -4.0F}, 1.1F, 1.5F, 0.2F, 60.0F);
    frustum_t f;
    frustum_from_view_proj(&f, vp);
    unsigned inside_count = 0U, outside_count = 0U;
    for (int i = 0; i < 4000; ++i) {
        const float x = (float)((i * 37) % 101) - 50.0F;
        const float y = (float)((i * 53) % 61) - 30.0F;
        const float z = (float)((i * 71) % 131) - 90.0F;
        float clip[4];
        apply(vp, (const float[4]){x, y, z, 1.0F}, clip);
        const float margin = 1e-3F * fabsf(clip[3]);
        const bool inside = fabsf(clip[0]) < clip[3] - margin && fabsf(clip[1]) < clip[3] - margin && fabsf(clip[2]) < clip[3] - margin;
        const bool outside = fabsf(clip[0]) > clip[3] + margin || fabsf(clip[1]) > clip[3] + margin || fabsf(clip[2]) > clip[3] + margin;
        if (inside) {
            ++inside_count;
            TEST_ASSERT_TRUE(sees_point(&f, x, y, z));
        }
        if (outside) {
            ++outside_count;
            TEST_ASSERT_FALSE(sees_point(&f, x, y, z));
        }
    }
    TEST_ASSERT_TRUE(inside_count > 100U && outside_count > 100U);
}

static void test_a_box_straddling_a_plane_is_kept(void) {
    frustum_t f;
    camera(&f);
    TEST_ASSERT_TRUE(frustum_sees_box(&f, (float[3]){-1.0F, 0.0F, -1.0F}, (float[3]){1.0F, 4.0F, 1.0F}, 0.0F));
    /* Behind the eye, far to the side, and past the far plane. */
    TEST_ASSERT_FALSE(frustum_sees_box(&f, (float[3]){-1.0F, 0.0F, 12.0F}, (float[3]){1.0F, 4.0F, 14.0F}, 0.0F));
    TEST_ASSERT_FALSE(frustum_sees_box(&f, (float[3]){200.0F, 0.0F, -5.0F}, (float[3]){210.0F, 4.0F, 5.0F}, 0.0F));
    TEST_ASSERT_FALSE(frustum_sees_box(&f, (float[3]){-1.0F, 0.0F, -200.0F}, (float[3]){1.0F, 4.0F, -150.0F}, 0.0F));
    /* A box reaching back into view from behind the eye still draws. */
    TEST_ASSERT_TRUE(frustum_sees_box(&f, (float[3]){-1.0F, 0.0F, 5.0F}, (float[3]){1.0F, 4.0F, 14.0F}, 0.0F));
}

/* Slack is in metres: a box just past the far plane is kept within it and dropped beyond. */
static void test_slack_is_metres(void) {
    frustum_t f;
    camera(&f);
    const float far_z = 10.0F - 100.0F;
    TEST_ASSERT_TRUE(frustum_sees_box(&f, (float[3]){0.0F, 2.0F, far_z - 0.4F}, (float[3]){0.0F, 2.0F, far_z - 0.3F}, 0.5F));
    TEST_ASSERT_FALSE(frustum_sees_box(&f, (float[3]){0.0F, 2.0F, far_z - 0.7F}, (float[3]){0.0F, 2.0F, far_z - 0.6F}, 0.5F));
}

/* An orthographic light looking straight down: the test bounds the cascade on all six sides. */
static void test_orthographic_cascade(void) {
    float view[16], proj[16], vp[16];
    look_at(view, (const float[3]){0.0F, 50.0F, 0.0F}, (const float[3]){0.0F, 0.0F, 0.0F}, (const float[3]){0.0F, 0.0F, -1.0F});
    ortho(proj, 10.0F, 1.0F, 100.0F);
    mul(vp, proj, view);
    frustum_t f;
    frustum_from_view_proj(&f, vp);
    TEST_ASSERT_TRUE(sees_point(&f, 9.0F, 0.0F, 9.0F));
    TEST_ASSERT_FALSE(sees_point(&f, 11.0F, 0.0F, 0.0F));
    TEST_ASSERT_FALSE(sees_point(&f, 0.0F, 60.0F, 0.0F));
    TEST_ASSERT_FALSE(sees_point(&f, 0.0F, -60.0F, 0.0F));
}

/* A rotated, scaled, moved unit cube: the world box holds every transformed corner and touches them. */
static void test_world_box_holds_the_corners(void) {
    const float c = cosf(0.7F), s = sinf(0.7F);
    /* Translate * rotate about y * scale (2, 0.5, 3). */
    const float m[16] = {2.0F * c, 0, -2.0F * s, 0, 0, 0.5F, 0, 0, 3.0F * s, 0, 3.0F * c, 0, 4.0F, 1.0F, -3.0F, 1};
    const float lo[3] = {-0.5F, -0.5F, -0.5F}, hi[3] = {0.5F, 0.5F, 0.5F};
    float wmin[3], wmax[3];
    frustum_box_to_world(m, lo, hi, wmin, wmax);
    float got_min[3] = {1e9F, 1e9F, 1e9F}, got_max[3] = {-1e9F, -1e9F, -1e9F};
    for (int corner = 0; corner < 8; ++corner) {
        const float p[4] = {(corner & 1) ? hi[0] : lo[0], (corner & 2) ? hi[1] : lo[1], (corner & 4) ? hi[2] : lo[2], 1.0F};
        float w[4];
        apply(m, p, w);
        for (int axis = 0; axis < 3; ++axis) {
            got_min[axis] = fminf(got_min[axis], w[axis]);
            got_max[axis] = fmaxf(got_max[axis], w[axis]);
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        TEST_ASSERT_TRUE(fabsf(wmin[axis] - got_min[axis]) < 1e-4F);
        TEST_ASSERT_TRUE(fabsf(wmax[axis] - got_max[axis]) < 1e-4F);
    }
}

static void test_light_frustum_keeps_casters_before_depth_window(void) {
    float lview[16], lproj[16], lvp[16];
    look_at(lview, (const float[3]){0.0F, 50.0F, 0.0F}, (const float[3]){0.0F, 0.0F, 0.0F}, (const float[3]){0.0F, 0.0F, -1.0F});
    ortho(lproj, 10.0F, 1.0F, 100.0F);
    mul(lvp, lproj, lview);
    frustum_t light_f;
    frustum_from_light_view_proj(&light_f, lvp);
    TEST_ASSERT_TRUE(sees_point(&light_f, 0.0F, 60.0F, 0.0F));
    TEST_ASSERT_FALSE(sees_point(&light_f, 11.0F, 60.0F, 0.0F));
    TEST_ASSERT_FALSE(sees_point(&light_f, 0.0F, -60.0F, 0.0F));
}

static void test_shadow_volume_keeps_offscreen_casters_toward_sun(void) {
    frustum_t camera_f;
    camera(&camera_f);
    const float eye[3] = {0.0F, 2.0F, 10.0F};
    const float target[3] = {0.0F, 2.0F, 0.0F};
    const float sun[3] = {0.6F, 0.8F, 0.0F};
    shadow_volume_t volume;
    TEST_ASSERT_TRUE(shadow_volume_from_camera_slice(&volume, &camera_f, eye, target, 1.0F, 16.0F / 9.0F, 0.1F, 12.0F, sun));
    const float caster[3] = {12.0F, 16.0F, 4.0F};
    TEST_ASSERT_FALSE(frustum_sees_box(&camera_f, caster, caster, 0.0F));
    TEST_ASSERT_TRUE(shadow_volume_sees_box(&volume, caster, caster, 0.05F));
    const float clip_min[3] = {-20.0F, -1.0F, -20.0F};
    const float clip_max[3] = {20.0F, 30.0F, 20.0F};
    float bounds_min[3], bounds_max[3];
    TEST_ASSERT_TRUE(shadow_volume_bounds_in_box(&volume, clip_min, clip_max, bounds_min, bounds_max));
    for (int axis = 0; axis < 3; ++axis) {
        TEST_ASSERT_TRUE(bounds_min[axis] <= caster[axis]);
        TEST_ASSERT_TRUE(bounds_max[axis] >= caster[axis]);
    }
}

/* A camera or sun that gives no direction builds no volume. */
static void test_shadow_volume_rejects_degenerate_vectors(void) {
    frustum_t camera_f;
    camera(&camera_f);
    const float eye[3] = {0.0F, 2.0F, 10.0F};
    const float target[3] = {0.0F, 2.0F, 0.0F};
    const float zero[3] = {0.0F, 0.0F, 0.0F};
    const float tiny[3] = {1.0e-8F, 0.0F, 0.0F};
    const float nan_sun[3] = {NAN, 0.8F, 0.0F};
    const float inf_target[3] = {INFINITY, 2.0F, 0.0F};
    const float sun[3] = {0.6F, 0.8F, 0.0F};
    shadow_volume_t volume;
    TEST_ASSERT_FALSE(shadow_volume_from_camera_slice(&volume, &camera_f, eye, eye, 1.0F, 1.5F, 0.1F, 12.0F, sun));
    TEST_ASSERT_FALSE(shadow_volume_from_camera_slice(&volume, &camera_f, eye, target, 1.0F, 1.5F, 0.1F, 12.0F, zero));
    TEST_ASSERT_FALSE(shadow_volume_from_camera_slice(&volume, &camera_f, eye, target, 1.0F, 1.5F, 0.1F, 12.0F, tiny));
    TEST_ASSERT_FALSE(shadow_volume_from_camera_slice(&volume, &camera_f, eye, target, 1.0F, 1.5F, 0.1F, 12.0F, nan_sun));
    TEST_ASSERT_FALSE(shadow_volume_from_camera_slice(&volume, &camera_f, eye, inf_target, 1.0F, 1.5F, 0.1F, 12.0F, sun));
}

/* Row k / 2 of the matrix added to or taken from row 3, then normalized. */
static void test_planes_match_the_row_extraction(void) {
    float vp[16];
    view_proj(vp, (const float[3]){3.0F, 5.0F, 12.0F}, (const float[3]){-2.0F, 0.0F, -4.0F}, 1.1F, 1.5F, 0.2F, 60.0F);
    frustum_t f;
    frustum_from_view_proj(&f, vp);
    for (int k = 0; k < 6; ++k) {
        float p[4];
        const float sign = (k % 2 == 0) ? 1.0F : -1.0F;
        for (int c = 0; c < 4; ++c) { p[c] = vp[4 * c + 3] + sign * vp[4 * c + k / 2]; }
        const float length = sqrtf(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
        for (int c = 0; c < 4; ++c) { TEST_ASSERT_TRUE(fabsf(p[c] / length - f.plane[k][c]) <= 1.0e-6F); }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_planes_match_the_clip_volume);
    RUN_TEST(test_planes_match_the_row_extraction);
    RUN_TEST(test_a_box_straddling_a_plane_is_kept);
    RUN_TEST(test_slack_is_metres);
    RUN_TEST(test_orthographic_cascade);
    RUN_TEST(test_world_box_holds_the_corners);
    RUN_TEST(test_light_frustum_keeps_casters_before_depth_window);
    RUN_TEST(test_shadow_volume_keeps_offscreen_casters_toward_sun);
    RUN_TEST(test_shadow_volume_rejects_degenerate_vectors);
    return UNITY_END();
}
