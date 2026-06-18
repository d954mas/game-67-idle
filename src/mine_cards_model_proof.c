#include "mine_cards_model_proof.h"

#include "app/nt_app.h"
#include "core/nt_core.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "math/nt_math.h"
#include "render/nt_render_defs.h"
#include "time/nt_time.h"

#include <glad/gl.h>

#if SKELETAL_ANIMATION_EXTENSION_ENABLED
#include "skeletal_animation/nt_skeletal_animation.h"
#include "skeletal_animation/nt_skeletal_mesh.h"
#endif

#include "mine_cards_kaykit_mesh.gen.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MineCardsRuntimeVertex {
    float position[3];
    float color[4];
} MineCardsRuntimeVertex;

#define MINE_CARDS_TOTAL_VERTEX_COUNT (MINE_CARDS_KAYKIT_VERTEX_COUNT + MINE_CARDS_PICKAXE_VERTEX_COUNT)
#define MINE_CARDS_TOTAL_INDEX_COUNT (MINE_CARDS_KAYKIT_INDEX_COUNT + MINE_CARDS_PICKAXE_INDEX_COUNT)

static const char *const k_vs_source =
    "precision highp float;\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec4 a_color;\n"
    "uniform mat4 u_mvp;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    v_color = a_color;\n"
    "    gl_Position = u_mvp * vec4(a_position, 1.0);\n"
    "}\n";

static const char *const k_fs_source =
    "precision mediump float;\n"
    "in vec4 v_color;\n"
    "out vec4 frag_color;\n"
    "void main() {\n"
    "    frag_color = v_color;\n"
    "}\n";

static MineCardsRuntimeVertex s_vertices[MINE_CARDS_TOTAL_VERTEX_COUNT];
static uint32_t s_indices[MINE_CARDS_TOTAL_INDEX_COUNT];
static int s_source_to_ozz[MINE_CARDS_KAYKIT_JOINT_COUNT];
static int s_hand_joint = -1;
static nt_shader_t s_vs;
static nt_shader_t s_fs;
static nt_pipeline_t s_pipeline;
static nt_buffer_t s_vbo;
static nt_buffer_t s_ibo;
static bool s_initialized;
static bool s_skeleton_bound;
static bool s_has_pose;

#if SKELETAL_ANIMATION_EXTENSION_ENABLED
static nt_skeletal_mesh_t *s_reusable_mesh;
static nt_skeletal_mesh_instance_t *s_reusable_mesh_instance;
static nt_skeletal_mesh_vertex_t *s_reusable_vertices;
static bool s_reusable_mesh_ready;
static bool s_reusable_mesh_error_reported;
static bool s_reusable_joint_map_ready;
static float s_reusable_model_matrices[MINE_CARDS_KAYKIT_JOINT_COUNT * 16];
static float s_reusable_skinned_positions[MINE_CARDS_KAYKIT_VERTEX_COUNT * 3];
static float s_reusable_tool_socket[16];
static double s_reusable_skin_total_ms;
static int s_reusable_skin_samples;
#endif

static void mat4_identity(float out[16]) {
    memset(out, 0, sizeof(float) * 16);
    out[0] = 1.0F;
    out[5] = 1.0F;
    out[10] = 1.0F;
    out[15] = 1.0F;
}

static void mat4_mul(const float a[16], const float b[16], float out[16]) {
    float r[16];
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            r[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0] + a[1 * 4 + row] * b[col * 4 + 1] + a[2 * 4 + row] * b[col * 4 + 2] + a[3 * 4 + row] * b[col * 4 + 3];
        }
    }
    memcpy(out, r, sizeof(r));
}

static void mat4_transform_point(const float m[16], const float p[3], float out[3]) {
    out[0] = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
    out[1] = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
    out[2] = m[2] * p[0] + m[6] * p[1] + m[10] * p[2] + m[14];
}

static void convert_y_up_to_z_up(const float in[3], float out[3]) {
    out[0] = in[0];
    out[1] = -in[2];
    out[2] = in[1];
}

static void make_rotation_x(float radians, float out[16]) {
    mat4_identity(out);
    const float c = cosf(radians);
    const float s = sinf(radians);
    out[5] = c;
    out[6] = s;
    out[9] = -s;
    out[10] = c;
}

static void make_rotation_z(float radians, float out[16]) {
    mat4_identity(out);
    const float c = cosf(radians);
    const float s = sinf(radians);
    out[0] = c;
    out[1] = s;
    out[4] = -s;
    out[5] = c;
}

static void make_trs(float tx, float ty, float tz, float scale, float out[16]) {
    mat4_identity(out);
    out[0] = scale;
    out[5] = scale;
    out[10] = scale;
    out[12] = tx;
    out[13] = ty;
    out[14] = tz;
}

#if SKELETAL_ANIMATION_EXTENSION_ENABLED
static void create_reusable_mesh(void) {
    if (s_reusable_mesh_ready || s_reusable_mesh != NULL || s_reusable_mesh_instance != NULL) {
        return;
    }

    s_reusable_vertices = (nt_skeletal_mesh_vertex_t *)calloc(MINE_CARDS_KAYKIT_VERTEX_COUNT, sizeof(nt_skeletal_mesh_vertex_t));
    if (s_reusable_vertices == NULL) {
        (void)fprintf(stderr, "skeletal mesh proof: failed to allocate reusable vertex descriptor\n");
        return;
    }

    for (uint32_t i = 0; i < MINE_CARDS_KAYKIT_VERTEX_COUNT; ++i) {
        const MineCardsKayKitSkinnedVertex *src = &k_mine_cards_kaykit_vertices[i];
        memcpy(s_reusable_vertices[i].position, src->position, sizeof(src->position));
        s_reusable_vertices[i].normal[1] = 1.0F;
        memcpy(s_reusable_vertices[i].joints, src->joints, sizeof(src->joints));
        memcpy(s_reusable_vertices[i].weights, src->weights, sizeof(src->weights));
    }

    const nt_skeletal_mesh_material_slot_t material_slots[] = {
        {
            .name = "kaykit_vertex_color",
            .index_offset = 0,
            .index_count = MINE_CARDS_KAYKIT_INDEX_COUNT,
            .texture_id = -1,
        },
    };
    const nt_skeletal_mesh_socket_t sockets[] = {
        {
            .name = "tool",
            .joint_name = "handslot.l",
            .local_offset = {0.13F, -0.09F, -0.15F},
            .local_rotation_quat = {0.652198275F, 0.199397023F, -0.213827128F, 0.699397023F},
            .local_scale = {1.28F, 1.28F, 1.28F},
        },
    };
    float asset_to_model[16];
    mat4_identity(asset_to_model);

    const nt_skeletal_mesh_desc_t desc = {
        .vertices = s_reusable_vertices,
        .vertex_count = MINE_CARDS_KAYKIT_VERTEX_COUNT,
        .indices = k_mine_cards_kaykit_indices,
        .index_count = MINE_CARDS_KAYKIT_INDEX_COUNT,
        .inverse_bind_matrices = &k_mine_cards_kaykit_inverse_bind[0][0],
        .joint_names = k_mine_cards_kaykit_joint_names,
        .joint_count = MINE_CARDS_KAYKIT_JOINT_COUNT,
        .material_slots = material_slots,
        .material_slot_count = 1,
        .sockets = sockets,
        .socket_count = 1,
    };
    nt_skeletal_mesh_desc_t mutable_desc = desc;
    memcpy(mutable_desc.asset_to_model, asset_to_model, sizeof(asset_to_model));

    char error[512];
    if (!nt_skeletal_mesh_create(&mutable_desc, &s_reusable_mesh, error, sizeof(error))) {
        (void)fprintf(stderr, "skeletal mesh proof: reusable mesh create failed: %s\n", error);
        free(s_reusable_vertices);
        s_reusable_vertices = NULL;
        return;
    }
    if (!nt_skeletal_mesh_instance_create(s_reusable_mesh, &s_reusable_mesh_instance, error, sizeof(error))) {
        (void)fprintf(stderr, "skeletal mesh proof: reusable mesh instance failed: %s\n", error);
        nt_skeletal_mesh_destroy(s_reusable_mesh);
        s_reusable_mesh = NULL;
        free(s_reusable_vertices);
        s_reusable_vertices = NULL;
        return;
    }
    s_reusable_mesh_ready = true;
}

static void destroy_reusable_mesh(void) {
    if (s_reusable_mesh_instance != NULL) {
        nt_skeletal_mesh_instance_destroy(s_reusable_mesh_instance);
    }
    if (s_reusable_mesh != NULL) {
        nt_skeletal_mesh_destroy(s_reusable_mesh);
    }
    free(s_reusable_vertices);
    s_reusable_mesh_instance = NULL;
    s_reusable_mesh = NULL;
    s_reusable_vertices = NULL;
    s_reusable_mesh_ready = false;
    s_reusable_joint_map_ready = false;
    s_reusable_skin_total_ms = 0.0;
    s_reusable_skin_samples = 0;
}
#endif

static void copy_indices(void) {
    memcpy(s_indices, k_mine_cards_kaykit_indices, sizeof(k_mine_cards_kaykit_indices));
    for (uint32_t i = 0; i < MINE_CARDS_PICKAXE_INDEX_COUNT; ++i) {
        s_indices[MINE_CARDS_KAYKIT_INDEX_COUNT + i] = k_mine_cards_pickaxe_indices[i] + MINE_CARDS_KAYKIT_VERTEX_COUNT;
    }
}

static void create_gpu_resources(void) {
    s_vs = nt_gfx_make_shader(&(nt_shader_desc_t){
        .type = NT_SHADER_VERTEX,
        .source = k_vs_source,
        .label = "mine_cards_kaykit_skin_vs",
    });
    s_fs = nt_gfx_make_shader(&(nt_shader_desc_t){
        .type = NT_SHADER_FRAGMENT,
        .source = k_fs_source,
        .label = "mine_cards_kaykit_skin_fs",
    });

    s_pipeline = nt_gfx_make_pipeline(&(nt_pipeline_desc_t){
        .vertex_shader = s_vs,
        .fragment_shader = s_fs,
        .layout =
            {
                .attrs =
                    {
                        {.location = 0, .format = NT_FORMAT_FLOAT3, .offset = 0},
                        {.location = 1, .format = NT_FORMAT_FLOAT4, .offset = 12},
                    },
                .attr_count = 2,
                .stride = sizeof(MineCardsRuntimeVertex),
            },
        .depth_test = true,
        .depth_write = true,
        .depth_func = NT_DEPTH_LESS,
        .cull_mode = 0,
        .label = "mine_cards_kaykit_skin_pipeline",
    });

    s_vbo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_VERTEX,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(s_vertices),
        .label = "mine_cards_kaykit_skin_vbo",
    });
    s_ibo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_INDEX,
        .usage = NT_USAGE_IMMUTABLE,
        .data = s_indices,
        .size = sizeof(s_indices),
        .index_type = NT_INDEX_UINT32,
        .label = "mine_cards_kaykit_skin_ibo",
    });
}

static void destroy_gpu_resources(void) {
    if (s_ibo.id != 0) {
        nt_gfx_destroy_buffer(s_ibo);
    }
    if (s_vbo.id != 0) {
        nt_gfx_destroy_buffer(s_vbo);
    }
    if (s_pipeline.id != 0) {
        nt_gfx_destroy_pipeline(s_pipeline);
    }
    if (s_fs.id != 0) {
        nt_gfx_destroy_shader(s_fs);
    }
    if (s_vs.id != 0) {
        nt_gfx_destroy_shader(s_vs);
    }
    s_ibo = (nt_buffer_t){0};
    s_vbo = (nt_buffer_t){0};
    s_pipeline = (nt_pipeline_t){0};
    s_fs = (nt_shader_t){0};
    s_vs = (nt_shader_t){0};
}

void mine_cards_model_proof_init(void) {
    if (s_initialized) {
        return;
    }
    for (uint32_t i = 0; i < MINE_CARDS_KAYKIT_JOINT_COUNT; ++i) {
        s_source_to_ozz[i] = -1;
    }
    copy_indices();
    create_gpu_resources();
#if SKELETAL_ANIMATION_EXTENSION_ENABLED
    create_reusable_mesh();
#endif
    s_initialized = true;
}

void mine_cards_model_proof_bind_skeleton(nt_skeletal_anim_clip_t *clip) {
    (void)clip;
#if SKELETAL_ANIMATION_EXTENSION_ENABLED
    if (clip == NULL) {
        return;
    }
    for (uint32_t i = 0; i < MINE_CARDS_KAYKIT_JOINT_COUNT; ++i) {
        s_source_to_ozz[i] = nt_skeletal_anim_find_joint(clip, k_mine_cards_kaykit_joint_names[i]);
    }
    s_hand_joint = nt_skeletal_anim_find_joint(clip, "handslot.l");
    s_reusable_joint_map_ready = true;
    for (uint32_t i = 0; i < MINE_CARDS_KAYKIT_JOINT_COUNT; ++i) {
        if (s_source_to_ozz[i] < 0) {
            s_reusable_joint_map_ready = false;
        }
    }
    if (!s_reusable_joint_map_ready) {
        (void)fprintf(stderr, "skeletal mesh proof: joint map incomplete; falling back to game-local skinning\n");
    }
    s_skeleton_bound = true;
#endif
}

void mine_cards_model_proof_step(float swing, float sweep) {
    (void)swing;
    (void)sweep;
}

static void skin_character_vertices(const float *model_matrices, int matrix_count) {
    for (uint32_t i = 0; i < MINE_CARDS_KAYKIT_VERTEX_COUNT; ++i) {
        const MineCardsKayKitSkinnedVertex *src = &k_mine_cards_kaykit_vertices[i];
        float skinned_y_up[3] = {0.0F, 0.0F, 0.0F};

        for (uint32_t influence = 0; influence < 4; ++influence) {
            const float weight = src->weights[influence];
            if (weight <= 0.0F) {
                continue;
            }
            const uint16_t source_joint = src->joints[influence];
            if (source_joint >= MINE_CARDS_KAYKIT_JOINT_COUNT) {
                continue;
            }
            const int ozz_joint = s_source_to_ozz[source_joint];
            if (ozz_joint < 0 || ozz_joint >= matrix_count) {
                continue;
            }

            float skin_matrix[16];
            float transformed[3];
            mat4_mul(model_matrices + ((uint32_t)ozz_joint * 16U), k_mine_cards_kaykit_inverse_bind[source_joint], skin_matrix);
            mat4_transform_point(skin_matrix, src->position, transformed);
            skinned_y_up[0] += transformed[0] * weight;
            skinned_y_up[1] += transformed[1] * weight;
            skinned_y_up[2] += transformed[2] * weight;
        }

        convert_y_up_to_z_up(skinned_y_up, s_vertices[i].position);
        s_vertices[i].position[0] *= 1.18F;
        s_vertices[i].position[1] *= 1.18F;
        s_vertices[i].position[2] *= 1.18F;
        s_vertices[i].position[2] -= 0.05F;
        memcpy(s_vertices[i].color, src->color, sizeof(src->color));
    }
}

static void skin_pickaxe_vertices_from_model(const float pickaxe_model[16]) {
    for (uint32_t i = 0; i < MINE_CARDS_PICKAXE_VERTEX_COUNT; ++i) {
        float y_up[3];
        const float local[3] = {
            k_mine_cards_pickaxe_vertices[i].position[0],
            k_mine_cards_pickaxe_vertices[i].position[1],
            k_mine_cards_pickaxe_vertices[i].position[2],
        };
        mat4_transform_point(pickaxe_model, local, y_up);
        convert_y_up_to_z_up(y_up, s_vertices[MINE_CARDS_KAYKIT_VERTEX_COUNT + i].position);
        s_vertices[MINE_CARDS_KAYKIT_VERTEX_COUNT + i].position[0] *= 1.18F;
        s_vertices[MINE_CARDS_KAYKIT_VERTEX_COUNT + i].position[1] *= 1.18F;
        s_vertices[MINE_CARDS_KAYKIT_VERTEX_COUNT + i].position[2] *= 1.18F;
        s_vertices[MINE_CARDS_KAYKIT_VERTEX_COUNT + i].position[2] -= 0.05F;
        memcpy(s_vertices[MINE_CARDS_KAYKIT_VERTEX_COUNT + i].color, (float[4]){0.86F, 0.72F, 0.50F, 1.0F}, sizeof(float) * 4);
    }
}

static void skin_pickaxe_vertices(const float *model_matrices, int matrix_count) {
    float pickaxe_model[16];
    if (s_hand_joint >= 0 && s_hand_joint < matrix_count) {
        memcpy(pickaxe_model, model_matrices + ((uint32_t)s_hand_joint * 16U), sizeof(pickaxe_model));
    } else {
        mat4_identity(pickaxe_model);
        pickaxe_model[12] = 0.38F;
        pickaxe_model[13] = 1.0F;
        pickaxe_model[14] = 0.0F;
    }

    float trs[16];
    float rx[16];
    float rz[16];
    float tmp[16];
    float orient[16];
    make_trs(0.13F, -0.09F, -0.15F, 1.28F, trs);
    make_rotation_x(glm_rad(86.0F), rx);
    make_rotation_z(glm_rad(-34.0F), rz);
    mat4_mul(rx, rz, tmp);
    mat4_mul(trs, tmp, orient);
    mat4_mul(pickaxe_model, orient, tmp);
    skin_pickaxe_vertices_from_model(tmp);
}

#if SKELETAL_ANIMATION_EXTENSION_ENABLED
static bool skin_character_with_reusable_mesh(const float *model_matrices, int matrix_count) {
    if (!s_reusable_mesh_ready || !s_reusable_joint_map_ready || s_reusable_mesh_instance == NULL || model_matrices == NULL || matrix_count <= 0) {
        return false;
    }

    for (uint32_t i = 0; i < MINE_CARDS_KAYKIT_JOINT_COUNT; ++i) {
        const int ozz_joint = s_source_to_ozz[i];
        if (ozz_joint < 0 || ozz_joint >= matrix_count) {
            return false;
        }
        memcpy(s_reusable_model_matrices + (i * 16U), model_matrices + ((uint32_t)ozz_joint * 16U), sizeof(float) * 16U);
    }

    char error[512];
    const double t0 = nt_time_now();
    if (!nt_skeletal_mesh_instance_update_pose(s_reusable_mesh_instance,
                                               s_reusable_model_matrices,
                                               MINE_CARDS_KAYKIT_JOINT_COUNT,
                                               error,
                                               sizeof(error))) {
        if (!s_reusable_mesh_error_reported) {
            (void)fprintf(stderr, "skeletal mesh proof: reusable pose update failed: %s\n", error);
            s_reusable_mesh_error_reported = true;
        }
        return false;
    }
    const int copied = nt_skeletal_mesh_instance_copy_skinned_positions(s_reusable_mesh_instance,
                                                                        s_reusable_skinned_positions,
                                                                        MINE_CARDS_KAYKIT_VERTEX_COUNT,
                                                                        error,
                                                                        sizeof(error));
    if (copied != (int)MINE_CARDS_KAYKIT_VERTEX_COUNT) {
        if (!s_reusable_mesh_error_reported) {
            (void)fprintf(stderr, "skeletal mesh proof: reusable position copy failed: %s\n", error);
            s_reusable_mesh_error_reported = true;
        }
        return false;
    }
    if (!nt_skeletal_mesh_instance_socket_matrix(s_reusable_mesh_instance, "tool", s_reusable_tool_socket, error, sizeof(error))) {
        if (!s_reusable_mesh_error_reported) {
            (void)fprintf(stderr, "skeletal mesh proof: reusable tool socket failed: %s\n", error);
            s_reusable_mesh_error_reported = true;
        }
        return false;
    }
    const double elapsed_ms = (nt_time_now() - t0) * 1000.0;
    if (s_reusable_skin_samples < 120) {
        s_reusable_skin_total_ms += elapsed_ms;
        ++s_reusable_skin_samples;
        if (s_reusable_skin_samples == 120) {
            (void)fprintf(stderr,
                          "skeletal mesh proof: reusable CPU skin avg=%.3fms vertices=%u joints=%u budget<=0.500ms\n",
                          s_reusable_skin_total_ms / (double)s_reusable_skin_samples,
                          (unsigned)MINE_CARDS_KAYKIT_VERTEX_COUNT,
                          (unsigned)MINE_CARDS_KAYKIT_JOINT_COUNT);
        }
    }

    for (uint32_t i = 0; i < MINE_CARDS_KAYKIT_VERTEX_COUNT; ++i) {
        float z_up[3];
        convert_y_up_to_z_up(s_reusable_skinned_positions + (i * 3U), z_up);
        s_vertices[i].position[0] = z_up[0] * 1.18F;
        s_vertices[i].position[1] = z_up[1] * 1.18F;
        s_vertices[i].position[2] = z_up[2] * 1.18F - 0.05F;
        memcpy(s_vertices[i].color, k_mine_cards_kaykit_vertices[i].color, sizeof(s_vertices[i].color));
    }
    skin_pickaxe_vertices_from_model(s_reusable_tool_socket);
    return true;
}
#endif

void mine_cards_model_proof_step_ozz(const float *model_matrices, int matrix_count) {
    if (!s_initialized || !s_skeleton_bound || model_matrices == NULL || matrix_count <= 0) {
        return;
    }
#if SKELETAL_ANIMATION_EXTENSION_ENABLED
    if (!skin_character_with_reusable_mesh(model_matrices, matrix_count)) {
        skin_character_vertices(model_matrices, matrix_count);
        skin_pickaxe_vertices(model_matrices, matrix_count);
    }
#else
    skin_character_vertices(model_matrices, matrix_count);
    skin_pickaxe_vertices(model_matrices, matrix_count);
#endif
    if (s_vbo.id != 0) {
        nt_gfx_update_buffer(s_vbo, s_vertices, sizeof(s_vertices));
    }
    s_has_pose = true;
}

bool mine_cards_model_proof_can_draw(void) {
    return s_initialized && s_skeleton_bound && s_has_pose && s_pipeline.id != 0 && s_vbo.id != 0 && s_ibo.id != 0;
}

static void draw_model_with_aspect(float aspect, bool screen_portrait) {
    mat4 view_m;
    mat4 proj_m;
    mat4 vp;
    mat4 model_m;
    mat4 mvp;
    glm_lookat((vec3){0.0F, -6.4F, 1.35F}, (vec3){0.04F, 0.0F, 0.88F}, (vec3){0.0F, 0.0F, 1.0F}, view_m);
    glm_perspective(glm_rad(screen_portrait ? 38.0F : 42.0F), aspect, 0.1F, 20.0F, proj_m);
    glm_mat4_mul(proj_m, view_m, vp);
    glm_mat4_identity(model_m);
    const float model_scale = screen_portrait ? 0.58F : 0.66F;
    const float model_x = screen_portrait ? -0.48F : -0.34F;
    const float model_z = screen_portrait ? 0.64F : 0.58F;
    glm_translate(model_m, (vec3){model_x, 0.0F, model_z});
    glm_rotate_z(model_m, glm_rad(-38.0F), model_m);
    glm_scale_uni(model_m, model_scale);
    glm_mat4_mul(vp, model_m, mvp);

    nt_gfx_bind_pipeline(s_pipeline);
    nt_gfx_set_uniform_mat4("u_mvp", (const float *)mvp);
    nt_gfx_bind_vertex_buffer(s_vbo);
    nt_gfx_bind_index_buffer(s_ibo);
    nt_gfx_draw_indexed(0, MINE_CARDS_TOTAL_INDEX_COUNT, MINE_CARDS_TOTAL_VERTEX_COUNT);
}

void mine_cards_model_proof_draw(float w, float h) {
    if (!mine_cards_model_proof_can_draw()) {
        return;
    }

    const float aspect = h > 0.0F ? w / h : 1.0F;
    draw_model_with_aspect(aspect, h > w * 1.10F);
}

void mine_cards_model_proof_draw_in_box(float screen_w, float screen_h, float x, float y, float w, float h) {
    if (!mine_cards_model_proof_can_draw() || w <= 1.0F || h <= 1.0F) {
        return;
    }

    const int ix = (int)(x + 0.5F);
    const int iy = (int)(y + 0.5F);
    const int iw = (int)(w + 0.5F);
    const int ih = (int)(h + 0.5F);
    glEnable(GL_SCISSOR_TEST);
    glViewport(ix, iy, iw, ih);
    glScissor(ix, iy, iw, ih);
    glClear(GL_DEPTH_BUFFER_BIT);

    const float aspect = h > 0.0F ? w / h : 1.0F;
    draw_model_with_aspect(aspect, screen_h > screen_w * 1.10F);

    glDisable(GL_SCISSOR_TEST);
    glViewport(0, 0, (int)(screen_w + 0.5F), (int)(screen_h + 0.5F));
}

void mine_cards_model_proof_restore_gpu(void) {
    if (!s_initialized) {
        return;
    }
    destroy_gpu_resources();
    create_gpu_resources();
    s_has_pose = false;
}

void mine_cards_model_proof_shutdown(void) {
    if (!s_initialized) {
        return;
    }
    destroy_gpu_resources();
#if SKELETAL_ANIMATION_EXTENSION_ENABLED
    destroy_reusable_mesh();
#endif
    s_initialized = false;
    s_skeleton_bound = false;
    s_has_pose = false;
}
