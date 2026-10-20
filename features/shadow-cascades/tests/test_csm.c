#include "unity.h"
#include "features/shadow_cascades/csm.h"
#include "features/shadow_cascades/frustum.h"

#include <math.h>

void setUp(void) {}
void tearDown(void) {}

/* Some Unity builds exclude float assertions. */
#define ASSERT_CLOSE(tolerance, expected, actual) TEST_ASSERT_TRUE(fabsf((expected) - (actual)) <= (tolerance))

static csm_config_t config(void) {
    return (csm_config_t){.cascades = 3, .resolution = 1024, .split_far = {12.0F, 32.0F, 75.0F},
        .depth_range = 260.0F, .margin_texels = 2.0F, .sun_dir = {0.38F, 0.74F, -0.55F}};
}

static csm_camera_t camera(float x, float z, float yaw) {
    csm_camera_t result = {.eye = {x - 9.0F * sinf(yaw), 4.0F, z + 9.0F * cosf(yaw)}, .target = {x, 1.2F, z},
        .fov_y = 1.0F, .aspect = 16.0F / 9.0F, .near_plane = 0.1F};
    return result;
}

static void transform(const float m[16], const float p[3], float out[4]) {
    for (int row = 0; row < 4; ++row) out[row] = m[row] * p[0] + m[4 + row] * p[1] + m[8 + row] * p[2] + m[12 + row];
}

/* The analytic sphere holds every corner of its slice, and no smaller sphere on the axis would. */
static void test_slice_sphere_holds_its_corners(void) {
    const float slices[][2] = {{0.1F, 12.0F}, {12.0F, 32.0F}, {32.0F, 75.0F}, {0.1F, 0.5F}};
    for (unsigned s = 0; s < sizeof slices / sizeof slices[0]; ++s) {
        const float fov = 1.0F, aspect = 16.0F / 9.0F;
        float c, r;
        csm_slice_sphere(slices[s][0], slices[s][1], fov, aspect, &c, &r);
        const float k = tanf(0.5F * fov) * sqrtf(1.0F + aspect * aspect);
        const float near = sqrtf((c - slices[s][0]) * (c - slices[s][0]) + slices[s][0] * slices[s][0] * k * k);
        const float far = sqrtf((slices[s][1] - c) * (slices[s][1] - c) + slices[s][1] * slices[s][1] * k * k);
        TEST_ASSERT_TRUE(near <= r * 1.0001F && far <= r * 1.0001F);
        TEST_ASSERT_TRUE(fabsf(far - r) < 1e-3F * r);
    }
}

static void test_each_cascade_holds_its_whole_slice(void) {
    const csm_config_t c = config();
    const csm_camera_t cam = camera(3.0F, -40.0F, 0.4F);
    csm_cascades_t out;
    csm_compute(&c, &cam, &out);
    float forward[3] = {cam.target[0] - cam.eye[0], cam.target[1] - cam.eye[1], cam.target[2] - cam.eye[2]};
    const float length = sqrtf(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
    float near = cam.near_plane;
    for (uint32_t i = 0; i < out.count; ++i) {
        /* Points along the view axis across the slice land inside the cascade, depth included. */
        for (float t = 0.0F; t <= 1.0F; t += 0.25F) {
            const float d = near + (out.split_far[i] - near) * t;
            const float p[3] = {cam.eye[0] + forward[0] / length * d, cam.eye[1] + forward[1] / length * d, cam.eye[2] + forward[2] / length * d};
            float clip[4];
            transform(out.render_vp[i], p, clip);
            for (int k = 0; k < 3; ++k) TEST_ASSERT_TRUE(fabsf(clip[k]) <= 1.0F);
        }
        near = out.split_far[i];
    }
}

static void test_lookup_lands_in_the_cascade_tile(void) {
    const csm_config_t c = config();
    const csm_camera_t cam = camera(0.0F, 0.0F, 0.0F);
    csm_cascades_t out;
    csm_compute(&c, &cam, &out);
    TEST_ASSERT_EQUAL_UINT32(2048U, csm_atlas_width(&c));
    TEST_ASSERT_EQUAL_UINT32(2048U, csm_atlas_height(&c));
    for (uint32_t i = 0; i < out.count; ++i) {
        const float p[3] = {cam.target[0], cam.target[1], cam.target[2]};
        float clip[4], light[4], uv[3];
        transform(out.render_vp[i], p, clip);
        transform(out.light_view, p, light);
        for (int axis = 0; axis < 3; ++axis) uv[axis] = light[axis] * out.lookup_scale[i][axis] + out.lookup_offset[i][axis];
        const float atlas = (float)csm_atlas_width(&c);
        const float x = (float)out.viewport[i][0] / atlas + (clip[0] * 0.5F + 0.5F) * (float)out.viewport[i][2] / atlas;
        const float y = (float)out.viewport[i][1] / atlas + (clip[1] * 0.5F + 0.5F) * (float)out.viewport[i][3] / atlas;
        ASSERT_CLOSE(1e-4F, x, uv[0]);
        ASSERT_CLOSE(1e-4F, y, uv[1]);
        ASSERT_CLOSE(1e-6F, out.tile[i][0], (float)out.viewport[i][0] / atlas);
        ASSERT_CLOSE(1e-6F, out.tile[i][3], (float)(out.viewport[i][1] + out.viewport[i][3]) / atlas);
        ASSERT_CLOSE(1e-4F, clip[2] * 0.5F + 0.5F, uv[2]);
    }
}

/* A still world point moves across the shadow map only in whole texels while the camera
   walks or turns, so shadow edges do not crawl. */
static void test_cascades_move_in_whole_texels(void) {
    const csm_config_t c = config();
    const float probe[3] = {5.0F, 0.0F, -12.0F};
    csm_cascades_t a, b;
    const csm_camera_t steps[] = {camera(0.0F, 0.0F, 0.0F), camera(0.013F, -0.021F, 0.0F), camera(0.4F, 0.3F, 0.0F), camera(0.4F, 0.3F, 0.7F)};
    csm_compute(&c, &steps[0], &a);
    for (unsigned s = 1; s < sizeof steps / sizeof steps[0]; ++s) {
        csm_compute(&c, &steps[s], &b);
        for (uint32_t i = 0; i < a.count; ++i) {
            TEST_ASSERT_TRUE(a.texel_world[i] == b.texel_world[i]);
            float pa[4], pb[4];
            transform(a.render_vp[i], probe, pa);
            transform(b.render_vp[i], probe, pb);
            for (int k = 0; k < 2; ++k) {
                const float texels = (pb[k] - pa[k]) * 0.5F * (float)c.resolution;
                ASSERT_CLOSE(2e-2F, roundf(texels), texels);
            }
        }
    }
}

/* Every corner of every slice, not only the view axis, lands inside its tile after snapping. */
static void test_slice_corners_stay_inside_their_tiles(void) {
    const csm_config_t c = config();
    const csm_camera_t cams[] = {camera(3.0F, -40.0F, 0.4F), camera(-7.3F, 12.9F, 2.2F), camera(0.017F, 0.0F, -1.1F)};
    for (unsigned k = 0; k < sizeof cams / sizeof cams[0]; ++k) {
        const csm_camera_t *cam = &cams[k];
        csm_cascades_t out;
        csm_compute(&c, cam, &out);
        float f[3] = {cam->target[0] - cam->eye[0], cam->target[1] - cam->eye[1], cam->target[2] - cam->eye[2]};
        const float fl = sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
        for (int i = 0; i < 3; ++i) f[i] /= fl;
        float r[3] = {-f[2], 0.0F, f[0]};
        const float rl = sqrtf(r[0] * r[0] + r[2] * r[2]);
        r[0] /= rl;
        r[2] /= rl;
        const float u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};
        float near = cam->near_plane;
        const float t = tanf(0.5F * cam->fov_y);
        for (uint32_t i = 0; i < out.count; ++i) {
            for (int corner = 0; corner < 8; ++corner) {
                const float z = (corner & 4) != 0 ? out.split_far[i] : near;
                const float h = z * t * ((corner & 1) != 0 ? 1.0F : -1.0F);
                const float w = z * t * cam->aspect * ((corner & 2) != 0 ? 1.0F : -1.0F);
                float p[3], clip[4];
                for (int a = 0; a < 3; ++a) p[a] = cam->eye[a] + f[a] * z + r[a] * w + u[a] * h;
                transform(out.render_vp[i], p, clip);
                /* A PCF texel of margin inside the tile, and a depth the window holds. */
                const float edge = 1.0F - 2.0F / (float)c.resolution;
                TEST_ASSERT_TRUE(fabsf(clip[0]) <= edge && fabsf(clip[1]) <= edge);
                TEST_ASSERT_TRUE(fabsf(clip[2]) <= 1.0F);
            }
            near = out.split_far[i];
        }
    }
}

/* The depth window spans at least the slice sphere, even when the configured range is
   narrower. A caster above the window toward the sun maps in front of the near plane,
   where the depth shaders pancake it. */
static void test_depth_window_holds_the_slice_and_orders_casters(void) {
    csm_config_t c = config();
    c.depth_range = 40.0F;
    const csm_camera_t cam = camera(0.0F, 0.0F, 0.0F);
    csm_cascades_t out;
    csm_compute(&c, &cam, &out);
    for (uint32_t i = 0; i < out.count; ++i) TEST_ASSERT_TRUE(out.half_depth[i] >= 0.5F * c.depth_range);
    const float receiver[3] = {cam.target[0], 0.0F, cam.target[2]};
    float ground[4], crane_top[4], sky[4];
    const float up = 30.0F, far_up = 3.0F * out.half_depth[2];
    const float top[3] = {receiver[0] + c.sun_dir[0] * up, receiver[1] + c.sun_dir[1] * up, receiver[2] + c.sun_dir[2] * up};
    const float beyond[3] = {receiver[0] + c.sun_dir[0] * far_up, receiver[1] + c.sun_dir[1] * far_up, receiver[2] + c.sun_dir[2] * far_up};
    transform(out.render_vp[2], receiver, ground);
    transform(out.render_vp[2], top, crane_top);
    transform(out.render_vp[2], beyond, sky);
    TEST_ASSERT_TRUE(crane_top[2] < ground[2]);
    TEST_ASSERT_TRUE(sky[2] < -1.0F);
}

/* Two cascades share one row: a 2:1 atlas, each tile a half. */
static void test_two_cascades_take_a_wide_atlas(void) {
    csm_config_t c = config();
    c.cascades = 2;
    c.split_far[1] = 60.0F;
    TEST_ASSERT_TRUE(csm_config_valid(&c));
    TEST_ASSERT_EQUAL_UINT32(2048U, csm_atlas_width(&c));
    TEST_ASSERT_EQUAL_UINT32(1024U, csm_atlas_height(&c));
    csm_cascades_t out;
    const csm_camera_t cam = camera(0.0F, 0.0F, 0.0F);
    csm_compute(&c, &cam, &out);
    ASSERT_CLOSE(1e-6F, 0.5F, out.tile[1][0]);
    ASSERT_CLOSE(1e-6F, 1.0F, out.tile[1][3]);
}

static void test_rejects_broken_configs(void) {
    csm_config_t c = config();
    c.cascades = 5;
    TEST_ASSERT_FALSE(csm_config_valid(&c));
    c = config();
    c.resolution = 1000;
    TEST_ASSERT_FALSE(csm_config_valid(&c));
    c = config();
    c.sun_dir[0] = c.sun_dir[1] = c.sun_dir[2] = 0.0F;
    TEST_ASSERT_FALSE(csm_config_valid(&c));
    c = config();
    c.split_far[1] = 10.0F;
    TEST_ASSERT_FALSE(csm_config_valid(&c));
    c = config();
    c.margin_texels = -1.0F;
    TEST_ASSERT_FALSE(csm_config_valid(&c));
    c.margin_texels = NAN;
    TEST_ASSERT_FALSE(csm_config_valid(&c));
    c.margin_texels = INFINITY;
    TEST_ASSERT_FALSE(csm_config_valid(&c));
}

/* A caster between the sun and a cascade's depth window still casts (the depth shaders
   pancake it onto the near plane), so the light's culling planes keep it; the sides and
   the far plane still drop what cannot reach the tile. */
static void test_light_culling_keeps_sunward_casters(void) {
    const csm_config_t c = config();
    const csm_camera_t cam = camera(3.0F, -40.0F, 0.4F);
    csm_cascades_t out;
    csm_compute(&c, &cam, &out);
    float sun[3] = {c.sun_dir[0], c.sun_dir[1], c.sun_dir[2]};
    const float length = sqrtf(sun[0] * sun[0] + sun[1] * sun[1] + sun[2] * sun[2]);
    for (int k = 0; k < 3; ++k) sun[k] /= length;
    /* Across the light: perpendicular to the sun, in the ground plane. */
    float side[3] = {-sun[2], 0.0F, sun[0]};
    const float side_length = sqrtf(side[0] * side[0] + side[2] * side[2]);
    side[0] /= side_length;
    side[2] /= side_length;
    for (uint32_t i = 0; i < out.count; ++i) {
        frustum_t view, light;
        frustum_from_view_proj(&view, out.render_vp[i]);
        frustum_from_light_view_proj(&light, out.render_vp[i]);
        const float beyond = out.half_depth[i] + 5.0F;
        /* Toward the sun past the window, anti-sunward past it, and off the tile's side. */
        const float offsets[3][3] = {
            {sun[0] * beyond * 2.0F, sun[1] * beyond * 2.0F, sun[2] * beyond * 2.0F},
            {-sun[0] * beyond * 2.0F, -sun[1] * beyond * 2.0F, -sun[2] * beyond * 2.0F},
            {side[0] * beyond * 4.0F, 0.0F, side[2] * beyond * 4.0F},
        };
        /* The middle of the cascade's view slice. */
        const float slice_near = i == 0U ? cam.near_plane : out.split_far[i - 1U];
        const float middle = 0.5F * (slice_near + out.split_far[i]);
        float forward[3] = {cam.target[0] - cam.eye[0], cam.target[1] - cam.eye[1], cam.target[2] - cam.eye[2]};
        const float forward_length = sqrtf(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
        const float target[3] = {cam.eye[0] + forward[0] / forward_length * middle, cam.eye[1] + forward[1] / forward_length * middle,
                                 cam.eye[2] + forward[2] / forward_length * middle};
        float min[3][3], max[3][3];
        for (int o = 0; o < 3; ++o) {
            for (int k = 0; k < 3; ++k) {
                min[o][k] = target[k] + offsets[o][k] - 0.5F;
                max[o][k] = target[k] + offsets[o][k] + 0.5F;
            }
        }
        TEST_ASSERT_FALSE(frustum_sees_box(&view, min[0], max[0], 0.05F));
        TEST_ASSERT_TRUE(frustum_sees_box(&light, min[0], max[0], 0.05F));
        TEST_ASSERT_FALSE(frustum_sees_box(&light, min[1], max[1], 0.05F));
        TEST_ASSERT_FALSE(frustum_sees_box(&light, min[2], max[2], 0.05F));
        /* The receivers at the slice keep drawing under both plane sets. */
        const float here_min[3] = {target[0] - 0.5F, target[1] - 0.5F, target[2] - 0.5F};
        const float here_max[3] = {target[0] + 0.5F, target[1] + 0.5F, target[2] + 0.5F};
        TEST_ASSERT_TRUE(frustum_sees_box(&view, here_min, here_max, 0.05F));
        TEST_ASSERT_TRUE(frustum_sees_box(&light, here_min, here_max, 0.05F));
    }
}

static void test_cascade_centre_stays_on_the_texel_grid(void) {
    const csm_config_t c = config();
    for (int step = 0; step < 20; ++step) {
        csm_cascades_t out;
        const csm_camera_t view = camera(0.37F * (float)step, 1.9F - 0.21F * (float)step, 0.13F * (float)step);
        csm_compute(&c, &view, &out);
        for (uint32_t i = 0; i < out.count; ++i) {
            /* render_vp translates by -centre / radius; a texel is 2 / resolution of that. */
            for (int axis = 0; axis < 2; ++axis) {
                const float texels = -out.render_vp[i][12 + axis] * 0.5F * (float)c.resolution;
                TEST_ASSERT_TRUE(fabsf(roundf(texels) - texels) <= 0.05F);
            }
        }
    }
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_slice_sphere_holds_its_corners);
    RUN_TEST(test_each_cascade_holds_its_whole_slice);
    RUN_TEST(test_lookup_lands_in_the_cascade_tile);
    RUN_TEST(test_cascades_move_in_whole_texels);
    RUN_TEST(test_cascade_centre_stays_on_the_texel_grid);
    RUN_TEST(test_slice_corners_stay_inside_their_tiles);
    RUN_TEST(test_depth_window_holds_the_slice_and_orders_casters);
    RUN_TEST(test_two_cascades_take_a_wide_atlas);
    RUN_TEST(test_rejects_broken_configs);
    RUN_TEST(test_light_culling_keeps_sunward_casters);
    return UNITY_END();
}
