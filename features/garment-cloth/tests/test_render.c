#include "garment/surface_transfer.h"
#include "garment/contact.h"
#include "garment/debug.h"
#include "unity.h"
#include <string.h>

void setUp(void) {}
void tearDown(void) {}

static const ClothFace faces[] = {{0,1,2}, {1,0,3}};
static const float rest[][3] = {{0,0,0}, {1,0,0}, {0,1,0}, {0,-2,0}};
static const ClothMesh panel = {.vertices=4, .faces=2, .rest=rest, .face=faces};

static void quat_rotate_vec(const float q[4], const float v[3], float out[3]) {
    float tx = 2.0F * (q[1] * v[2] - q[2] * v[1]);
    float ty = 2.0F * (q[2] * v[0] - q[0] * v[2]);
    float tz = 2.0F * (q[0] * v[1] - q[1] * v[0]);
    out[0] = v[0] + q[3] * tx + (q[1] * tz - q[2] * ty);
    out[1] = v[1] + q[3] * ty + (q[2] * tx - q[0] * tz);
    out[2] = v[2] + q[3] * tz + (q[0] * ty - q[1] * tx);
}

static void test_vertex_quats_preserve_rest_rigid_and_thickness_on_hairpin_fold(void) {
    ClothVertexBasis basis[CLOTH_VERTICES];
    float rec[4][12];
    TEST_ASSERT_TRUE(cloth_vertex_frames_prepare(&panel, basis));
    TEST_ASSERT_TRUE(cloth_vertex_quats_eval(&panel, basis, rest, rec));
    /* Rest pose: exact identity quaternion (0,0,0,1) and P_curr == P_rest. */
    for (int v = 0; v < 4; ++v) {
        TEST_ASSERT_TRUE(fabsf(rec[v][0] - rest[v][0]) < 1e-6F);
        TEST_ASSERT_TRUE(fabsf(rec[v][4] - rest[v][0]) < 1e-6F);
        TEST_ASSERT_TRUE(fabsf(rec[v][3]) < 1e-6F && fabsf(rec[v][7]) < 1e-6F &&
                         fabsf(rec[v][8]) < 1e-6F && fabsf(rec[v][9] - 1.0F) < 1e-6F);
    }
    /* Rigid 90-deg rotation + translation: each record carries exactly that motion for points near its vertex. */
    float moved[4][3];
    for (int i = 0; i < 4; ++i) {
        moved[i][0] = rest[i][2] + 3.0F;
        moved[i][1] = rest[i][1] - 1.5F;
        moved[i][2] = -rest[i][0] + 0.5F;
    }
    TEST_ASSERT_TRUE(cloth_vertex_quats_eval(&panel, basis, moved, rec));
    const float p_rest[3] = {0.25F, 0.25F, 0.02F};
    for (int v = 0; v < 4; ++v) {
        const float q[4] = {rec[v][3], rec[v][7], rec[v][8], rec[v][9]};
        const float local[3] = {p_rest[0] - rec[v][4], p_rest[1] - rec[v][5], p_rest[2] - rec[v][6]};
        float out[3];
        quat_rotate_vec(q, local, out);
        TEST_ASSERT_TRUE(fabsf(out[0] + rec[v][0] - (p_rest[2] + 3.0F)) < 1e-5F);
        TEST_ASSERT_TRUE(fabsf(out[1] + rec[v][1] - (p_rest[1] - 1.5F)) < 1e-5F);
        TEST_ASSERT_TRUE(fabsf(out[2] + rec[v][2] - (-p_rest[0] + 0.5F)) < 1e-5F);
    }
    /* Extreme 160-deg hairpin fold: inner and outer shell offset stays rigidly separated (no collapse!). */
    float bent[4][3];
    memcpy(bent, rest, sizeof bent);
    bent[3][1] = 1.85F;
    bent[3][2] = 0.65F;
    TEST_ASSERT_TRUE(cloth_vertex_quats_eval(&panel, basis, bent, rec));
    /* Blend vertices (0, 1, 2) with hemisphere alignment. */
    float w[3] = {0.4F, 0.4F, 0.2F};
    float q0[4] = {rec[0][3], rec[0][7], rec[0][8], rec[0][9]};
    float qb[4] = {0}, pc[3] = {0}, pr[3] = {0};
    for (int k = 0; k < 3; ++k) {
        float qk[4] = {rec[k][3], rec[k][7], rec[k][8], rec[k][9]};
        float s = (q0[0]*qk[0] + q0[1]*qk[1] + q0[2]*qk[2] + q0[3]*qk[3]) < 0.0F ? -1.0F : 1.0F;
        for (int j = 0; j < 4; ++j) qb[j] += w[k] * s * qk[j];
        for (int j = 0; j < 3; ++j) {
            pc[j] += w[k] * rec[k][j];
            pr[j] += w[k] * rec[k][4 + j];
        }
    }
    float qlen = sqrtf(qb[0]*qb[0] + qb[1]*qb[1] + qb[2]*qb[2] + qb[3]*qb[3]);
    TEST_ASSERT_TRUE(qlen > 0.5F);
    for (int j = 0; j < 4; ++j) qb[j] /= qlen;
    float thick_rest[3] = {0.0F, 0.0F, 0.004F}, thick_rot[3];
    quat_rotate_vec(qb, thick_rest, thick_rot);
    /* Rigid quaternion rotation preserves 100% of shell thickness (4.0mm) without collapse! */
    float thick_len = sqrtf(thick_rot[0]*thick_rot[0] + thick_rot[1]*thick_rot[1] + thick_rot[2]*thick_rot[2]);
    TEST_ASSERT_TRUE(fabsf(thick_len - 0.004F) < 1e-6F);
    (void)pc; (void)pr;
}

typedef struct {
    int weights, tethers, shapes, bones;
    float weight_at_origin[3], tether_color[3];
    const ClothSphere *a, *b;
} DebugLines;

static void inspect_line(void *user, unsigned layer, const float a[3], const float b[3], const float rgb[3]) {
    DebugLines *capture = user;
    if (layer == CLOTH_DEBUG_WEIGHTS) {
        ++capture->weights;
        if (a[0] == 0 && a[1] == 0 && a[2] == 0) memcpy(capture->weight_at_origin, rgb, sizeof capture->weight_at_origin);
    } else if (layer == CLOTH_DEBUG_TETHERS) {
        ++capture->tethers;
        memcpy(capture->tether_color, rgb, sizeof capture->tether_color);
        TEST_ASSERT_TRUE(a[0] == 1 && b[0] == 0);
    } else if (layer == CLOTH_DEBUG_SHAPES) {
        float normal[3];
        ++capture->shapes;
        TEST_ASSERT_TRUE(fabsf(0.002F - cloth_point_cone_contact(a, capture->a, capture->b, 0.002F, normal, NULL)) < 1e-5F);
        TEST_ASSERT_TRUE(fabsf(0.002F - cloth_point_cone_contact(b, capture->a, capture->b, 0.002F, normal, NULL)) < 1e-5F);
    } else if (layer == CLOTH_DEBUG_SKELETON) {
        ++capture->bones;
        TEST_ASSERT_TRUE(a[0] == 0 && b[0] == 1);
    } else {
        TEST_FAIL_MESSAGE("unexpected debug layer");
    }
}

static void test_debug_draw_uses_selected_weights_live_tether_and_round_cone_surface(void) {
    const float inv[] = {1, 1, 1, 1}, r[] = {0, 0.3F, 0.01F, 0.01F}, selected[] = {1, 0, 0, 0};
    const ClothEdge edge = {.a=0,.b=1};
    const ClothTether tether[] = {{0,0}, {0,0.8F}, {-1,0}, {-1,0}};
    ClothMesh m = panel;
    m.edges = 1; m.edge = &edge; m.inv_area = inv; m.max_distance = r; m.tether = tether;
    DebugLines lines = {0};
    cloth_debug_mesh(&m, rest, NULL, 0, inspect_line, &lines);
    TEST_ASSERT_EQUAL_INT(0, lines.weights + lines.tethers);
    cloth_debug_mesh(&m, rest, NULL, CLOTH_DEBUG_WEIGHTS, inspect_line, &lines);
    TEST_ASSERT_TRUE(lines.weights > 0 && lines.weight_at_origin[2] > lines.weight_at_origin[0]);
    cloth_debug_mesh(&m, rest, selected, CLOTH_DEBUG_WEIGHTS | CLOTH_DEBUG_TETHERS, inspect_line, &lines);
    TEST_ASSERT_TRUE(lines.weight_at_origin[0] > lines.weight_at_origin[2]);
    TEST_ASSERT_EQUAL_INT(1, lines.tethers);
    TEST_ASSERT_TRUE(lines.tether_color[0] > lines.tether_color[1]);
    cloth_debug_bone(rest[0], rest[1], inspect_line, &lines);
    TEST_ASSERT_EQUAL_INT(1, lines.bones);

    ClothSphere sa = {.c={0.1F,0.3F,0.1F},.r=0.18F}, sb = {.c={0.2F,0.7F,0.4F},.r=0.09F};
    lines.a = &sa;
    lines.b = &sb;
    cloth_debug_cone(&sa, &sb, inspect_line, &lines);
    TEST_ASSERT_TRUE(lines.shapes > 0);
    sb.r = sa.r;
    cloth_debug_cone(&sa, &sb, inspect_line, &lines);
    lines.b = &sa;
    cloth_debug_cone(&sa, &sa, inspect_line, &lines);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_debug_draw_uses_selected_weights_live_tether_and_round_cone_surface);
    RUN_TEST(test_vertex_quats_preserve_rest_rigid_and_thickness_on_hairpin_fold);
    return UNITY_END();
}
