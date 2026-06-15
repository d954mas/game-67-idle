#include "app/nt_app.h"
#include "core/nt_core.h"
#include "devapi/nt_devapi.h"
#include "game_audio.h"
#include "game_state_actions.h"
#include "game_storage.h"
#include "game_state.h"
#include "generated/roblox_fishing_models.h"
#include "generated/roblox_fishing_ui_assets.gen.h"
#include "generated/rune_marches_assets.gen.h"
#include "drawable_comp/nt_drawable_comp.h"
#include "entity/nt_entity.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "material/nt_material.h"
#include "material_comp/nt_material_comp.h"
#include "math/nt_math.h"
#include "mesh_comp/nt_mesh_comp.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "render/nt_render_items.h"
#include "renderers/nt_mesh_renderer.h"
#include "renderers/nt_shape_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "transform_comp/nt_transform_comp.h"
#include "window/nt_window.h"

#include "cJSON.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#else
#include <glad/gl.h>
#endif

#define DEFAULT_SAVE_KEY "autosave"

#ifndef ROBLOX_FISHING_MODEL_PACK_PATH
#define ROBLOX_FISHING_MODEL_PACK_PATH "assets/roblox_fishing_models.ntpack"
#endif

typedef struct UiBox {
    float x;
    float y;
    float w;
    float h;
} UiBox;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = 9123;
static bool s_fresh_state_requested;
static bool s_autosave_enabled = true;
static int s_window_width = 960;
static int s_window_height = 540;
static UiBox s_cycle_box;
static UiBox s_scout_box;
static UiBox s_strike_box;
static UiBox s_spark_box;
static UiBox s_guard_box;
static UiBox s_retreat_box;
static UiBox s_rest_box;
static UiBox s_upgrade_box;

#define FISHING_MESH_PROP_COUNT 16
#define FISHING_MESH_MODEL_COUNT 6

typedef enum FishingMeshModel {
    FISHING_MESH_MODEL_CUBE,
    FISHING_MESH_MODEL_FISH,
    FISHING_MESH_MODEL_BOAT,
    FISHING_MESH_MODEL_SIGN,
    FISHING_MESH_MODEL_LEAF,
    FISHING_MESH_MODEL_BOBBER,
} FishingMeshModel;

static nt_hash32_t s_fishing_model_pack_id;
static nt_resource_t s_fishing_mesh_models[FISHING_MESH_MODEL_COUNT];
static nt_resource_t s_fishing_mesh_vs;
static nt_resource_t s_fishing_mesh_fs;
static nt_resource_t s_fishing_text_vs;
static nt_resource_t s_fishing_text_fs;
static nt_resource_t s_fishing_font_resource;
static nt_resource_t s_fishing_white_tex;
static nt_material_t s_fishing_mesh_material;
static nt_material_t s_fishing_text_material;
static nt_font_t s_fishing_font;
static nt_entity_t s_fishing_mesh_props[FISHING_MESH_PROP_COUNT];
static FishingMeshModel s_fishing_mesh_prop_models[FISHING_MESH_PROP_COUNT];
static nt_render_item_t s_fishing_mesh_sort_scratch[FISHING_MESH_PROP_COUNT];
static nt_buffer_t s_frame_uniforms_ubo;
static bool s_fishing_mesh_ready;
static uint32_t s_fishing_mesh_last_draw_groups;
static uint32_t s_fishing_mesh_last_instances;
static float s_fishing_ui_height;

typedef enum RuneTelemetryEvent {
    RUNE_TELEMETRY_SESSION_START,
    RUNE_TELEMETRY_FTUE_FIRST_ACTION,
    RUNE_TELEMETRY_COMBAT_FIRST_ACTION,
    RUNE_TELEMETRY_FTUE_FIRST_REWARD,
    RUNE_TELEMETRY_UPGRADE_SPARK_WARD_1,
    RUNE_TELEMETRY_CHOICE_BELL_ROPE,
    RUNE_TELEMETRY_ROUTE_REEDMERE_OPEN,
    RUNE_TELEMETRY_ROUTE_GREENFEN_OPEN,
    RUNE_TELEMETRY_LEVEL_WARDEN_RANK_2,
    RUNE_TELEMETRY_UPGRADE_SPARK_WARD_2,
    RUNE_TELEMETRY_ROUTE_POST_GREENFEN_CHOICE,
    RUNE_TELEMETRY_ROUTE_BRIAR_CLEAR,
    RUNE_TELEMETRY_ROUTE_MOONWELL_CLEAR,
    RUNE_TELEMETRY_ROUTE_ASHEN_CAIRN_OPEN,
    RUNE_TELEMETRY_ROUTE_STARFALL_GROTTO_OPEN,
    RUNE_TELEMETRY_STALL_30S,
    RUNE_TELEMETRY_SESSION_STOP,
    RUNE_TELEMETRY_COUNT
} RuneTelemetryEvent;

typedef struct RuneTelemetryCounter {
    int count;
    uint64_t first_frame;
    uint64_t last_frame;
} RuneTelemetryCounter;

static RuneTelemetryCounter s_rune_telemetry[RUNE_TELEMETRY_COUNT];

static const char *s_rune_telemetry_ids[RUNE_TELEMETRY_COUNT] = {
    "rune_session_start",
    "rune_ftue_first_action",
    "rune_combat_first_action",
    "rune_ftue_first_reward",
    "rune_upgrade_spark_ward_1",
    "rune_choice_bell_rope",
    "rune_route_reedmere_open",
    "rune_route_greenfen_open",
    "rune_level_warden_rank_2",
    "rune_upgrade_spark_ward_2",
    "rune_route_post_greenfen_choice",
    "rune_route_briar_clear",
    "rune_route_moonwell_clear",
    "rune_route_ashen_cairn_open",
    "rune_route_starfall_grotto_open",
    "rune_stall_30s",
    "rune_session_stop",
};

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static char s_pending_capture_path[512];
#endif

static void ortho(float left, float right, float bottom, float top, float near_z, float far_z, float out[16]) {
    memset(out, 0, sizeof(float) * 16);
    out[0] = 2.0F / (right - left);
    out[5] = 2.0F / (top - bottom);
    out[10] = -2.0F / (far_z - near_z);
    out[12] = -(right + left) / (right - left);
    out[13] = -(top + bottom) / (top - bottom);
    out[14] = -(far_z + near_z) / (far_z - near_z);
    out[15] = 1.0F;
}

typedef struct RuneTexVertex {
    float pos[2];
    float uv[2];
    float color[4];
} RuneTexVertex;

static struct RuneTextureRenderer {
    nt_shader_t vs;
    nt_shader_t fs;
    nt_pipeline_t pipeline;
    nt_buffer_t vbo;
    nt_texture_t textures[RUNE_ASSET_COUNT];
    nt_texture_t fishing_textures[FISHING_ASSET_COUNT];
    float vp[16];
    bool initialized;
} s_rune_tex;

static void rune_textures_create_gpu(void) {
    static const char *vs_source =
        "layout(location = 0) in vec2 a_pos;\n"
        "layout(location = 3) in vec2 a_uv;\n"
        "layout(location = 2) in vec4 a_color;\n"
        "uniform mat4 u_vp;\n"
        "out vec2 v_uv;\n"
        "out vec4 v_color;\n"
        "void main() {\n"
        "    v_uv = a_uv;\n"
        "    v_color = a_color;\n"
        "    gl_Position = u_vp * vec4(a_pos, 0.0, 1.0);\n"
        "}\n";
    static const char *fs_source =
        "#ifdef GL_ES\n"
        "precision mediump float;\n"
        "#endif\n"
        "uniform sampler2D u_tex0;\n"
        "in vec2 v_uv;\n"
        "in vec4 v_color;\n"
        "out vec4 frag_color;\n"
        "void main() {\n"
        "    frag_color = texture(u_tex0, v_uv) * v_color;\n"
        "}\n";

    s_rune_tex.vs = nt_gfx_make_shader(&(nt_shader_desc_t){
        .type = NT_SHADER_VERTEX,
        .source = vs_source,
        .label = "rune_tex_vs",
    });
    s_rune_tex.fs = nt_gfx_make_shader(&(nt_shader_desc_t){
        .type = NT_SHADER_FRAGMENT,
        .source = fs_source,
        .label = "rune_tex_fs",
    });
    s_rune_tex.pipeline = nt_gfx_make_pipeline(&(nt_pipeline_desc_t){
        .vertex_shader = s_rune_tex.vs,
        .fragment_shader = s_rune_tex.fs,
        .layout =
            {
                .attr_count = 3,
                .stride = sizeof(RuneTexVertex),
                .attrs =
                    {
                        {.location = NT_ATTR_POSITION, .format = NT_FORMAT_FLOAT2, .offset = offsetof(RuneTexVertex, pos)},
                        {.location = NT_ATTR_TEXCOORD0, .format = NT_FORMAT_FLOAT2, .offset = offsetof(RuneTexVertex, uv)},
                        {.location = NT_ATTR_COLOR, .format = NT_FORMAT_FLOAT4, .offset = offsetof(RuneTexVertex, color)},
                    },
            },
        .depth_test = false,
        .depth_write = false,
        .blend = true,
        .blend_src = NT_BLEND_SRC_ALPHA,
        .blend_dst = NT_BLEND_ONE_MINUS_SRC_ALPHA,
        .label = "rune_tex_pipeline",
    });
    s_rune_tex.vbo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_VERTEX,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(RuneTexVertex) * 6U,
        .label = "rune_tex_quad_vbo",
    });
    for (int i = 0; i < RUNE_ASSET_COUNT; ++i) {
        const RuneAssetImage *asset = &g_rune_assets[i];
        s_rune_tex.textures[i] = nt_gfx_make_texture(&(nt_texture_desc_t){
            .width = asset->width,
            .height = asset->height,
            .data = asset->rgba,
            .format = NT_PIXEL_RGBA8,
            .min_filter = NT_FILTER_LINEAR,
            .mag_filter = NT_FILTER_LINEAR,
            .wrap_u = NT_WRAP_CLAMP_TO_EDGE,
            .wrap_v = NT_WRAP_CLAMP_TO_EDGE,
            .label = asset->id,
        });
    }
    for (int i = 0; i < FISHING_ASSET_COUNT; ++i) {
        const FishingAssetImage *asset = &g_fishing_assets[i];
        s_rune_tex.fishing_textures[i] = nt_gfx_make_texture(&(nt_texture_desc_t){
            .width = asset->width,
            .height = asset->height,
            .data = asset->rgba,
            .format = NT_PIXEL_RGBA8,
            .min_filter = NT_FILTER_LINEAR,
            .mag_filter = NT_FILTER_LINEAR,
            .wrap_u = NT_WRAP_CLAMP_TO_EDGE,
            .wrap_v = NT_WRAP_CLAMP_TO_EDGE,
            .label = asset->id,
        });
    }
}

static void rune_textures_init(void) {
    if (s_rune_tex.initialized) {
        return;
    }
    rune_textures_create_gpu();
    s_rune_tex.initialized = true;
}

static void rune_textures_shutdown(void) {
    if (!s_rune_tex.initialized) {
        return;
    }
    for (int i = 0; i < RUNE_ASSET_COUNT; ++i) {
        if (s_rune_tex.textures[i].id != 0) {
            nt_gfx_destroy_texture(s_rune_tex.textures[i]);
        }
    }
    for (int i = 0; i < FISHING_ASSET_COUNT; ++i) {
        if (s_rune_tex.fishing_textures[i].id != 0) {
            nt_gfx_destroy_texture(s_rune_tex.fishing_textures[i]);
        }
    }
    if (s_rune_tex.vbo.id != 0) {
        nt_gfx_destroy_buffer(s_rune_tex.vbo);
    }
    if (s_rune_tex.pipeline.id != 0) {
        nt_gfx_destroy_pipeline(s_rune_tex.pipeline);
    }
    if (s_rune_tex.fs.id != 0) {
        nt_gfx_destroy_shader(s_rune_tex.fs);
    }
    if (s_rune_tex.vs.id != 0) {
        nt_gfx_destroy_shader(s_rune_tex.vs);
    }
    memset(&s_rune_tex, 0, sizeof(s_rune_tex));
}

static void fishing_mesh_set_prop(int index, FishingMeshModel model, float x, float y, float z, float sx, float sy, float sz, const float color[4]) {
    if (index < 0 || index >= FISHING_MESH_PROP_COUNT) {
        return;
    }
    nt_entity_t entity = s_fishing_mesh_props[index];
    s_fishing_mesh_prop_models[index] = model;
    float *pos = nt_transform_comp_position(entity);
    float *scale = nt_transform_comp_scale(entity);
    pos[0] = x;
    pos[1] = y;
    pos[2] = z;
    scale[0] = sx;
    scale[1] = sy;
    scale[2] = sz;
    *nt_transform_comp_dirty(entity) = true;
    nt_drawable_comp_set_color(entity, color[0], color[1], color[2], color[3]);
}

static void fishing_mesh_system_init(void) {
    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture);
    nt_resource_set_activator(NT_ASSET_MESH, nt_gfx_activate_mesh, nt_gfx_deactivate_mesh);
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);

    nt_font_init(&(nt_font_desc_t){.max_fonts = 2});
    nt_material_desc_t mat_desc = nt_material_desc_defaults();
    nt_material_init(&mat_desc);
    nt_entity_init(&(nt_entity_desc_t){.max_entities = 32});
    nt_transform_comp_init(&(nt_transform_comp_desc_t){.capacity = 32});
    nt_mesh_comp_init(&(nt_mesh_comp_desc_t){.capacity = 32});
    nt_material_comp_init(&(nt_material_comp_desc_t){.capacity = 32});
    nt_drawable_comp_init(&(nt_drawable_comp_desc_t){.capacity = 32});
    nt_mesh_renderer_init(&(nt_mesh_renderer_desc_t){.max_instances = 128, .max_pipelines = 16});
    nt_text_renderer_init();

    s_fishing_model_pack_id = nt_hash32_str("roblox_fishing_models");
    s_fishing_mesh_models[FISHING_MESH_MODEL_CUBE] = nt_resource_request(ASSET_MESH_ROBLOX_FISHING_CUBE_MODEL, NT_ASSET_MESH);
    s_fishing_mesh_models[FISHING_MESH_MODEL_FISH] = nt_resource_request(ASSET_MESH_ROBLOX_FISHING_FISH_TROPHY, NT_ASSET_MESH);
    s_fishing_mesh_models[FISHING_MESH_MODEL_BOAT] = nt_resource_request(ASSET_MESH_ROBLOX_FISHING_TOY_BOAT_HULL, NT_ASSET_MESH);
    s_fishing_mesh_models[FISHING_MESH_MODEL_SIGN] = nt_resource_request(ASSET_MESH_ROBLOX_FISHING_SHOP_SIGN, NT_ASSET_MESH);
    s_fishing_mesh_models[FISHING_MESH_MODEL_LEAF] = nt_resource_request(ASSET_MESH_ROBLOX_FISHING_PALM_LEAF_CHUNK, NT_ASSET_MESH);
    s_fishing_mesh_models[FISHING_MESH_MODEL_BOBBER] = nt_resource_request(ASSET_MESH_ROBLOX_FISHING_BOBBER_DIAMOND, NT_ASSET_MESH);
    s_fishing_mesh_vs = nt_resource_request(ASSET_SHADER_ROBLOX_FISHING_MESH_INST_VERT, NT_ASSET_SHADER_CODE);
    s_fishing_mesh_fs = nt_resource_request(ASSET_SHADER_ROBLOX_FISHING_MESH_INST_FRAG, NT_ASSET_SHADER_CODE);
    s_fishing_text_vs = nt_resource_request(ASSET_SHADER_ROBLOX_FISHING_SLUG_TEXT_VERT, NT_ASSET_SHADER_CODE);
    s_fishing_text_fs = nt_resource_request(ASSET_SHADER_ROBLOX_FISHING_SLUG_TEXT_FRAG, NT_ASSET_SHADER_CODE);
    s_fishing_font_resource = nt_resource_request(ASSET_FONT_ROBLOX_FISHING_FONT, NT_ASSET_FONT);
    s_fishing_white_tex = nt_resource_request(ASSET_TEXTURE_ROBLOX_FISHING_WHITE, NT_ASSET_TEXTURE);

    s_fishing_mesh_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_fishing_mesh_vs,
        .fs = s_fishing_mesh_fs,
        .textures = {{.name = "u_texture", .resource = s_fishing_white_tex}},
        .texture_count = 1,
        .attr_map = {{.stream_name = "position", .location = 0}, {.stream_name = "uv0", .location = 1}},
        .attr_map_count = 2,
        .depth_test = true,
        .depth_write = true,
        .cull_mode = NT_CULL_NONE,
        .color_mode = NT_COLOR_MODE_FLOAT4,
        .label = "splash_rods_glb_props",
    });
    s_fishing_text_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_fishing_text_vs,
        .fs = s_fishing_text_fs,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .params[0] = {.name = "u_alpha_cutoff", .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
        .param_count = 1,
        .label = "splash_rods_ttf_text",
    });
    s_fishing_font = nt_font_create(&(nt_font_create_desc_t){
        .curve_texture_width = 1024,
        .curve_texture_height = 512,
        .band_texture_height = 256,
        .band_count = 8,
        .measure_cache_size = 512,
    });
    nt_font_add(s_fishing_font, s_fishing_font_resource);

    for (int i = 0; i < FISHING_MESH_PROP_COUNT; ++i) {
        s_fishing_mesh_props[i] = nt_entity_create();
        nt_transform_comp_add(s_fishing_mesh_props[i]);
        nt_mesh_comp_add(s_fishing_mesh_props[i]);
        nt_material_comp_add(s_fishing_mesh_props[i]);
        nt_drawable_comp_add(s_fishing_mesh_props[i]);
        *nt_material_comp_handle(s_fishing_mesh_props[i]) = s_fishing_mesh_material;
    }

    fishing_mesh_set_prop(0, FISHING_MESH_MODEL_SIGN, -4.95F, 1.04F, -1.18F, 0.40F, 0.40F, 0.40F, (float[4]){1.0F, 0.76F, 0.14F, 1.0F});
    fishing_mesh_set_prop(1, FISHING_MESH_MODEL_BOAT, 5.38F, 0.24F, 0.36F, 0.42F, 0.42F, 0.42F, (float[4]){1.0F, 0.24F, 0.18F, 1.0F});
    fishing_mesh_set_prop(2, FISHING_MESH_MODEL_FISH, 1.35F, 0.72F, -0.28F, 0.48F, 0.48F, 0.48F, (float[4]){1.0F, 0.14F, 0.90F, 1.0F});
    fishing_mesh_set_prop(3, FISHING_MESH_MODEL_FISH, 2.90F, 0.42F, -1.86F, 0.36F, 0.36F, 0.36F, (float[4]){0.22F, 0.72F, 1.0F, 1.0F});
    fishing_mesh_set_prop(4, FISHING_MESH_MODEL_FISH, -0.14F, 0.42F, -1.34F, 0.34F, 0.34F, 0.34F, (float[4]){1.0F, 0.72F, 0.10F, 1.0F});
    fishing_mesh_set_prop(5, FISHING_MESH_MODEL_BOBBER, 1.22F, 0.70F, -0.22F, 0.46F, 0.46F, 0.46F, (float[4]){1.0F, 0.12F, 0.30F, 1.0F});
    fishing_mesh_set_prop(6, FISHING_MESH_MODEL_LEAF, -5.98F, 1.42F, -1.12F, 0.32F, 0.32F, 0.32F, (float[4]){0.16F, 0.88F, 0.24F, 1.0F});
    fishing_mesh_set_prop(7, FISHING_MESH_MODEL_LEAF, -5.60F, 1.40F, -1.28F, 0.30F, 0.30F, 0.30F, (float[4]){0.16F, 0.80F, 0.22F, 1.0F});
    fishing_mesh_set_prop(8, FISHING_MESH_MODEL_LEAF, 4.58F, 1.18F, -3.44F, 0.28F, 0.28F, 0.28F, (float[4]){0.16F, 0.86F, 0.24F, 1.0F});
    fishing_mesh_set_prop(9, FISHING_MESH_MODEL_CUBE, -2.85F, 0.62F, 1.02F, 0.30F, 0.70F, 0.30F, (float[4]){0.94F, 0.54F, 0.18F, 1.0F});
    fishing_mesh_set_prop(10, FISHING_MESH_MODEL_CUBE, 0.40F, 0.62F, 3.45F, 0.34F, 0.62F, 0.34F, (float[4]){0.94F, 0.54F, 0.18F, 1.0F});
    fishing_mesh_set_prop(11, FISHING_MESH_MODEL_CUBE, 3.92F, 0.72F, -2.20F, 0.36F, 0.36F, 0.36F, (float[4]){0.72F, 0.30F, 1.0F, 1.0F});
    fishing_mesh_set_prop(12, FISHING_MESH_MODEL_BOBBER, 2.25F, 0.60F, -0.55F, 0.28F, 0.28F, 0.28F, (float[4]){1.0F, 0.78F, 0.12F, 0.85F});
    fishing_mesh_set_prop(13, FISHING_MESH_MODEL_BOBBER, 2.55F, 0.78F, -0.40F, 0.24F, 0.24F, 0.24F, (float[4]){1.0F, 0.78F, 0.12F, 0.85F});
    fishing_mesh_set_prop(14, FISHING_MESH_MODEL_CUBE, -4.72F, 0.66F, -1.05F, 0.30F, 0.30F, 0.30F, (float[4]){0.20F, 0.88F, 1.0F, 1.0F});
    fishing_mesh_set_prop(15, FISHING_MESH_MODEL_CUBE, -5.30F, 0.66F, -1.04F, 0.28F, 0.28F, 0.28F, (float[4]){1.0F, 0.46F, 0.72F, 1.0F});

    s_frame_uniforms_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "splash_rods_frame_uniforms",
    });

    nt_resource_mount(s_fishing_model_pack_id, 100);
    nt_resource_load_auto(s_fishing_model_pack_id, ROBLOX_FISHING_MODEL_PACK_PATH);
    nt_resource_set_activate_time_budget(0);
    s_fishing_mesh_ready = true;
}

static void fishing_mesh_restore_gpu(void) {
    if (!s_fishing_mesh_ready) {
        return;
    }
    nt_resource_invalidate(NT_ASSET_SHADER_CODE);
    nt_resource_invalidate(NT_ASSET_MESH);
    nt_resource_invalidate(NT_ASSET_TEXTURE);
    nt_resource_invalidate(NT_ASSET_FONT);
    if (s_frame_uniforms_ubo.id != 0) {
        nt_gfx_destroy_buffer(s_frame_uniforms_ubo);
    }
    s_frame_uniforms_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "splash_rods_frame_uniforms",
    });
    nt_mesh_renderer_restore_gpu();
    nt_text_renderer_restore_gpu();
}

static void fishing_mesh_system_step(void) {
    if (!s_fishing_mesh_ready) {
        return;
    }
    nt_resource_step();
    nt_material_step();
    nt_font_step();
}

static void fishing_mesh_system_shutdown(void) {
    if (!s_fishing_mesh_ready) {
        return;
    }
    nt_text_renderer_shutdown();
    nt_mesh_renderer_shutdown();
    nt_font_destroy(s_fishing_font);
    nt_font_shutdown();
    nt_drawable_comp_shutdown();
    nt_material_comp_shutdown();
    nt_mesh_comp_shutdown();
    nt_transform_comp_shutdown();
    nt_entity_shutdown();
    nt_material_destroy(s_fishing_text_material);
    nt_material_destroy(s_fishing_mesh_material);
    nt_material_shutdown();
    nt_resource_shutdown();
    nt_fs_shutdown();
    nt_http_shutdown();
    nt_hash_shutdown();
    if (s_frame_uniforms_ubo.id != 0) {
        nt_gfx_destroy_buffer(s_frame_uniforms_ubo);
    }
    memset(s_fishing_mesh_props, 0, sizeof(s_fishing_mesh_props));
    s_fishing_mesh_ready = false;
}

static void draw_fishing_mesh_props(const float view[16], const float proj[16], const float view_proj[16], const float cam_pos[3]) {
    if (!s_fishing_mesh_ready) {
        s_fishing_mesh_last_draw_groups = 0;
        s_fishing_mesh_last_instances = 0;
        return;
    }
    const nt_material_info_t *mat_info = nt_material_get_info(s_fishing_mesh_material);
    if (!mat_info || !mat_info->ready) {
        s_fishing_mesh_last_draw_groups = 0;
        s_fishing_mesh_last_instances = 0;
        return;
    }
    for (int i = 0; i < FISHING_MESH_MODEL_COUNT; ++i) {
        if (!nt_resource_is_ready(s_fishing_mesh_models[i])) {
            s_fishing_mesh_last_draw_groups = 0;
            s_fishing_mesh_last_instances = 0;
            return;
        }
    }

    nt_frame_uniforms_t uniforms = {0};
    memcpy(uniforms.view_proj, view_proj, 64);
    memcpy(uniforms.view, view, 64);
    memcpy(uniforms.proj, proj, 64);
    uniforms.camera_pos[0] = cam_pos[0];
    uniforms.camera_pos[1] = cam_pos[1];
    uniforms.camera_pos[2] = cam_pos[2];
    uniforms.time[0] = (float)g_nt_app.time;
    uniforms.time[1] = g_nt_app.dt;
    uniforms.resolution[0] = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    uniforms.resolution[1] = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    if (uniforms.resolution[0] > 0.0F && uniforms.resolution[1] > 0.0F) {
        uniforms.resolution[2] = 1.0F / uniforms.resolution[0];
        uniforms.resolution[3] = 1.0F / uniforms.resolution[1];
    }
    uniforms.near_far[0] = 0.1F;
    uniforms.near_far[1] = 80.0F;
    nt_gfx_update_buffer(s_frame_uniforms_ubo, &uniforms, sizeof(uniforms));
    nt_gfx_bind_uniform_buffer(s_frame_uniforms_ubo, 0);

    nt_transform_comp_update();
    nt_render_item_t items[FISHING_MESH_PROP_COUNT];
    for (int i = 0; i < FISHING_MESH_PROP_COUNT; ++i) {
        const uint32_t mesh_id = nt_resource_get(s_fishing_mesh_models[s_fishing_mesh_prop_models[i]]);
        *nt_mesh_comp_handle(s_fishing_mesh_props[i]) = (nt_mesh_t){.id = mesh_id};
        items[i].sort_key = nt_sort_key_opaque(s_fishing_mesh_material.id, mesh_id);
        items[i].entity = s_fishing_mesh_props[i].id;
        items[i].batch_key = nt_batch_key(s_fishing_mesh_material.id, mesh_id);
    }
    nt_sort_by_key(items, FISHING_MESH_PROP_COUNT, s_fishing_mesh_sort_scratch);
    s_fishing_mesh_last_instances = FISHING_MESH_PROP_COUNT;
    s_fishing_mesh_last_draw_groups = 0;
    for (int i = 0; i < FISHING_MESH_PROP_COUNT; ++i) {
        if (i == 0 || s_fishing_mesh_sort_scratch[i].batch_key != s_fishing_mesh_sort_scratch[i - 1].batch_key) {
            s_fishing_mesh_last_draw_groups++;
        }
    }
    nt_mesh_renderer_draw_list(s_fishing_mesh_sort_scratch, FISHING_MESH_PROP_COUNT);
}

static void rune_textures_restore_gpu(void) {
    memset(&s_rune_tex.vs, 0, sizeof(s_rune_tex.vs));
    memset(&s_rune_tex.fs, 0, sizeof(s_rune_tex.fs));
    memset(&s_rune_tex.pipeline, 0, sizeof(s_rune_tex.pipeline));
    memset(&s_rune_tex.vbo, 0, sizeof(s_rune_tex.vbo));
    memset(s_rune_tex.textures, 0, sizeof(s_rune_tex.textures));
    memset(s_rune_tex.fishing_textures, 0, sizeof(s_rune_tex.fishing_textures));
    rune_textures_create_gpu();
}

static void rune_textures_set_vp(const float vp[16]) {
    memcpy(s_rune_tex.vp, vp, sizeof(s_rune_tex.vp));
}

static void rune_asset_region(RuneAssetId id, float x, float y, float w, float h, float u0, float v0, float u1, float v1, const float color[4]) {
    if (!s_rune_tex.initialized || id < 0 || id >= RUNE_ASSET_COUNT || s_rune_tex.pipeline.id == 0 || s_rune_tex.vbo.id == 0 || s_rune_tex.textures[id].id == 0) {
        return;
    }
    const float r = color ? color[0] : 1.0F;
    const float g = color ? color[1] : 1.0F;
    const float b = color ? color[2] : 1.0F;
    const float a = color ? color[3] : 1.0F;
    const RuneTexVertex vertices[6] = {
        {{x, y}, {u0, v0}, {r, g, b, a}},
        {{x + w, y}, {u1, v0}, {r, g, b, a}},
        {{x + w, y + h}, {u1, v1}, {r, g, b, a}},
        {{x + w, y + h}, {u1, v1}, {r, g, b, a}},
        {{x, y + h}, {u0, v1}, {r, g, b, a}},
        {{x, y}, {u0, v0}, {r, g, b, a}},
    };
    nt_gfx_orphan_buffer(s_rune_tex.vbo, vertices, sizeof(vertices));
    nt_gfx_bind_pipeline(s_rune_tex.pipeline);
    nt_gfx_bind_vertex_buffer(s_rune_tex.vbo);
    nt_gfx_bind_texture(s_rune_tex.textures[id], 0);
    nt_gfx_set_uniform_mat4("u_vp", s_rune_tex.vp);
    nt_gfx_set_uniform_int("u_tex0", 0);
    nt_gfx_draw(0, 6);
}

static void rune_asset(RuneAssetId id, float x, float y, float w, float h, const float color[4]) {
    rune_asset_region(id, x, y, w, h, 0.0F, 0.0F, 1.0F, 1.0F, color);
}

static void fishing_asset_region(FishingAssetId id, float x, float y, float w, float h, float u0, float v0, float u1, float v1, const float color[4]) {
    if (!s_rune_tex.initialized || id < 0 || id >= FISHING_ASSET_COUNT || s_rune_tex.pipeline.id == 0 || s_rune_tex.vbo.id == 0 || s_rune_tex.fishing_textures[id].id == 0) {
        return;
    }
    const float r = color ? color[0] : 1.0F;
    const float g = color ? color[1] : 1.0F;
    const float b = color ? color[2] : 1.0F;
    const float a = color ? color[3] : 1.0F;
    const RuneTexVertex vertices[6] = {
        {{x, y}, {u0, v0}, {r, g, b, a}},
        {{x + w, y}, {u1, v0}, {r, g, b, a}},
        {{x + w, y + h}, {u1, v1}, {r, g, b, a}},
        {{x + w, y + h}, {u1, v1}, {r, g, b, a}},
        {{x, y + h}, {u0, v1}, {r, g, b, a}},
        {{x, y}, {u0, v0}, {r, g, b, a}},
    };
    nt_gfx_orphan_buffer(s_rune_tex.vbo, vertices, sizeof(vertices));
    nt_gfx_bind_pipeline(s_rune_tex.pipeline);
    nt_gfx_bind_vertex_buffer(s_rune_tex.vbo);
    nt_gfx_bind_texture(s_rune_tex.fishing_textures[id], 0);
    nt_gfx_set_uniform_mat4("u_vp", s_rune_tex.vp);
    nt_gfx_set_uniform_int("u_tex0", 0);
    nt_gfx_draw(0, 6);
}

static void fishing_asset(FishingAssetId id, float x, float y, float w, float h, const float color[4]) {
    fishing_asset_region(id, x, y, w, h, 0.0F, 0.0F, 1.0F, 1.0F, color);
}

static void rune_asset_slice9(RuneAssetId id, float x, float y, float w, float h, float left, float top, float right, float bottom, const float color[4]) {
    if (id < 0 || id >= RUNE_ASSET_COUNT || w <= 0.0F || h <= 0.0F) {
        return;
    }
    const RuneAssetImage *asset = &g_rune_assets[id];
    if (asset->width == 0 || asset->height == 0) {
        return;
    }
    if (left + right > w && left + right > 0.0F) {
        const float scale = w / (left + right);
        left *= scale;
        right *= scale;
    }
    if (top + bottom > h && top + bottom > 0.0F) {
        const float scale = h / (top + bottom);
        top *= scale;
        bottom *= scale;
    }
    const float src_w = (float)asset->width;
    const float src_h = (float)asset->height;
    const float dx[4] = {x, x + left, x + w - right, x + w};
    const float dy[4] = {y, y + top, y + h - bottom, y + h};
    const float du[4] = {0.0F, left / src_w, (src_w - right) / src_w, 1.0F};
    const float dv[4] = {0.0F, top / src_h, (src_h - bottom) / src_h, 1.0F};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const float rw = dx[col + 1] - dx[col];
            const float rh = dy[row + 1] - dy[row];
            if (rw > 0.0F && rh > 0.0F) {
                rune_asset_region(id, dx[col], dy[row], rw, rh, du[col], dv[row], du[col + 1], dv[row + 1], color);
            }
        }
    }
}

static void fishing_asset_slice9(FishingAssetId id, float x, float y, float w, float h, float left, float top, float right, float bottom, const float color[4]) {
    if (id < 0 || id >= FISHING_ASSET_COUNT || w <= 0.0F || h <= 0.0F) {
        return;
    }
    const FishingAssetImage *asset = &g_fishing_assets[id];
    if (asset->width == 0 || asset->height == 0) {
        return;
    }
    if (left + right > w && left + right > 0.0F) {
        const float scale = w / (left + right);
        left *= scale;
        right *= scale;
    }
    if (top + bottom > h && top + bottom > 0.0F) {
        const float scale = h / (top + bottom);
        top *= scale;
        bottom *= scale;
    }
    const float src_w = (float)asset->width;
    const float src_h = (float)asset->height;
    const float dx[4] = {x, x + left, x + w - right, x + w};
    const float dy[4] = {y, y + top, y + h - bottom, y + h};
    const float du[4] = {0.0F, left / src_w, (src_w - right) / src_w, 1.0F};
    const float dv[4] = {0.0F, top / src_h, (src_h - bottom) / src_h, 1.0F};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const float rw = dx[col + 1] - dx[col];
            const float rh = dy[row + 1] - dy[row];
            if (rw > 0.0F && rh > 0.0F) {
                fishing_asset_region(id, dx[col], dy[row], rw, rh, du[col], dv[row], du[col + 1], dv[row + 1], color);
            }
        }
    }
}

static void rect(float x, float y, float w, float h, const float color[4]) {
    const float pos[3] = {x + w * 0.5F, y + h * 0.5F, 0.0F};
    const float size[2] = {w, h};
    nt_shape_renderer_rect(pos, size, color);
}

static void rect_wire(float x, float y, float w, float h, const float color[4]) {
    const float pos[3] = {x + w * 0.5F, y + h * 0.5F, 0.0F};
    const float size[2] = {w, h};
    nt_shape_renderer_rect_wire(pos, size, color);
}

static void circle(float x, float y, float radius, const float color[4]) {
    const float center[3] = {x, y, 0.0F};
    nt_shape_renderer_circle(center, radius, color);
}

static void ui_pill(float x, float y, float w, float h, const float fill[4], const float shine[4], const float shadow[4]) {
    const float r = h * 0.5F;
    rect(x + r, y + 5.0F, w - r * 2.0F, h - 4.0F, shadow);
    circle(x + r, y + h * 0.5F + 5.0F, r, shadow);
    circle(x + w - r, y + h * 0.5F + 5.0F, r, shadow);
    rect(x + r, y, w - r * 2.0F, h, fill);
    circle(x + r, y + h * 0.5F, r, fill);
    circle(x + w - r, y + h * 0.5F, r, fill);
    rect(x + r, y + 5.0F, w - r * 2.0F, h * 0.12F, shine);
}

static const uint8_t *glyph_rows(char ch) {
    static const uint8_t unknown[7] = {0x1F, 0x11, 0x04, 0x04, 0x04, 0x00, 0x04};
    static const uint8_t space[7] = {0};
    static const uint8_t colon[7] = {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
    static const uint8_t slash[7] = {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10};
    static const uint8_t plus[7] = {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
    static const uint8_t dash[7] = {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
    static const uint8_t dot[7] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
    static const uint8_t digits[10][7] = {
        {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
        {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
        {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
        {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
        {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
        {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
        {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
        {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    };
    static const uint8_t letters[26][7] = {
        {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
        {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
        {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
        {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E},
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
        {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
        {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},
        {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
        {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
        {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E},
        {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
        {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
        {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
        {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
        {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
        {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
        {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
        {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
        {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
        {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
        {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
        {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},
        {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
        {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04},
        {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F},
    };
    if (ch >= 'a' && ch <= 'z') {
        ch = (char)(ch - 'a' + 'A');
    }
    if (ch >= 'A' && ch <= 'Z') {
        return letters[ch - 'A'];
    }
    if (ch >= '0' && ch <= '9') {
        return digits[ch - '0'];
    }
    switch (ch) {
    case ' ':
        return space;
    case ':':
        return colon;
    case '/':
        return slash;
    case '+':
        return plus;
    case '-':
        return dash;
    case '.':
        return dot;
    default:
        return unknown;
    }
}

static float text_width(const char *text, float scale) {
    const size_t len = text ? strlen(text) : 0U;
    return len > 0U ? (float)len * 6.0F * scale - scale : 0.0F;
}

static void draw_text(const char *text, float x, float y, float scale, const float color[4]) {
    if (!text || scale <= 0.0F) {
        return;
    }
    float cursor = x;
    for (const char *p = text; *p; ++p) {
        const uint8_t *rows = glyph_rows(*p);
        for (int row = 0; row < 7; ++row) {
            for (int col = 0; col < 5; ++col) {
                if ((rows[row] & (uint8_t)(1U << (4 - col))) != 0U) {
                    rect(cursor + (float)col * scale, y + (float)row * scale, scale, scale, color);
                }
            }
        }
        cursor += 6.0F * scale;
    }
}

static void draw_text_centered(const char *text, float cx, float y, float scale, const float color[4]) {
    draw_text(text, cx - text_width(text, scale) * 0.5F, y, scale, color);
}

static void draw_text_shadow(const char *text, float x, float y, float scale, const float color[4]) {
    draw_text(text, x + scale, y + scale, scale, (float[4]){0.02F, 0.08F, 0.12F, 0.72F});
    draw_text(text, x, y, scale, color);
}

static void draw_text_centered_shadow(const char *text, float cx, float y, float scale, const float color[4]) {
    draw_text_shadow(text, cx - text_width(text, scale) * 0.5F, y, scale, color);
}

static void draw_text_fit(const char *text, UiBox box, float preferred_scale, const float color[4]) {
    float scale = preferred_scale;
    const float width = text_width(text, scale);
    if (width > box.w - 16.0F && width > 0.0F) {
        scale *= (box.w - 16.0F) / width;
    }
    if (scale < 0.72F) {
        scale = 0.72F;
    }
    draw_text_centered(text, box.x + box.w * 0.5F, box.y + box.h * 0.5F - 3.5F * scale, scale, color);
}

static void draw_text_fit_shadow(const char *text, UiBox box, float preferred_scale, const float color[4]) {
    float scale = preferred_scale;
    const float width = text_width(text, scale);
    if (width > box.w - 18.0F && width > 0.0F) {
        scale *= (box.w - 18.0F) / width;
    }
    if (scale < 1.12F) {
        scale = 1.12F;
    }
    draw_text_centered_shadow(text, box.x + box.w * 0.5F, box.y + box.h * 0.5F - 3.5F * scale, scale, color);
}

static bool fishing_ttf_ready(void) {
    const nt_material_info_t *mat_info = nt_material_get_info(s_fishing_text_material);
    return s_fishing_mesh_ready && mat_info && mat_info->ready && nt_resource_is_ready(s_fishing_font_resource) && nt_font_valid(s_fishing_font);
}

static void fishing_ttf_prepare(const float view_proj[16]) {
    if (!fishing_ttf_ready()) {
        return;
    }
    nt_frame_uniforms_t uniforms = {0};
    memcpy(uniforms.view_proj, view_proj, 64);
    memcpy(uniforms.view, view_proj, 64);
    memcpy(uniforms.proj, view_proj, 64);
    uniforms.resolution[0] = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    uniforms.resolution[1] = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    if (uniforms.resolution[0] > 0.0F && uniforms.resolution[1] > 0.0F) {
        uniforms.resolution[2] = 1.0F / uniforms.resolution[0];
        uniforms.resolution[3] = 1.0F / uniforms.resolution[1];
    }
    nt_gfx_update_buffer(s_frame_uniforms_ubo, &uniforms, sizeof(uniforms));
    nt_gfx_bind_uniform_buffer(s_frame_uniforms_ubo, 0);
    nt_text_renderer_set_material(s_fishing_text_material);
    nt_text_renderer_set_font(s_fishing_font);
}

static void draw_ttf_text_raw(const char *text, float x, float y, float size, const float color[4]) {
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){x, s_fishing_ui_height - y - size, 0.0F});
    nt_text_renderer_draw(text, (const float *)model, size, color, 0.0F, 0.0F);
}

static void draw_ui_text(const char *text, float x, float y, float size, const float color[4]) {
    if (fishing_ttf_ready()) {
        nt_shape_renderer_flush();
        draw_ttf_text_raw(text, x + 1.5F, y + 2.0F, size, (float[4]){0.02F, 0.07F, 0.10F, 0.72F});
        draw_ttf_text_raw(text, x, y, size, color);
        nt_text_renderer_flush();
    } else {
        draw_text_shadow(text, x, y, size / 10.0F, color);
    }
}

static void draw_ui_text_fit(const char *text, UiBox box, float preferred_size, const float color[4]) {
    if (fishing_ttf_ready()) {
        float size = preferred_size;
        nt_text_size_t measured = nt_font_measure(s_fishing_font, text, size, 0.0F);
        const float max_w = box.w - 18.0F;
        if (measured.width > max_w && measured.width > 0.0F) {
            size *= max_w / measured.width;
            measured = nt_font_measure(s_fishing_font, text, size, 0.0F);
        }
        if (size < 14.0F) {
            size = 14.0F;
            measured = nt_font_measure(s_fishing_font, text, size, 0.0F);
        }
        draw_ui_text(text, box.x + box.w * 0.5F - measured.width * 0.5F, box.y + box.h * 0.5F - measured.height * 0.5F, size, color);
    } else {
        draw_text_fit_shadow(text, box, preferred_size / 10.0F, color);
    }
}

static bool contains(UiBox box, float x, float y) {
    return x >= box.x && x <= box.x + box.w && y >= box.y && y <= box.y + box.h;
}

static void layout(float w, float h) {
    const bool compact = h > w || w < 560.0F;
    const float primary_w = compact ? w - 20.0F : 320.0F;
    const float primary_h = compact ? 52.0F : 86.0F;
    const float secondary_h = compact ? 52.0F : (h < 420.0F ? 56.0F : 64.0F);
    const float pad = compact ? 10.0F : 14.0F;
    const float row_y = h - secondary_h - pad;
    const float small_w = compact ? (w - pad * 3.0F) * 0.5F : (w - primary_w - pad * 6.0F) / 4.0F;
    const float primary_y = compact ? row_y - primary_h - pad * 0.8F : h - primary_h - pad;
    s_cycle_box = (UiBox){
        compact ? pad : w * 0.5F - primary_w * 0.5F,
        primary_y,
        primary_w,
        primary_h,
    };
    s_scout_box = s_cycle_box;
    if (compact) {
        s_strike_box = (UiBox){pad, primary_y, small_w, secondary_h};
        s_spark_box = (UiBox){pad * 2.0F + small_w, primary_y, small_w, secondary_h};
        s_guard_box = (UiBox){pad, row_y, small_w, secondary_h};
        s_retreat_box = (UiBox){pad * 2.0F + small_w, row_y, small_w, secondary_h};
        s_rest_box = (UiBox){pad, row_y, small_w, secondary_h};
        s_upgrade_box = (UiBox){pad * 2.0F + small_w, row_y, small_w, secondary_h};
    } else {
        const float primary_x = s_scout_box.x;
        s_rest_box = (UiBox){pad, row_y, small_w, secondary_h};
        s_strike_box = (UiBox){pad * 2.0F + small_w, row_y, small_w, secondary_h};
        s_spark_box = (UiBox){primary_x + primary_w + pad, row_y, small_w, secondary_h};
        s_upgrade_box = (UiBox){primary_x + primary_w + pad * 2.0F + small_w, row_y, small_w, secondary_h};
        s_guard_box = s_spark_box;
        s_retreat_box = s_upgrade_box;
    }
}

static void rune_telemetry_reset(void) {
    memset(s_rune_telemetry, 0, sizeof(s_rune_telemetry));
}

static void rune_telemetry_record(RuneTelemetryEvent event) {
    if (event < 0 || event >= RUNE_TELEMETRY_COUNT) {
        return;
    }
    RuneTelemetryCounter *counter = &s_rune_telemetry[event];
    if (counter->count == 0) {
        counter->first_frame = (uint64_t)g_nt_app.frame;
    }
    counter->count += 1;
    counter->last_frame = (uint64_t)g_nt_app.frame;
}

static void rune_telemetry_record_once(RuneTelemetryEvent event) {
    if (event < 0 || event >= RUNE_TELEMETRY_COUNT || s_rune_telemetry[event].count > 0) {
        return;
    }
    rune_telemetry_record(event);
}

static bool rune_telemetry_in_combat(const GameState *state) {
    return state->rune_encounter != GAME_STATE_RUNE_ENCOUNTER_NONE;
}

static void rune_telemetry_after_action(const GameState *before, const GameState *after) {
    if (before->rune_main_quest_step < 1 && after->rune_main_quest_step >= 1) {
        rune_telemetry_record_once(RUNE_TELEMETRY_FTUE_FIRST_ACTION);
    }
    if (rune_telemetry_in_combat(before) && (after->rune_enemy_hp < before->rune_enemy_hp || after->rune_encounter == GAME_STATE_RUNE_ENCOUNTER_NONE)) {
        rune_telemetry_record_once(RUNE_TELEMETRY_COMBAT_FIRST_ACTION);
    }
    if ((before->rune_silver < 6 || before->rune_xp < 4 || before->rune_sparks < 1) && after->rune_silver >= 6 && after->rune_xp >= 4 && after->rune_sparks >= 1) {
        rune_telemetry_record_once(RUNE_TELEMETRY_FTUE_FIRST_REWARD);
    }
    if (before->rune_spell_level < 1 && after->rune_spell_level >= 1 && after->rune_tower_unlocked) {
        rune_telemetry_record_once(RUNE_TELEMETRY_UPGRADE_SPARK_WARD_1);
    }
    if (before->rune_side_quest_step != 3 && after->rune_side_quest_step == 3) {
        rune_telemetry_record_once(RUNE_TELEMETRY_CHOICE_BELL_ROPE);
    }
    if (!before->rune_east_road_unlocked && after->rune_east_road_unlocked) {
        rune_telemetry_record_once(RUNE_TELEMETRY_ROUTE_REEDMERE_OPEN);
    }
    if (!before->rune_causeway_unlocked && after->rune_causeway_unlocked) {
        rune_telemetry_record_once(RUNE_TELEMETRY_ROUTE_GREENFEN_OPEN);
    }
    if (before->rune_player_level < 2 && after->rune_player_level >= 2) {
        rune_telemetry_record_once(RUNE_TELEMETRY_LEVEL_WARDEN_RANK_2);
    }
    if (before->rune_spell_level < 2 && after->rune_spell_level >= 2) {
        rune_telemetry_record_once(RUNE_TELEMETRY_UPGRADE_SPARK_WARD_2);
    }
    if (before->rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_NONE && after->rune_route_choice != GAME_STATE_RUNE_ROUTE_CHOICE_NONE) {
        rune_telemetry_record_once(RUNE_TELEMETRY_ROUTE_POST_GREENFEN_CHOICE);
    }
    if (before->rune_briar_gate_safety < 1 && after->rune_briar_gate_safety >= 1) {
        rune_telemetry_record_once(RUNE_TELEMETRY_ROUTE_BRIAR_CLEAR);
    }
    if (before->rune_moonwell_safety < 1 && after->rune_moonwell_safety >= 1) {
        rune_telemetry_record_once(RUNE_TELEMETRY_ROUTE_MOONWELL_CLEAR);
    }
    if (!before->rune_ashen_cairn_unlocked && after->rune_ashen_cairn_unlocked) {
        rune_telemetry_record_once(RUNE_TELEMETRY_ROUTE_ASHEN_CAIRN_OPEN);
    }
    if (!before->rune_starfall_grotto_unlocked && after->rune_starfall_grotto_unlocked) {
        rune_telemetry_record_once(RUNE_TELEMETRY_ROUTE_STARFALL_GROTTO_OPEN);
    }
}

static void action_click(void (*fn)(GameState *)) {
    const GameState before = g_game_state;
    fn(&g_game_state);
    rune_telemetry_after_action(&before, &g_game_state);
    game_audio_play(GAME_AUDIO_CUE_CLICK);
    game_audio_set_volume(g_game_state.settings_master_volume, g_game_state.settings_sfx_volume);
}

static bool click_box(UiBox box, void (*fn)(GameState *)) {
    for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
        const nt_pointer_t pointer = g_nt_input.pointers[i];
        if (pointer.active && pointer.buttons[NT_BUTTON_LEFT].is_pressed && contains(box, pointer.x, pointer.y)) {
            action_click(fn);
            return true;
        }
    }
    return false;
}

static void handle_input(void) {
    if (nt_input_key_is_pressed(NT_KEY_SPACE) || nt_input_key_is_pressed(NT_KEY_ENTER)) {
        action_click(game_fishing_primary_action);
    }
    if (nt_input_key_is_pressed(NT_KEY_1)) {
        action_click(game_fishing_cast);
    }
    if (nt_input_key_is_pressed(NT_KEY_2)) {
        action_click(game_fishing_reel);
    }
    if (nt_input_key_is_pressed(NT_KEY_3)) {
        action_click(game_fishing_sell_all);
    }
    if (nt_input_key_is_pressed(NT_KEY_R) || nt_input_key_is_pressed(NT_KEY_S)) {
        action_click(game_fishing_sell_all);
    }
    if (nt_input_key_is_pressed(NT_KEY_U)) {
        action_click(game_fishing_buy_better_line);
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        if (click_box(s_scout_box, game_fishing_primary_action) ||
            click_box(s_strike_box, game_fishing_cast) ||
            click_box(s_spark_box, game_fishing_reel) ||
            click_box(s_rest_box, game_fishing_sell_all) ||
            click_box(s_upgrade_box, game_fishing_buy_better_line)) {
            return;
        }
    }
}

static void draw_seed_shape(float w, float h) {
    const float cx = w * 0.5F;
    const float cy = h * 0.40F;
    const float size = h < w ? h * 0.18F : w * 0.18F;
    const float colors[][4] = {
        {0.25F, 0.52F, 0.92F, 1.0F},
        {0.10F, 0.72F, 0.48F, 1.0F},
        {0.96F, 0.55F, 0.16F, 1.0F},
        {0.82F, 0.28F, 0.50F, 1.0F},
    };
    const float *accent = colors[g_game_state.shape_index % 4];
    const float shadow[4] = {0.06F, 0.08F, 0.11F, 0.28F};
    const float line[4] = {0.95F, 0.97F, 1.0F, 0.85F};

    circle(cx + size * 0.08F, cy + size * 0.12F, size * 0.86F, shadow);
    switch (g_game_state.shape_index) {
    case GAME_STATE_SHAPE_SPHERE:
        circle(cx, cy, size * 0.70F, accent);
        circle(cx - size * 0.20F, cy - size * 0.22F, size * 0.18F, line);
        break;
    case GAME_STATE_SHAPE_CYLINDER:
        rect(cx - size * 0.62F, cy - size * 0.48F, size * 1.24F, size * 0.96F, accent);
        circle(cx, cy - size * 0.48F, size * 0.62F, accent);
        circle(cx, cy + size * 0.48F, size * 0.62F, line);
        break;
    case GAME_STATE_SHAPE_CAPSULE:
        rect(cx - size * 0.46F, cy - size * 0.72F, size * 0.92F, size * 1.44F, accent);
        circle(cx, cy - size * 0.72F, size * 0.46F, accent);
        circle(cx, cy + size * 0.72F, size * 0.46F, line);
        break;
    case GAME_STATE_SHAPE_CUBE:
    default:
        rect(cx - size * 0.62F, cy - size * 0.62F, size * 1.24F, size * 1.24F, accent);
        rect_wire(cx - size * 0.50F, cy - size * 0.50F, size, size, line);
        break;
    }
}

static void bar(float x, float y, float w, float h, int value, int max_value, const float color[4]) {
    const float bg[4] = {0.05F, 0.06F, 0.08F, 1.0F};
    const float fill = max_value > 0 ? (float)value / (float)max_value : 0.0F;
    rect(x, y, w, h, bg);
    rect(x, y, w * (fill < 0.0F ? 0.0F : (fill > 1.0F ? 1.0F : fill)), h, color);
}

static void button(UiBox box, const float color[4], bool enabled) {
    const float enabled_tint[4] = {color[0], color[1], color[2], 1.0F};
    const float disabled_tint[4] = {0.72F, 0.70F, 0.66F, 0.98F};
    const RuneAssetId asset = enabled ? RUNE_ASSET_UI_V2_BUTTON_IDLE_SLICE9 : RUNE_ASSET_UI_V2_BUTTON_DISABLED_SLICE9;
    const float left = enabled ? 56.0F : 52.0F;
    const float right = enabled ? 56.0F : 52.0F;
    rune_asset_slice9(asset, box.x, box.y + 4.0F, box.w, box.h, left, 20.0F, right, 20.0F, (float[4]){0.12F, 0.10F, 0.09F, 0.70F});
    rune_asset_slice9(asset, box.x, box.y, box.w, box.h, left, 20.0F, right, 20.0F, enabled ? enabled_tint : disabled_tint);
}

static void primary_button(UiBox box, bool enabled) {
    const RuneAssetId asset = enabled ? RUNE_ASSET_UI_BASES_V2_BUTTON_IDLE_SLICE9 : RUNE_ASSET_UI_BASES_V2_BUTTON_DISABLED_SLICE9;
    const float tint[4] = {1.0F, 0.98F, 0.90F, 1.0F};
    const float disabled_tint[4] = {0.72F, 0.70F, 0.66F, 0.98F};
    rune_asset_slice9(asset, box.x, box.y + 5.0F, box.w, box.h, 96.0F, 44.0F, 96.0F, 44.0F, (float[4]){0.10F, 0.08F, 0.06F, 0.72F});
    rune_asset_slice9(asset, box.x, box.y, box.w, box.h, 96.0F, 44.0F, 96.0F, 44.0F, enabled ? tint : disabled_tint);
}

static float clampf(float value, float lo, float hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

static void rune_asset_centered(RuneAssetId id, float cx, float cy, float w, float h, const float color[4]) {
    rune_asset(id, cx - w * 0.5F, cy - h * 0.5F, w, h, color);
}

static void draw_panel_decor(float x, float y, float w, float h, bool modal, bool compact) {
    const float tint[4] = {0.96F, 0.94F, 0.84F, 1.0F};
    const RuneAssetId top = modal ? RUNE_ASSET_UI_V2_DECOR_MODAL_TOP_GEM : RUNE_ASSET_UI_V2_DECOR_JOURNAL_TOP_GEM;
    const RuneAssetId bottom = modal ? RUNE_ASSET_UI_V2_DECOR_MODAL_BOTTOM_GEM : RUNE_ASSET_UI_V2_DECOR_JOURNAL_BOTTOM_GEM;
    const float top_w = clampf(w * (modal ? 0.24F : 0.27F), compact ? 54.0F : 64.0F, compact ? 82.0F : 112.0F);
    const float top_h = top_w * 0.46F;
    rune_asset_centered(top, x + w * 0.5F, y + (compact ? 3.0F : 7.0F), top_w, top_h, tint);
    if (h > (compact ? 142.0F : 190.0F)) {
        const float bottom_w = top_w * 0.86F;
        rune_asset_centered(bottom, x + w * 0.5F, y + h - (compact ? 16.0F : 24.0F), bottom_w, bottom_w * 0.42F, tint);
    }
}

static void draw_reward_decor(UiBox box, bool compact) {
    const float tint[4] = {0.98F, 0.92F, 0.74F, 1.0F};
    const float w = clampf(box.w * 0.42F, compact ? 42.0F : 56.0F, compact ? 72.0F : 104.0F);
    rune_asset_centered(RUNE_ASSET_UI_V2_DECOR_REWARD_TOP_GEM, box.x + box.w * 0.5F, box.y - 5.0F, w, w * 0.38F, tint);
    rune_asset_centered(RUNE_ASSET_UI_V2_DECOR_REWARD_BOTTOM_GEM, box.x + box.w * 0.5F, box.y + box.h + 9.0F, w * 0.86F, w * 0.32F, tint);
}

static void draw_landmark(RuneAssetId asset, float x, float y, float w, float h, bool active, bool unlocked) {
    const float ring[4] = {0.96F, 0.88F, 0.58F, active ? 0.95F : 0.42F};
    const float tint[4] = {unlocked ? 1.0F : 0.64F, unlocked ? 1.0F : 0.62F, unlocked ? 1.0F : 0.58F, 1.0F};
    circle(x, y, active ? 28.0F : 22.0F, ring);
    rune_asset(asset, x - w * 0.5F, y - h * 0.82F, w, h, tint);
    if (!unlocked) {
        rect_wire(x - 16.0F, y - 8.0F, 32.0F, 28.0F, ring);
    }
}

static const char *fishing_primary_label(void) {
    if (g_game_state.fishing_phase == GAME_STATE_FISHING_PHASE_FULL) {
        return "SELL FULL BAG";
    }
    if (g_game_state.fishing_phase == GAME_STATE_FISHING_PHASE_BITE) {
        return "HOOK BITE";
    }
    if (g_game_state.fishing_phase == GAME_STATE_FISHING_PHASE_REELING) {
        return "REEL";
    }
    return "CAST";
}

static void cube3(float x, float y, float z, float sx, float sy, float sz, const float color[4]) {
    nt_shape_renderer_cube((float[3]){x, y, z}, (float[3]){sx, sy, sz}, color);
}

static void sphere3(float x, float y, float z, float radius, const float color[4]) {
    nt_shape_renderer_sphere((float[3]){x, y, z}, radius, color);
}

static void cyl3(float x, float y, float z, float radius, float height, const float color[4]) {
    nt_shape_renderer_cylinder((float[3]){x, y, z}, radius, height, color);
}

static void quat_axis(float x, float y, float z, float radians, float out[4]) {
    vec3 axis = {x, y, z};
    glm_vec3_normalize(axis);
    glm_quatv(out, radians, axis);
}

static void cube3_rot(float x, float y, float z, float sx, float sy, float sz, const float rot[4], const float color[4]) {
    nt_shape_renderer_cube_rot((float[3]){x, y, z}, (float[3]){sx, sy, sz}, rot, color);
}

static void cyl3_rot(float x, float y, float z, float radius, float height, const float rot[4], const float color[4]) {
    nt_shape_renderer_cylinder_rot((float[3]){x, y, z}, radius, height, rot, color);
}

static void circle3_rot(float x, float y, float z, float radius, const float rot[4], const float color[4]) {
    nt_shape_renderer_circle_rot((float[3]){x, y, z}, radius, rot, color);
}

static void triangle3(float ax, float ay, float az, float bx, float by, float bz, float cx, float cy, float cz, const float color[4]) {
    nt_shape_renderer_triangle((float[3]){ax, ay, az}, (float[3]){bx, by, bz}, (float[3]){cx, cy, cz}, color);
}

static void draw_palm_tree(float x, float z, float scale) {
    float rot_x[4];
    float rot_z[4];
    float rot_leaf[4];
    quat_axis(0.0F, 0.0F, 1.0F, glm_rad(-12.0F), rot_z);
    cyl3_rot(x, 0.72F * scale, z, 0.085F * scale, 1.55F * scale, rot_z, (float[4]){0.55F, 0.27F, 0.09F, 1.0F});
    sphere3(x - 0.12F * scale, 1.48F * scale, z, 0.22F * scale, (float[4]){0.28F, 0.14F, 0.05F, 1.0F});
    quat_axis(1.0F, 0.0F, 0.0F, glm_rad(72.0F), rot_x);
    circle3_rot(x - 0.22F * scale, 1.58F * scale, z, 0.28F * scale, rot_x, (float[4]){0.15F, 0.68F, 0.22F, 1.0F});
    for (int i = 0; i < 6; ++i) {
        const float angle = glm_rad((float)i * 60.0F);
        quat_axis(cosf(angle) * 0.45F, 0.0F, sinf(angle) * 0.45F, glm_rad(24.0F), rot_leaf);
        cube3_rot(x + cosf(angle) * 0.34F * scale, 1.62F * scale, z + sinf(angle) * 0.34F * scale, 0.76F * scale, 0.075F * scale, 0.20F * scale, rot_leaf, (float[4]){0.17F, 0.86F, 0.24F, 1.0F});
    }
}

static void draw_flower_patch(float x, float z, float scale) {
    const float colors[][4] = {
        {1.0F, 0.24F, 0.58F, 1.0F},
        {1.0F, 0.82F, 0.12F, 1.0F},
        {0.58F, 0.24F, 1.0F, 1.0F},
    };
    for (int i = 0; i < 6; ++i) {
        const float ox = ((float)(i % 3) - 1.0F) * 0.16F * scale;
        const float oz = ((float)(i / 3) - 0.5F) * 0.18F * scale;
        cyl3(x + ox, 0.26F * scale, z + oz, 0.018F * scale, 0.26F * scale, (float[4]){0.12F, 0.56F, 0.16F, 1.0F});
        sphere3(x + ox, 0.43F * scale, z + oz, 0.055F * scale, colors[i % 3]);
    }
}

static void draw_fishing_world_3d(float w, float h) {
    mat4 proj;
    mat4 view;
    mat4 vp;
    const float cam_pos[3] = {5.35F, 6.20F, 9.40F};
    glm_perspective(glm_rad(36.0F), w / h, 0.1F, 80.0F, proj);
    glm_lookat((vec3){cam_pos[0], cam_pos[1], cam_pos[2]}, (vec3){-0.42F, 0.82F, 1.55F}, (vec3){0.0F, 1.0F, 0.0F}, view);
    glm_mat4_mul(proj, view, vp);

    nt_shape_renderer_set_vp((const float *)vp);
    nt_shape_renderer_set_cam_pos((float[3]){cam_pos[0], cam_pos[1], cam_pos[2]});
    nt_shape_renderer_set_depth(true);
    nt_shape_renderer_set_line_width(1.2F);

    const float water[4] = {0.02F, 0.72F, 0.95F, 1.0F};
    const float water_deep[4] = {0.00F, 0.37F, 0.88F, 1.0F};
    const float water_light[4] = {0.12F, 0.84F, 0.96F, 1.0F};
    const float sand[4] = {1.0F, 0.82F, 0.30F, 1.0F};
    const float grass[4] = {0.20F, 0.82F, 0.30F, 1.0F};
    const float wood[4] = {0.82F, 0.43F, 0.15F, 1.0F};
    const float wood_dark[4] = {0.42F, 0.20F, 0.08F, 1.0F};
    const float avatar_blue[4] = {0.04F, 0.34F, 0.82F, 1.0F};
    const float skin[4] = {1.0F, 0.72F, 0.34F, 1.0F};
    const float hair[4] = {0.20F, 0.09F, 0.03F, 1.0F};
    const float foam[4] = {0.38F, 0.94F, 0.98F, 0.72F};
    const float leaf[4] = {0.16F, 0.78F, 0.23F, 1.0F};
    float rot_z[4];
    float rot_x[4];
    float flat_rot[4];

    quat_axis(1.0F, 0.0F, 0.0F, glm_rad(90.0F), flat_rot);
    cube3(0.0F, -0.14F, 0.0F, 30.0F, 0.10F, 30.0F, water_deep);
    cube3(-2.8F, -0.08F, -1.6F, 18.0F, 0.08F, 15.0F, water);
    cube3(5.8F, -0.06F, -2.8F, 7.0F, 0.05F, 7.0F, (float[4]){0.18F, 0.86F, 0.96F, 1.0F});
    for (int i = 0; i < 18; ++i) {
        const float x = -9.5F + (float)(i % 8) * 2.25F;
        const float z = -5.8F + (float)(i / 8) * 2.25F + (float)(i % 3) * 0.18F;
        const float len = 0.42F + (float)(i % 4) * 0.18F;
        cube3(x, 0.015F, z, len, 0.012F, 0.026F, (i % 3) == 0 ? foam : water_light);
    }

    sphere3(-4.2F, 0.02F, -1.8F, 1.55F, sand);
    cube3(-4.2F, 0.12F, -1.8F, 2.65F, 0.18F, 2.65F, sand);
    sphere3(-4.4F, 0.35F, -1.9F, 1.10F, grass);
    sphere3(-3.45F, 0.42F, -1.25F, 0.22F, (float[4]){0.40F, 0.54F, 0.58F, 1.0F});
    sphere3(-5.15F, 0.42F, -1.45F, 0.16F, (float[4]){0.34F, 0.48F, 0.54F, 1.0F});
    cube3(-5.0F, 0.78F, -2.2F, 1.0F, 0.90F, 0.80F, (float[4]){0.95F, 0.36F, 0.22F, 1.0F});
    cube3(-5.0F, 1.36F, -2.2F, 1.22F, 0.28F, 1.02F, (float[4]){1.0F, 0.88F, 0.42F, 1.0F});
    cube3(-5.0F, 0.72F, -1.75F, 0.52F, 0.42F, 0.10F, (float[4]){0.10F, 0.68F, 0.86F, 1.0F});
    cube3(-4.98F, 1.68F, -1.72F, 0.86F, 0.18F, 0.12F, (float[4]){0.10F, 0.24F, 0.34F, 1.0F});
    sphere3(-4.64F, 1.68F, -1.62F, 0.13F, (float[4]){1.0F, 0.78F, 0.10F, 1.0F});
    sphere3(-5.00F, 1.68F, -1.62F, 0.13F, (float[4]){0.24F, 0.90F, 1.0F, 1.0F});
    sphere3(-5.36F, 1.68F, -1.62F, 0.13F, (float[4]){1.0F, 0.38F, 0.72F, 1.0F});
    draw_flower_patch(-3.95F, -0.94F, 1.0F);

    sphere3(3.9F, 0.04F, -3.0F, 1.35F, sand);
    sphere3(3.9F, 0.35F, -3.0F, 0.95F, grass);
    cube3(3.45F, 0.78F, -2.95F, 0.12F, 1.05F, 0.12F, wood_dark);
    cube3(4.35F, 0.78F, -2.95F, 0.12F, 1.05F, 0.12F, wood_dark);
    cube3(3.90F, 1.25F, -2.95F, 1.10F, 0.16F, 0.16F, wood_dark);
    quat_axis(0.0F, 0.0F, 1.0F, glm_rad(-42.0F), rot_z);
    cube3_rot(3.62F, 0.74F, -2.95F, 0.12F, 0.80F, 0.10F, rot_z, (float[4]){0.08F, 0.18F, 0.26F, 1.0F});
    quat_axis(0.0F, 0.0F, 1.0F, glm_rad(42.0F), rot_z);
    cube3_rot(4.18F, 0.74F, -2.95F, 0.12F, 0.80F, 0.10F, rot_z, (float[4]){0.08F, 0.18F, 0.26F, 1.0F});
    triangle3(3.34F, 1.56F, -2.94F, 4.46F, 1.56F, -2.94F, 3.90F, 2.08F, -2.94F, (float[4]){1.0F, 0.80F, 0.16F, 1.0F});
    draw_palm_tree(6.20F, -4.82F, 0.34F);

    for (int i = 0; i < 7; ++i) {
        cube3(-2.6F + (float)i * 0.72F, 0.18F, 2.05F, 0.62F, 0.16F, 3.65F, wood);
        cube3(-2.6F + (float)i * 0.72F, 0.285F, 2.05F, 0.50F, 0.035F, 3.42F, (float[4]){0.98F, 0.60F, 0.25F, 1.0F});
    }
    for (int i = 0; i < 4; ++i) {
        cyl3(-2.8F + (float)i * 1.25F, 0.25F, 0.42F, 0.13F, 0.70F, wood_dark);
        cyl3(-2.8F + (float)i * 1.25F, 0.25F, 3.64F, 0.13F, 0.70F, wood_dark);
    }
    cube3(-2.05F, 0.54F, 0.42F, 1.80F, 0.08F, 0.08F, (float[4]){0.55F, 0.27F, 0.08F, 1.0F});
    cube3(-0.82F, 0.54F, 3.64F, 1.80F, 0.08F, 0.08F, (float[4]){0.55F, 0.27F, 0.08F, 1.0F});

    cube3(-1.0F, 0.58F, 2.4F, 0.32F, 0.82F, 0.24F, (float[4]){0.05F, 0.08F, 0.12F, 1.0F});
    cube3(-0.56F, 0.58F, 2.4F, 0.32F, 0.82F, 0.24F, (float[4]){0.05F, 0.08F, 0.12F, 1.0F});
    cube3(-0.78F, 1.10F, 2.35F, 0.82F, 0.88F, 0.40F, avatar_blue);
    cube3(-0.78F, 1.15F, 2.58F, 0.54F, 0.20F, 0.08F, (float[4]){1.0F, 0.92F, 0.18F, 1.0F});
    cube3(-0.78F, 1.72F, 2.32F, 0.56F, 0.48F, 0.46F, skin);
    cube3(-0.78F, 2.02F, 2.32F, 0.62F, 0.20F, 0.50F, hair);
    cube3(-0.78F, 2.16F, 2.32F, 0.92F, 0.10F, 0.70F, (float[4]){1.0F, 0.88F, 0.22F, 1.0F});
    cube3(-0.94F, 1.78F, 2.56F, 0.07F, 0.07F, 0.035F, (float[4]){0.02F, 0.04F, 0.08F, 1.0F});
    cube3(-0.62F, 1.78F, 2.56F, 0.07F, 0.07F, 0.035F, (float[4]){0.02F, 0.04F, 0.08F, 1.0F});
    cube3(-0.78F, 1.62F, 2.56F, 0.20F, 0.045F, 0.035F, (float[4]){0.70F, 0.18F, 0.12F, 1.0F});
    quat_axis(0.0F, 0.0F, 1.0F, glm_rad(-24.0F), rot_z);
    cube3_rot(-0.30F, 1.26F, 2.18F, 0.20F, 0.82F, 0.18F, rot_z, skin);
    sphere3(-0.06F, 1.62F, 2.04F, 0.12F, skin);
    cube3(-1.25F, 1.18F, 2.18F, 0.22F, 0.72F, 0.20F, skin);

    for (int i = 0; i < 13; ++i) {
        const float t = (float)i / 12.0F;
        const float x = -0.06F + (1.30F + 0.06F) * t;
        const float y = 1.62F + (0.82F - 1.62F) * t;
        const float z = 2.04F + (-0.28F - 2.04F) * t;
        sphere3(x, y, z, 0.040F - 0.014F * t, (float[4]){0.52F, 0.24F, 0.06F, 1.0F});
    }
    for (int i = 0; i < 9; ++i) {
        const float t = (float)i / 8.0F;
        sphere3(1.30F + (1.22F - 1.30F) * t, 0.82F + (0.16F - 0.82F) * t, -0.28F + (-0.22F + 0.28F) * t, 0.014F, (float[4]){0.94F, 0.98F, 0.86F, 0.70F});
    }
    sphere3(1.22F, 0.10F, -0.22F, 0.07F, (float[4]){1.0F, 0.18F, 0.30F, 1.0F});
    for (int i = 0; i < 12; ++i) {
        sphere3(2.10F + (float)(i % 4) * 0.24F, 0.48F + (float)(i / 4) * 0.22F, -0.58F + (float)(i % 2) * 0.24F, 0.085F, (float[4]){1.0F, 0.77F, 0.12F, 1.0F});
    }

    quat_axis(1.0F, 0.0F, 0.0F, glm_rad(18.0F), rot_x);
    cube3_rot(-6.4F, 0.75F, -0.30F, 0.12F, 1.45F, 0.12F, rot_x, (float[4]){0.46F, 0.24F, 0.10F, 1.0F});
    sphere3(-6.4F, 1.62F, -0.52F, 0.40F, leaf);
    sphere3(-6.7F, 1.52F, -0.26F, 0.22F, leaf);
    sphere3(-6.15F, 1.46F, -0.22F, 0.22F, leaf);

    nt_shape_renderer_flush();
    draw_fishing_mesh_props((const float *)view, (const float *)proj, (const float *)vp, (float[3]){cam_pos[0], cam_pos[1], cam_pos[2]});
}

static void fishing_button(UiBox box, FishingAssetId asset, const char *label, bool enabled, float scale) {
    const float text_col[4] = {1.0F, 1.0F, 0.94F, enabled ? 1.0F : 0.52F};
    const float r = box.h * 0.42F;
    rect(box.x + r, box.y + 6.0F, box.w - r * 2.0F, box.h - 12.0F, (float[4]){0.02F, 0.08F, 0.14F, 0.38F});
    circle(box.x + r, box.y + box.h * 0.5F + 4.0F, r, (float[4]){0.02F, 0.08F, 0.14F, 0.38F});
    circle(box.x + box.w - r, box.y + box.h * 0.5F + 4.0F, r, (float[4]){0.02F, 0.08F, 0.14F, 0.38F});
    nt_shape_renderer_flush();
    fishing_asset_slice9(asset, box.x, box.y, box.w, box.h, 80.0F, 38.0F, 80.0F, 38.0F, enabled ? (float[4]){1.0F, 1.0F, 1.0F, 1.0F} : (float[4]){0.62F, 0.70F, 0.74F, 0.78F});
    draw_ui_text_fit(label, box, scale * 10.0F, text_col);
}

static void fishing_primary_button(UiBox box, const char *label, bool enabled, float scale) {
    const float text_col[4] = {1.0F, 1.0F, 0.94F, enabled ? 1.0F : 0.60F};
    const float r = box.h * 0.43F;
    rect(box.x + r, box.y + 8.0F, box.w - r * 2.0F, box.h - 12.0F, (float[4]){0.02F, 0.10F, 0.05F, 0.48F});
    circle(box.x + r, box.y + box.h * 0.5F + 5.0F, r, (float[4]){0.02F, 0.10F, 0.05F, 0.48F});
    circle(box.x + box.w - r, box.y + box.h * 0.5F + 5.0F, r, (float[4]){0.02F, 0.10F, 0.05F, 0.48F});
    nt_shape_renderer_flush();
    fishing_asset_slice9(FISHING_ASSET_PRIMARY_BUTTON_SLICE9, box.x, box.y, box.w, box.h, 92.0F, 44.0F, 92.0F, 44.0F, enabled ? (float[4]){1.0F, 1.0F, 1.0F, 1.0F} : (float[4]){0.72F, 0.78F, 0.66F, 0.82F});
    fishing_asset(FISHING_ASSET_ROD_ICON, box.x + box.h * 0.22F, box.y + box.h * 0.18F, box.h * 0.62F, box.h * 0.62F, (float[4]){1.0F, 1.0F, 1.0F, enabled ? 1.0F : 0.60F});
    draw_ui_text_fit(label, (UiBox){box.x + box.h * 0.70F, box.y, box.w - box.h * 0.86F, box.h}, scale * 10.0F, text_col);
}

static void draw_fishing_panel_asset(FishingAssetId asset, float x, float y, float w, float h, float left, float top, float right, float bottom) {
    fishing_asset_slice9(asset, x, y, w, h, left, top, right, bottom, (float[4]){1.0F, 1.0F, 1.0F, 1.0F});
}

static void draw_fishing_scene(float w, float h) {
    const float vp[16] = {0};
    float mutable_vp[16];
    float text_vp[16];
    ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, mutable_vp);
    ortho(0.0F, w, 0.0F, h, -1.0F, 1.0F, text_vp);
    s_fishing_ui_height = h;

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        fishing_mesh_restore_gpu();
        rune_textures_restore_gpu();
        nt_shape_renderer_restore_gpu();
    }
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.35F, 0.84F, 1.0F, 1.0F}, .clear_depth = 1.0F});
    (void)vp;

    draw_fishing_world_3d(w, h);

    nt_shape_renderer_set_vp(mutable_vp);
    rune_textures_set_vp(mutable_vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(4.0F);

    const bool compact = h > w || w < 760.0F;
    const float white[4] = {1.0F, 1.0F, 0.94F, 1.0F};
    const float ink[4] = {0.04F, 0.15F, 0.23F, 1.0F};
    const float mint[4] = {0.26F, 0.94F, 0.72F, 1.0F};
    const float gold[4] = {1.0F, 0.78F, 0.20F, 1.0F};
    char text[128];
    fishing_ttf_prepare((const float *)text_vp);

    const float hud_y = compact ? 10.0F : 12.0F;
    const float hud_h = compact ? 64.0F : 58.0F;
    const float title_w = compact ? w * 0.42F : 214.0F;
    draw_fishing_panel_asset(FISHING_ASSET_STATUS_PILL_SLICE9, 12.0F, hud_y, title_w, hud_h, 82.0F, 32.0F, 82.0F, 32.0F);
    fishing_asset(FISHING_ASSET_ROD_ICON, 23.0F, hud_y + 9.0F, 34.0F, 34.0F, (float[4]){1.0F, 1.0F, 1.0F, 1.0F});
    draw_ui_text("SPLASH RODS", 64.0F, hud_y + 17.0F, compact ? 15.0F : 17.0F, ink);

    const float chip_y = hud_y;
    const float chip_h = hud_h;
    const float coin_x = compact ? w * 0.48F : 244.0F;
    const float level_x = compact ? 12.0F : 402.0F;
    const float bag_x = compact ? w * 0.48F : 560.0F;
    const float index_x = compact ? w * 0.73F : 736.0F;
    if (!compact) {
        draw_fishing_panel_asset(FISHING_ASSET_STATUS_PILL_SLICE9, coin_x, chip_y, 146.0F, chip_h, 82.0F, 32.0F, 82.0F, 32.0F);
        draw_fishing_panel_asset(FISHING_ASSET_STATUS_PILL_SLICE9, level_x, chip_y, 146.0F, chip_h, 82.0F, 32.0F, 82.0F, 32.0F);
        draw_fishing_panel_asset(FISHING_ASSET_STATUS_PILL_SLICE9, bag_x, chip_y, 162.0F, chip_h, 82.0F, 32.0F, 82.0F, 32.0F);
        draw_fishing_panel_asset(FISHING_ASSET_STATUS_PILL_SLICE9, index_x, chip_y, 142.0F, chip_h, 82.0F, 32.0F, 82.0F, 32.0F);
    }
    fishing_asset(FISHING_ASSET_COIN_ICON, coin_x + 10.0F, hud_y + 10.0F, compact ? 28.0F : 34.0F, compact ? 28.0F : 34.0F, (float[4]){1.0F, 1.0F, 1.0F, 1.0F});
    (void)snprintf(text, sizeof(text), "%d", g_game_state.fishing_coins);
    draw_ui_text(text, coin_x + 50.0F, hud_y + 18.0F, compact ? 15.0F : 17.0F, gold);
    (void)snprintf(text, sizeof(text), "LV %d", g_game_state.fishing_level);
    draw_ui_text(text, level_x + 28.0F, hud_y + 18.0F, compact ? 15.0F : 17.0F, ink);
    fishing_asset(FISHING_ASSET_BACKPACK_ICON, bag_x + 10.0F, hud_y + 9.0F, compact ? 30.0F : 36.0F, compact ? 30.0F : 36.0F, (float[4]){1.0F, 1.0F, 1.0F, 1.0F});
    (void)snprintf(text, sizeof(text), "BAG %d/%d", g_game_state.fishing_backpack_count, g_game_state.fishing_backpack_slots);
    draw_ui_text(text, bag_x + 52.0F, hud_y + 18.0F, compact ? 15.0F : 16.0F, ink);
    fishing_asset(FISHING_ASSET_FISH_ICON, index_x + 9.0F, hud_y + 10.0F, compact ? 32.0F : 38.0F, compact ? 28.0F : 32.0F, (float[4]){1.0F, 1.0F, 1.0F, 1.0F});
    (void)snprintf(text, sizeof(text), "IDX %d/5", g_game_state.fishing_index_count);
    draw_ui_text(text, index_x + 50.0F, hud_y + 18.0F, compact ? 15.0F : 16.0F, ink);

    const float catch_x = w * 0.67F;
    const float catch_y = h * 0.36F;
    const float catch_w = w * 0.29F;
    const float catch_h = compact ? 108.0F : 126.0F;
    if (!compact) {
        fishing_asset(FISHING_ASSET_COIN_ICON, catch_x - 48.0F, catch_y + 76.0F, 34.0F, 34.0F, (float[4]){1.0F, 1.0F, 1.0F, 0.92F});
        fishing_asset(FISHING_ASSET_COIN_ICON, catch_x - 24.0F, catch_y + 104.0F, 26.0F, 26.0F, (float[4]){1.0F, 1.0F, 1.0F, 0.86F});
        fishing_asset(FISHING_ASSET_COIN_ICON, catch_x + catch_w + 6.0F, catch_y + 54.0F, 30.0F, 30.0F, (float[4]){1.0F, 1.0F, 1.0F, 0.86F});
    }
    draw_fishing_panel_asset(FISHING_ASSET_CATCH_CARD_SLICE9, catch_x, catch_y, catch_w, catch_h, 58.0F, 44.0F, 58.0F, 44.0F);
    fishing_asset(FISHING_ASSET_FISH_ICON, catch_x + 12.0F, catch_y + 18.0F, compact ? 44.0F : 58.0F, compact ? 38.0F : 50.0F, (float[4]){1.0F, 1.0F, 1.0F, 1.0F});
    draw_ui_text("NEW CATCH", catch_x + 66.0F, catch_y + 16.0F, compact ? 15.0F : 16.0F, gold);
    draw_ui_text_fit(g_game_state.fishing_last_fish, (UiBox){catch_x + 66.0F, catch_y + 43.0F, catch_w - 82.0F, 32.0F}, compact ? 17.0F : 18.0F, white);
    draw_ui_text_fit(g_game_state.fishing_last_reward, (UiBox){catch_x + 66.0F, catch_y + 76.0F, catch_w - 82.0F, 26.0F}, compact ? 15.0F : 16.0F, gold);
    draw_ui_text_fit(g_game_state.fishing_objective, (UiBox){catch_x + 18.0F, catch_y + catch_h - 33.0F, catch_w - 36.0F, 26.0F}, compact ? 14.0F : 15.0F, mint);

    const float meter_x = compact ? 20.0F : w * 0.35F;
    const float meter_y = compact ? h - 220.0F : h - 202.0F;
    const float meter_w = compact ? w - 40.0F : w * 0.30F;
    const bool show_meter = g_game_state.fishing_phase == GAME_STATE_FISHING_PHASE_BITE || g_game_state.fishing_phase == GAME_STATE_FISHING_PHASE_REELING;
    if (show_meter) {
        draw_ui_text(g_game_state.fishing_phase == GAME_STATE_FISHING_PHASE_BITE ? "HOOK THE BITE" : "REEL IT IN",
                     meter_x,
                     meter_y - 30.0F,
                     compact ? 15.0F : 17.0F,
                     white);
        ui_pill(meter_x - 8.0F,
                meter_y - 4.0F,
                meter_w + 16.0F,
                30.0F,
                (float[4]){0.03F, 0.18F, 0.26F, 0.78F},
                (float[4]){0.32F, 0.72F, 0.82F, 0.36F},
                (float[4]){0.02F, 0.06F, 0.08F, 0.36F});
        const float pct = (float)g_game_state.fishing_catch_progress / 100.0F;
        const float fill_w = meter_w * (pct < 0.0F ? 0.0F : (pct > 1.0F ? 1.0F : pct));
        if (fill_w > 8.0F) {
            ui_pill(meter_x,
                    meter_y,
                    fill_w < 22.0F ? 22.0F : fill_w,
                    22.0F,
                    (float[4]){1.0F, 0.38F, 0.70F, 0.98F},
                    (float[4]){1.0F, 0.84F, 0.96F, 0.74F},
                    (float[4]){0.0F, 0.0F, 0.0F, 0.0F});
        }
    }

    fishing_primary_button(s_scout_box, fishing_primary_label(), true, compact ? 1.7F : 2.6F);
    fishing_button(s_rest_box, FISHING_ASSET_SECONDARY_BUTTON_SLICE9, "SELL", game_fishing_can_sell(&g_game_state), compact ? 1.24F : 1.72F);
    fishing_button(s_upgrade_box, FISHING_ASSET_UPGRADE_BUTTON_SLICE9, "LINE", game_fishing_can_buy_better_line(&g_game_state), compact ? 1.20F : 1.62F);

    (void)snprintf(text, sizeof(text), "NEXT %s", g_game_state.fishing_next_unlock);
    const UiBox next_box = {
        compact ? 18.0F : w * 0.34F,
        s_scout_box.y - (compact ? 54.0F : 64.0F),
        compact ? w - 36.0F : w * 0.32F,
        compact ? 42.0F : 52.0F,
    };
    draw_fishing_panel_asset(FISHING_ASSET_STATUS_PILL_SLICE9, next_box.x, next_box.y, next_box.w, next_box.h, 82.0F, 32.0F, 82.0F, 32.0F);
    fishing_asset(FISHING_ASSET_ROD_ICON, next_box.x + 12.0F, next_box.y + 6.0F, next_box.h - 12.0F, next_box.h - 12.0F, (float[4]){1.0F, 1.0F, 1.0F, 1.0F});
    draw_ui_text_fit(text,
                     (UiBox){next_box.x + next_box.h, next_box.y + 7.0F, next_box.w - next_box.h - 16.0F, next_box.h - 14.0F},
                     compact ? 13.0F : 15.0F,
                     ink);

    nt_shape_renderer_flush();
    nt_gfx_end_pass();
    nt_gfx_end_frame();

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
    if (s_pending_capture_path[0] != '\0') {
        char path[sizeof(s_pending_capture_path)];
        (void)snprintf(path, sizeof(path), "%s", s_pending_capture_path);
        s_pending_capture_path[0] = '\0';
        FILE *file = fopen(path, "wb");
        if (file) {
            const int width = (int)w;
            const int height = (int)h;
            const size_t bytes = (size_t)width * (size_t)height * 3U;
            unsigned char *pixels = (unsigned char *)malloc(bytes);
            if (pixels) {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadBuffer(GL_BACK);
                glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
                (void)fprintf(file, "P6\n%d %d\n255\n", width, height);
                for (int y = height - 1; y >= 0; y--) {
                    (void)fwrite(pixels + ((size_t)y * (size_t)width * 3U), 1, (size_t)width * 3U, file);
                }
                free(pixels);
            }
            (void)fclose(file);
        }
    }
#endif

    nt_window_swap_buffers();
}

static void draw_game(float w, float h) {
    draw_fishing_scene(w, h);
    return;

    const float vp[16] = {0};
    float mutable_vp[16];
    ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, mutable_vp);

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        rune_textures_restore_gpu();
        nt_shape_renderer_restore_gpu();
    }
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.05F, 0.07F, 0.08F, 1.0F}, .clear_depth = 1.0F});
    (void)vp;

    nt_shape_renderer_set_vp(mutable_vp);
    rune_textures_set_vp(mutable_vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(3.0F);

    const bool compact = h > w || w < 760.0F;
    char text[96];
    const float white[4] = {0.92F, 0.96F, 0.90F, 1.0F};
    const float muted[4] = {0.62F, 0.70F, 0.66F, 1.0F};
    const float gold[4] = {0.95F, 0.74F, 0.28F, 1.0F};
    const float danger[4] = {0.96F, 0.34F, 0.32F, 1.0F};
    const float arcane[4] = {0.60F, 0.82F, 1.0F, 1.0F};

    const float top_h = compact ? 82.0F : 78.0F;
    rune_asset_slice9(RUNE_ASSET_UI_V2_STATUS_BAR_SLICE9, 8.0F, 6.0F, w - 16.0F, top_h - 12.0F, 72.0F, 20.0F, 72.0F, 20.0F, (float[4]){0.82F, 0.78F, 0.70F, 1.0F});
    if (compact) {
        draw_text_centered("RUNE RPG", w * 0.50F, 6.0F, 1.45F, white);
    }
    const int hp_max = game_rune_hp_max(&g_game_state);
    const float status_icon = compact ? 18.0F : 24.0F;
    const int mana_max = g_game_state.rune_spell_level >= 2 ? 14 : (g_game_state.rune_spell_level > 0 ? 12 : 10);
    if (compact) {
        rune_asset(RUNE_ASSET_UI_V2_ICON_HEALTH, 12.0F, 27.0F, status_icon, status_icon, white);
        (void)snprintf(text, sizeof(text), "HP %d/%d", g_game_state.rune_hp, hp_max);
        draw_text(text, 34.0F, 28.0F, 0.90F, white);
        bar(34.0F, 42.0F, w * 0.28F, 8.0F, g_game_state.rune_hp, hp_max, (float[4]){0.80F, 0.18F, 0.16F, 1.0F});

        rune_asset(RUNE_ASSET_UI_V2_ICON_MANA, 12.0F, 54.0F, status_icon, status_icon, white);
        (void)snprintf(text, sizeof(text), "MP %d/%d", g_game_state.rune_mana, mana_max);
        draw_text(text, 34.0F, 55.0F, 0.84F, arcane);
        bar(34.0F, 68.0F, w * 0.28F, 7.0F, g_game_state.rune_mana, mana_max, (float[4]){0.18F, 0.35F, 0.90F, 1.0F});

        rune_asset(RUNE_ASSET_UI_V2_ICON_SILVER, w * 0.50F, 30.0F, status_icon, status_icon, white);
        (void)snprintf(text, sizeof(text), "SILVER %d", g_game_state.rune_silver);
        draw_text(text, w * 0.57F, 31.0F, 0.78F, gold);
        rune_asset(RUNE_ASSET_UI_V2_ICON_XP, w * 0.50F, 51.0F, status_icon, status_icon, white);
        (void)snprintf(text, sizeof(text), "LV%d  XP%d", g_game_state.rune_player_level, g_game_state.rune_xp % 20);
        draw_text(text, w * 0.57F, 52.0F, 0.76F, white);
    } else {
        rune_asset(RUNE_ASSET_UI_V2_ICON_HEALTH, w * 0.018F, 18.0F, status_icon, status_icon, white);
        (void)snprintf(text, sizeof(text), "HP %d/%d", g_game_state.rune_hp, hp_max);
        draw_text(text, w * 0.04F, 19.0F, 1.28F, white);
        bar(w * 0.04F, 38.0F, w * 0.18F, 12.0F, g_game_state.rune_hp, hp_max, (float[4]){0.80F, 0.18F, 0.16F, 1.0F});
        rune_asset(RUNE_ASSET_UI_V2_ICON_MANA, w * 0.018F, 50.0F, status_icon, status_icon, white);
        (void)snprintf(text, sizeof(text), "MP %d/%d", g_game_state.rune_mana, mana_max);
        draw_text(text, w * 0.04F, 51.0F, 1.15F, arcane);
        bar(w * 0.04F, 66.0F, w * 0.18F, 8.0F, g_game_state.rune_mana, mana_max, (float[4]){0.18F, 0.35F, 0.90F, 1.0F});
        rune_asset(RUNE_ASSET_UI_V2_ICON_SILVER, w * 0.235F, 24.0F, status_icon, status_icon, white);
        (void)snprintf(text, sizeof(text), "SILVER %d", g_game_state.rune_silver);
        draw_text(text, w * 0.26F, 25.0F, 1.18F, gold);
        bar(w * 0.26F, 47.0F, w * 0.18F, 9.0F, g_game_state.rune_silver % 24, 24, (float[4]){0.92F, 0.75F, 0.28F, 1.0F});
        rune_asset(RUNE_ASSET_UI_V2_ICON_XP, w * 0.475F, 24.0F, status_icon, status_icon, white);
        (void)snprintf(text, sizeof(text), "LVL %d XP %d/20", g_game_state.rune_player_level, g_game_state.rune_xp % 20);
        draw_text(text, w * 0.50F, 25.0F, 1.18F, white);
        bar(w * 0.50F, 47.0F, w * 0.18F, 9.0F, g_game_state.rune_xp % 20, 20, (float[4]){0.32F, 0.80F, 0.48F, 1.0F});
        rune_asset(RUNE_ASSET_UI_V2_ICON_ROAD_SAFETY, w * 0.715F, 24.0F, status_icon, status_icon, white);
        (void)snprintf(text, sizeof(text), "ROAD %d", g_game_state.rune_road_safety);
        draw_text(text, w * 0.74F, 25.0F, 1.18F, white);
        bar(w * 0.74F, 47.0F, w * 0.18F, 9.0F, g_game_state.rune_road_safety, 4, (float[4]){0.48F, 0.74F, 0.86F, 1.0F});
    }
    if (!compact) {
        draw_seed_shape(w * 1.12F, h * 0.16F);
    }

    const float map_x = compact ? 12.0F : w * 0.05F;
    const float map_y = compact ? top_h + 6.0F : top_h + h * 0.04F;
    const float map_w = compact ? w - 24.0F : w * 0.58F;
    const float map_h = compact ? h * 0.29F : h * 0.52F;
    rune_asset(RUNE_ASSET_MAP_BACKGROUND, map_x, map_y, map_w, map_h, (float[4]){0.92F, 0.96F, 0.92F, 1.0F});
    for (int i = 0; i < 9; ++i) {
        const float tx = map_x + map_w * (0.12F + 0.09F * (float)i);
        const float ty = map_y + map_h * (0.32F + 0.08F * (float)(i % 3));
        circle(tx, ty, 4.0F + (float)(i % 2) * 1.5F, (float[4]){0.11F, 0.20F, 0.13F, 0.55F});
    }
    rect(map_x + map_w * 0.18F, map_y + map_h * 0.46F, map_w * 0.58F, 8.0F, (float[4]){0.38F, 0.30F, 0.20F, 1.0F});
    rect(map_x + map_w * 0.42F, map_y + map_h * 0.20F, 8.0F, map_h * 0.44F, (float[4]){0.28F, 0.22F, 0.18F, 1.0F});
    draw_landmark(
        RUNE_ASSET_LANDMARK_MIREGATE,
        map_x + map_w * 0.18F,
        map_y + map_h * 0.50F,
        compact ? 64.0F : 78.0F,
        compact ? 78.0F : 92.0F,
        g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_MIREGATE,
        true);
    draw_landmark(
        RUNE_ASSET_LANDMARK_WISPFEN_ROAD,
        map_x + map_w * 0.50F,
        map_y + map_h * 0.50F,
        compact ? 64.0F : 78.0F,
        compact ? 78.0F : 92.0F,
        g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_WISPFEN_ROAD,
        true);
    draw_landmark(
        RUNE_ASSET_LANDMARK_OLD_BELL_TOWER_LOCKED,
        map_x + map_w * 0.78F,
        map_y + map_h * 0.34F,
        compact ? 62.0F : 76.0F,
        compact ? 86.0F : 106.0F,
        g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_OLD_BELL_TOWER,
        g_game_state.rune_tower_unlocked);
    draw_landmark(
        RUNE_ASSET_LANDMARK_WISPFEN_ROAD,
        map_x + map_w * 0.88F,
        map_y + map_h * 0.66F,
        compact ? 48.0F : 58.0F,
        compact ? 56.0F : 68.0F,
        g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_REEDMERE_CROSSING,
        g_game_state.rune_east_road_unlocked);
    draw_landmark(
        RUNE_ASSET_LANDMARK_WISPFEN_ROAD,
        map_x + map_w * 0.94F,
        map_y + map_h * 0.33F,
        compact ? 38.0F : 48.0F,
        compact ? 46.0F : 58.0F,
        g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_GREENFEN_CAUSEWAY,
        g_game_state.rune_causeway_unlocked);
    const bool route_visible = g_game_state.rune_spell_level >= 2 || g_game_state.rune_briar_gate_unlocked || g_game_state.rune_moonwell_unlocked;
    const bool ashen_visible = g_game_state.rune_briar_gate_safety > 0 || g_game_state.rune_ashen_cairn_unlocked;
    const bool starfall_visible = g_game_state.rune_moonwell_safety > 0 || g_game_state.rune_starfall_grotto_unlocked;
    draw_landmark(
        RUNE_ASSET_LANDMARK_OLD_BELL_TOWER_LOCKED,
        map_x + map_w * 0.70F,
        map_y + map_h * 0.08F,
        compact ? 34.0F : 42.0F,
        compact ? 44.0F : 54.0F,
        g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_BRIAR_GATE,
        route_visible);
    draw_landmark(
        RUNE_ASSET_LANDMARK_WISPFEN_ROAD,
        map_x + map_w * 0.96F,
        map_y + map_h * 0.10F,
        compact ? 32.0F : 40.0F,
        compact ? 38.0F : 48.0F,
        g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_MOONWELL,
        route_visible);
    draw_landmark(
        RUNE_ASSET_LANDMARK_OLD_BELL_TOWER_LOCKED,
        map_x + map_w * 0.78F,
        map_y + map_h * 0.03F,
        compact ? 30.0F : 36.0F,
        compact ? 38.0F : 46.0F,
        g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_ASHEN_CAIRN,
        ashen_visible);
    draw_landmark(
        RUNE_ASSET_LANDMARK_WISPFEN_ROAD,
        map_x + map_w * 0.88F,
        map_y + map_h * 0.22F,
        compact ? 30.0F : 36.0F,
        compact ? 38.0F : 46.0F,
        g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_STARFALL_GROTTO,
        starfall_visible);
    draw_text("MIREGATE", map_x + map_w * 0.08F, map_y + map_h * 0.69F, compact ? 1.15F : 1.5F, white);
    draw_text("WISPFEN", map_x + map_w * 0.42F, map_y + map_h * 0.69F, compact ? 1.15F : 1.5F, white);
    draw_text(g_game_state.rune_tower_unlocked ? "BELL TOWER" : "LOCKED", map_x + map_w * 0.66F, map_y + map_h * 0.15F, compact ? 0.86F : 1.4F, g_game_state.rune_tower_unlocked ? white : muted);
    draw_text(g_game_state.rune_east_road_unlocked ? "REEDMERE" : "EAST", map_x + map_w * 0.79F, map_y + map_h * 0.78F, compact ? 0.9F : 1.15F, g_game_state.rune_east_road_unlocked ? white : muted);
    if (!compact) {
        draw_text(g_game_state.rune_causeway_unlocked ? "CAUSEWAY" : "PASS", map_x + map_w * 0.82F, map_y + map_h * 0.30F, 0.95F, g_game_state.rune_causeway_unlocked ? white : muted);
    }
    if (route_visible && !compact) {
        draw_text("BRIAR", map_x + map_w * 0.61F, map_y + map_h * 0.06F, compact ? 0.72F : 0.9F, g_game_state.rune_briar_gate_unlocked ? white : muted);
        draw_text("MOON", map_x + map_w * 0.90F, map_y + map_h * 0.06F, compact ? 0.72F : 0.9F, g_game_state.rune_moonwell_unlocked ? white : muted);
    }
    if (ashen_visible && !compact) {
        draw_text(compact ? "CAIRN" : "ASHEN", map_x + map_w * 0.72F, map_y + map_h * 0.02F, compact ? 0.62F : 0.82F, g_game_state.rune_ashen_cairn_unlocked ? white : muted);
    }
    if (starfall_visible && !compact) {
        draw_text(compact ? "STAR" : "STARFALL", map_x + map_w * 0.82F, map_y + map_h * 0.22F, compact ? 0.62F : 0.82F, g_game_state.rune_starfall_grotto_unlocked ? white : muted);
    }

    const bool in_combat = g_game_state.rune_encounter != GAME_STATE_RUNE_ENCOUNTER_NONE;
    const float panel_x = compact ? 12.0F : w * 0.67F;
    const float panel_y = compact ? map_y + map_h + 10.0F : map_y;
    const float panel_w = compact ? w - 24.0F : w * 0.28F;
    float panel_h = compact ? s_scout_box.y - panel_y - 18.0F : map_h;
    if (panel_h < 128.0F) {
        panel_h = 128.0F;
    }
    const bool generated_panel_fits = in_combat ? (panel_w >= 360.0F && panel_h >= 240.0F) : (panel_w >= 220.0F && panel_h >= 280.0F);
    const bool use_generated_panel_base = !compact && generated_panel_fits;
    const RuneAssetId panel_asset = use_generated_panel_base
                                        ? (in_combat ? RUNE_ASSET_UI_BASES_V2_MODAL_PANEL_SLICE9 : RUNE_ASSET_UI_BASES_V2_JOURNAL_PANEL_SLICE9)
                                        : (in_combat ? RUNE_ASSET_UI_V2_MODAL_PANEL_SLICE9 : RUNE_ASSET_UI_V2_JOURNAL_PANEL_SLICE9);
    const float panel_l = use_generated_panel_base ? (in_combat ? 104.0F : 84.0F) : (in_combat ? 92.0F : 54.0F);
    const float panel_t = use_generated_panel_base ? (in_combat ? 100.0F : 104.0F) : (in_combat ? 96.0F : 74.0F);
    const float panel_r = use_generated_panel_base ? (in_combat ? 104.0F : 84.0F) : (in_combat ? 92.0F : 54.0F);
    const float panel_b = use_generated_panel_base ? (in_combat ? 100.0F : 104.0F) : (in_combat ? 82.0F : 64.0F);
    rune_asset_slice9(panel_asset,
                      panel_x,
                      panel_y,
                      panel_w,
                      panel_h,
                      panel_l,
                      panel_t,
                      panel_r,
                      panel_b,
                      use_generated_panel_base ? (float[4]){1.0F, 0.98F, 0.90F, 1.0F} : (float[4]){0.88F, 0.84F, 0.76F, 1.0F});
    draw_panel_decor(panel_x, panel_y, panel_w, panel_h, in_combat, compact);
    const bool reed_combat = g_game_state.rune_encounter == GAME_STATE_RUNE_ENCOUNTER_REED_RAIDER;
    const bool fen_combat = g_game_state.rune_encounter == GAME_STATE_RUNE_ENCOUNTER_FEN_SHADE;
    const bool briar_combat = g_game_state.rune_encounter == GAME_STATE_RUNE_ENCOUNTER_BRIAR_STALKER;
    const bool moonwell_combat = g_game_state.rune_encounter == GAME_STATE_RUNE_ENCOUNTER_MOONWELL_SENTINEL;
    const float panel_title_y = use_generated_panel_base ? panel_y + 30.0F : panel_y + 18.0F;
    const float panel_title_scale = compact ? 1.18F : (use_generated_panel_base ? 1.45F : 1.8F);
    draw_text_centered(
        in_combat ? (briar_combat ? "BRIAR STALKER" : (moonwell_combat ? "MOON SENTINEL" : (fen_combat ? "FEN SHADE" : (reed_combat ? "REED RAIDER" : "MIRE WISP")))) : "WARDEN JOURNAL",
        panel_x + panel_w * 0.5F,
        panel_title_y,
        panel_title_scale,
        white);
    if (in_combat) {
        const float enemy_w = compact ? panel_w * 0.28F : panel_w * 0.54F;
        const float enemy_h = enemy_w * 0.89F;
        rune_asset(
            RUNE_ASSET_ENEMY_MIRE_WISP,
            panel_x + panel_w * 0.50F - enemy_w * 0.5F,
            panel_y + panel_h * (compact ? 0.17F : 0.21F),
            enemy_w,
            enemy_h,
            briar_combat     ? (float[4]){1.0F, 0.78F, 0.58F, 1.0F}
            : moonwell_combat ? (float[4]){0.64F, 0.82F, 1.0F, 1.0F}
            : fen_combat      ? (float[4]){0.72F, 1.0F, 0.86F, 1.0F}
            : reed_combat     ? (float[4]){0.82F, 1.0F, 0.72F, 1.0F}
                              : (float[4]){1.0F, 1.0F, 1.0F, 1.0F});
        bar(panel_x + panel_w * 0.16F, panel_y + panel_h * 0.66F, panel_w * 0.68F, 13.0F, g_game_state.rune_enemy_hp, game_rune_enemy_max_hp(&g_game_state), (float[4]){0.86F, 0.24F, 0.32F, 1.0F});
        (void)snprintf(text, sizeof(text), "ENEMY HP %d/%d", g_game_state.rune_enemy_hp, game_rune_enemy_max_hp(&g_game_state));
        draw_text_centered(text, panel_x + panel_w * 0.5F, panel_y + panel_h * 0.72F, compact ? 1.1F : 1.4F, danger);
    } else {
        const float icon_w = compact ? panel_w * 0.17F : panel_w * 0.42F;
        rune_asset(g_game_state.rune_location == GAME_STATE_RUNE_LOCATION_REEDMERE_CROSSING ? RUNE_ASSET_LANDMARK_MIREGATE : RUNE_ASSET_LANDMARK_WISPFEN_ROAD, panel_x + panel_w * 0.50F - icon_w * 0.5F, panel_y + panel_h * (compact ? 0.15F : 0.19F), icon_w, icon_w * 1.16F, (float[4]){0.92F, 1.0F, 0.94F, 0.92F});
        if (g_game_state.rune_reward_text[0] != '\0') {
            const float reward_h = compact ? 16.0F : 22.0F;
            const float reward_y = panel_y + panel_h * (compact ? 0.47F : 0.51F);
            const UiBox reward_box = {panel_x + panel_w * 0.08F, reward_y, panel_w * 0.84F, reward_h};
            rune_asset_slice9(RUNE_ASSET_UI_V2_REWARD_CHIP_SLICE9, reward_box.x, reward_box.y - 8.0F, reward_box.w, reward_box.h + 18.0F, 44.0F, 20.0F, 44.0F, 20.0F, (float[4]){0.92F, 0.82F, 0.62F, 1.0F});
            draw_reward_decor(reward_box, compact);
            draw_text_fit(g_game_state.rune_reward_text, reward_box, compact ? 0.86F : 1.05F, gold);
        }
        bar(panel_x + panel_w * 0.16F, panel_y + panel_h * (compact ? 0.56F : 0.62F), panel_w * 0.68F, compact ? 11.0F : 13.0F, g_game_state.rune_main_quest_step, 18, (float[4]){0.92F, 0.72F, 0.25F, 1.0F});
        (void)snprintf(text, sizeof(text), "MAIN %d/18", g_game_state.rune_main_quest_step);
        draw_text_centered(text, panel_x + panel_w * 0.5F, panel_y + panel_h * (compact ? 0.66F : 0.72F), compact ? 0.92F : 1.4F, gold);
        if (compact) {
            draw_text_centered("SCOUT THE ROAD", panel_x + panel_w * 0.5F, panel_y + panel_h * 0.78F, 0.86F, white);
        } else {
            (void)snprintf(text, sizeof(text), "SIDE %d/3  EAST %d  GREEN %d  BRIAR %d  MOON %d", g_game_state.rune_side_quest_step, g_game_state.rune_east_road_safety, g_game_state.rune_greenfen_safety, g_game_state.rune_briar_gate_safety, g_game_state.rune_moonwell_safety);
            draw_text_centered(text, panel_x + panel_w * 0.5F, panel_y + panel_h * 0.76F, 0.9F, muted);
        }
    }
    if (in_combat) {
        bar(panel_x + panel_w * 0.16F, panel_y + panel_h * 0.78F, panel_w * 0.68F, 10.0F, g_game_state.rune_spell_level, 2, (float[4]){0.50F, 0.86F, 0.98F, 1.0F});
    }
    if (!compact) {
        (void)snprintf(text, sizeof(text), "SPARK DMG %d", game_rune_spark_damage(&g_game_state));
        draw_text_centered(text, panel_x + panel_w * 0.5F, panel_y + panel_h * 0.84F, 1.4F, arcane);
    }
    if (g_game_state.rune_kindness_reputation > 0 && !compact) {
        (void)snprintf(text, sizeof(text), "KIND %d  FAVOR %d", g_game_state.rune_kindness_reputation, g_game_state.rune_spirit_favor);
        draw_text_centered(text, panel_x + panel_w * 0.5F, panel_y + panel_h * 0.90F, 1.05F, gold);
        draw_text_centered(g_game_state.rune_objective, panel_x + panel_w * 0.5F, panel_y + panel_h * 0.94F, 1.05F, white);
    } else if (!compact) {
        draw_text_centered(g_game_state.rune_objective, panel_x + panel_w * 0.5F, panel_y + panel_h * 0.91F, 1.25F, white);
    }

    const bool side_choice = g_game_state.rune_side_quest_step == 2 && g_game_state.rune_bell_rope_charm;
    const bool shrine_choice = game_rune_can_light_moss_shrine(&g_game_state);
    const bool causeway_choice = game_rune_can_open_causeway(&g_game_state);
    const bool lore_choice = game_rune_can_study_rune_lore(&g_game_state);
    const bool route_choice = game_rune_can_choose_next_route(&g_game_state);
    const bool route_locked = g_game_state.rune_route_choice != GAME_STATE_RUNE_ROUTE_CHOICE_NONE;
    const bool briar_scout = game_rune_can_scout_briar_gate(&g_game_state);
    const bool moonwell_scout = game_rune_can_scout_moonwell(&g_game_state);
    const bool ashen_discovery = game_rune_can_discover_ashen_cairn(&g_game_state);
    const bool starfall_discovery = game_rune_can_discover_starfall_grotto(&g_game_state);
    const bool can_upgrade = g_game_state.rune_spell_level == 0 && g_game_state.rune_silver >= 12 && g_game_state.rune_sparks >= 1;
    const bool can_primary = !in_combat;
    if (in_combat) {
        button(s_strike_box, (float[4]){0.58F, 0.23F, 0.20F, 1.0F}, true);
        button(s_spark_box, (float[4]){0.35F, 0.26F, 0.78F, 1.0F}, g_game_state.rune_mana >= 3);
        button(s_guard_box, (float[4]){0.25F, 0.42F, 0.52F, 1.0F}, true);
        button(s_retreat_box, (float[4]){0.36F, 0.30F, 0.26F, 1.0F}, true);
        draw_text_fit("STRIKE", s_strike_box, 1.8F, white);
        draw_text_fit("SPARK", s_spark_box, 1.8F, g_game_state.rune_mana >= 3 ? white : muted);
        draw_text_fit("GUARD", s_guard_box, 1.8F, white);
        draw_text_fit("RETREAT", s_retreat_box, 1.5F, white);
    }
    button(s_rest_box, (float[4]){0.22F, 0.48F, 0.35F, 1.0F}, true);
    if (compact) {
        button(s_scout_box, (float[4]){0.24F, 0.50F, 0.70F, 1.0F}, !in_combat && can_primary);
    } else {
        primary_button(s_scout_box, !in_combat && can_primary);
    }
    button(s_upgrade_box, (float[4]){0.72F, 0.54F, 0.20F, 1.0F}, side_choice || shrine_choice || lore_choice || route_choice || can_upgrade);
    draw_text_fit(side_choice ? "TAKE 6" : "REST", s_rest_box, side_choice ? 1.35F : 1.8F, white);
    const char *primary_label = "SCOUT ROAD";
    if (g_game_state.rune_tower_unlocked && !g_game_state.rune_tower_inspected) {
        primary_label = "INSPECT";
    } else if (route_choice) {
        primary_label = "BRIAR GATE";
    } else if (briar_scout) {
        primary_label = "BRIAR FIGHT";
    } else if (moonwell_scout) {
        primary_label = "MOON TRIAL";
    } else if (ashen_discovery) {
        primary_label = "ASHEN MAP";
    } else if (starfall_discovery) {
        primary_label = "STAR MAP";
    } else if (route_locked) {
        primary_label = g_game_state.rune_route_choice == GAME_STATE_RUNE_ROUTE_CHOICE_MOONWELL ? (g_game_state.rune_starfall_grotto_unlocked ? "STARFALL" : "MOONWELL")
                                                                                                : (g_game_state.rune_ashen_cairn_unlocked ? "ASHEN" : "BRIAR SAFE");
    } else if (causeway_choice) {
        primary_label = "OPEN PASS";
    } else if (g_game_state.rune_causeway_unlocked) {
        primary_label = "GREENFEN";
    } else if (g_game_state.rune_east_road_unlocked) {
        primary_label = "NEXT ROAD";
    }
    draw_text_fit(primary_label, s_scout_box, 2.0F, !in_combat && can_primary ? white : muted);
    draw_text_fit(
        side_choice ? "GIVE ROPE" : (shrine_choice ? "BLESSING" : (lore_choice ? "STUDY" : (route_choice ? "MOONWELL" : (g_game_state.rune_spell_level >= 2 ? "WARD II" : (g_game_state.rune_spirit_favor > 0 ? "FAVOR 1" : (g_game_state.rune_spell_level > 0 ? "WARD ON" : "UPGRADE")))))),
        s_upgrade_box,
        side_choice || shrine_choice || route_choice ? 1.25F : 1.5F,
        side_choice || shrine_choice || lore_choice || route_choice || can_upgrade ? white : muted);

    (void)snprintf(text, sizeof(text), "LOG: %s", g_game_state.rune_combat_log);
    draw_text(text, w * 0.05F, h - 12.0F, 1.15F, muted);

    nt_shape_renderer_flush();
    nt_gfx_end_pass();
    nt_gfx_end_frame();

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
    if (s_pending_capture_path[0] != '\0') {
        char path[sizeof(s_pending_capture_path)];
        (void)snprintf(path, sizeof(path), "%s", s_pending_capture_path);
        s_pending_capture_path[0] = '\0';
        FILE *file = fopen(path, "wb");
        if (file) {
            const int width = (int)w;
            const int height = (int)h;
            const size_t bytes = (size_t)width * (size_t)height * 3U;
            unsigned char *pixels = (unsigned char *)malloc(bytes);
            if (pixels) {
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadBuffer(GL_BACK);
                glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
                (void)fprintf(file, "P6\n%d %d\n255\n", width, height);
                for (int y = height - 1; y >= 0; y--) {
                    (void)fwrite(pixels + ((size_t)y * (size_t)width * 3U), 1, (size_t)width * 3U, file);
                }
                free(pixels);
            }
            (void)fclose(file);
        }
    }
#endif

    nt_window_swap_buffers();
}

static void autosave_if_dirty(void) {
    if (!s_autosave_enabled || !game_state_is_dirty()) {
        return;
    }
    char error[256] = {0};
    char *json = game_state_save_json_string(&g_game_state, error, (int)sizeof(error));
    if (!json) {
        (void)fprintf(stderr, "autosave serialize failed: %s\n", error);
        return;
    }
    if (game_storage_save_json(DEFAULT_SAVE_KEY, GAME_STATE_DOCUMENT, json, error, (int)sizeof(error))) {
        game_state_clear_dirty();
    } else {
        (void)fprintf(stderr, "autosave failed: %s\n", error);
    }
    free(json);
}

static void load_default_save_if_available(void) {
    if (s_fresh_state_requested || !s_autosave_enabled) {
        return;
    }
    char error[256] = {0};
    char *json = NULL;
    if (!game_storage_load_json(DEFAULT_SAVE_KEY, GAME_STATE_DOCUMENT, &json, error, (int)sizeof(error))) {
        return;
    }
    if (!game_state_load_json_string(&g_game_state, json, error, (int)sizeof(error))) {
        (void)fprintf(stderr, "autosave load failed: %s\n", error);
    }
    free(json);
    game_state_clear_dirty();
}

static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--devapi") == 0) {
            s_devapi_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                s_devapi_port = (uint16_t)strtoul(argv[++i], NULL, 10);
            }
        } else if (strcmp(argv[i], "--fresh-state") == 0) {
            s_fresh_state_requested = true;
        } else if (strcmp(argv[i], "--disable-autosave") == 0) {
            s_autosave_enabled = false;
        } else if (strcmp(argv[i], "--window-size") == 0 && i + 1 < argc) {
            int width = 0;
            int height = 0;
            if (sscanf(argv[++i], "%dx%d", &width, &height) == 2 && width > 0 && height > 0) {
                s_window_width = width;
                s_window_height = height;
            }
        }
    }
}

#if NT_DEVAPI_ENABLED
static cJSON *state_json(void) {
    cJSON *state = game_state_to_json(&g_game_state);
    cJSON_AddNumberToObject(state, "frame", (double)g_nt_app.frame);
    cJSON_AddBoolToObject(state, "state_dirty", game_state_is_dirty());
    cJSON *seed = cJSON_AddObjectToObject(state, "seed");
    cJSON_AddStringToObject(seed, "shape", game_seed_shape_label(&g_game_state));
    cJSON_AddBoolToObject(seed, "can_cycle", true);
    cJSON *fishing = cJSON_AddObjectToObject(state, "fishing_summary");
    cJSON_AddStringToObject(fishing, "phase", game_state_fishing_phase_name(g_game_state.fishing_phase));
    cJSON_AddNumberToObject(fishing, "coins", g_game_state.fishing_coins);
    cJSON_AddNumberToObject(fishing, "level", g_game_state.fishing_level);
    cJSON_AddNumberToObject(fishing, "xp", g_game_state.fishing_xp);
    cJSON_AddNumberToObject(fishing, "backpack_count", g_game_state.fishing_backpack_count);
    cJSON_AddNumberToObject(fishing, "backpack_slots", g_game_state.fishing_backpack_slots);
    cJSON_AddNumberToObject(fishing, "backpack_value", g_game_state.fishing_backpack_value);
    cJSON_AddNumberToObject(fishing, "catch_progress", g_game_state.fishing_catch_progress);
    cJSON_AddNumberToObject(fishing, "index_count", g_game_state.fishing_index_count);
    cJSON_AddNumberToObject(fishing, "total_catches", g_game_state.fishing_total_catches);
    cJSON_AddNumberToObject(fishing, "better_line_level", g_game_state.fishing_better_line_level);
    cJSON_AddStringToObject(fishing, "last_fish", g_game_state.fishing_last_fish);
    cJSON_AddStringToObject(fishing, "last_rarity", g_game_state.fishing_last_rarity);
    cJSON_AddStringToObject(fishing, "objective", g_game_state.fishing_objective);
    cJSON_AddStringToObject(fishing, "last_reward", g_game_state.fishing_last_reward);
    cJSON_AddBoolToObject(fishing, "glb_props_ready", s_fishing_mesh_ready);
    cJSON_AddNumberToObject(fishing, "mesh_draw_groups", s_fishing_mesh_last_draw_groups);
    cJSON_AddNumberToObject(fishing, "mesh_instances", s_fishing_mesh_last_instances);
    cJSON_AddBoolToObject(fishing, "can_cast", game_fishing_can_cast(&g_game_state));
    cJSON_AddBoolToObject(fishing, "can_reel", game_fishing_can_reel(&g_game_state));
    cJSON_AddBoolToObject(fishing, "can_sell", game_fishing_can_sell(&g_game_state));
    cJSON_AddBoolToObject(fishing, "can_buy_better_line", game_fishing_can_buy_better_line(&g_game_state));
    cJSON *rune = cJSON_AddObjectToObject(state, "rune_summary");
    cJSON_AddStringToObject(rune, "location", game_state_rune_location_name(g_game_state.rune_location));
    cJSON_AddStringToObject(rune, "encounter", game_state_rune_encounter_name(g_game_state.rune_encounter));
    cJSON_AddStringToObject(rune, "route_choice", game_state_rune_route_choice_name(g_game_state.rune_route_choice));
    cJSON_AddNumberToObject(rune, "spark_damage", game_rune_spark_damage(&g_game_state));
    cJSON_AddNumberToObject(rune, "hp_max", game_rune_hp_max(&g_game_state));
    cJSON_AddNumberToObject(rune, "player_level", g_game_state.rune_player_level);
    cJSON_AddBoolToObject(rune, "can_scout", g_game_state.rune_encounter == GAME_STATE_RUNE_ENCOUNTER_NONE);
    cJSON_AddBoolToObject(rune, "can_scout_east", g_game_state.rune_east_road_unlocked && g_game_state.rune_encounter == GAME_STATE_RUNE_ENCOUNTER_NONE);
    cJSON_AddBoolToObject(rune, "can_buy_spark_ward", g_game_state.rune_spell_level == 0 && g_game_state.rune_silver >= 12 && g_game_state.rune_sparks >= 1);
    cJSON_AddBoolToObject(rune, "can_choose_bell_rope", g_game_state.rune_side_quest_step == 2 && g_game_state.rune_bell_rope_charm);
    cJSON_AddBoolToObject(rune, "can_inspect_tower", g_game_state.rune_tower_unlocked && !g_game_state.rune_tower_inspected);
    cJSON_AddBoolToObject(rune, "can_light_moss_shrine", game_rune_can_light_moss_shrine(&g_game_state));
    cJSON_AddBoolToObject(rune, "can_open_causeway", game_rune_can_open_causeway(&g_game_state));
    cJSON_AddBoolToObject(rune, "can_scout_greenfen", game_rune_can_scout_greenfen(&g_game_state));
    cJSON_AddBoolToObject(rune, "can_study_rune_lore", game_rune_can_study_rune_lore(&g_game_state));
    cJSON_AddBoolToObject(rune, "can_choose_next_route", game_rune_can_choose_next_route(&g_game_state));
    cJSON_AddBoolToObject(rune, "can_scout_briar_gate", game_rune_can_scout_briar_gate(&g_game_state));
    cJSON_AddBoolToObject(rune, "can_scout_moonwell", game_rune_can_scout_moonwell(&g_game_state));
    cJSON_AddBoolToObject(rune, "can_discover_ashen_cairn", game_rune_can_discover_ashen_cairn(&g_game_state));
    cJSON_AddBoolToObject(rune, "can_discover_starfall_grotto", game_rune_can_discover_starfall_grotto(&g_game_state));
    return state;
}

static cJSON *rune_telemetry_json(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "schema", "rune_marches.runtime_telemetry");
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddNumberToObject(root, "frame", (double)g_nt_app.frame);
    cJSON *events = cJSON_AddArrayToObject(root, "events");
    for (int i = 0; i < RUNE_TELEMETRY_COUNT; ++i) {
        cJSON *event = cJSON_CreateObject();
        cJSON_AddStringToObject(event, "id", s_rune_telemetry_ids[i]);
        cJSON_AddNumberToObject(event, "count", s_rune_telemetry[i].count);
        cJSON_AddBoolToObject(event, "recorded", s_rune_telemetry[i].count > 0);
        cJSON_AddNumberToObject(event, "first_frame", (double)s_rune_telemetry[i].first_frame);
        cJSON_AddNumberToObject(event, "last_frame", (double)s_rune_telemetry[i].last_frame);
        cJSON_AddItemToArray(events, event);
    }
    return root;
}

static bool ep_game_state(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    *result = state_json();
    return true;
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    rune_telemetry_reset();
    game_fishing_reset_playtest(&g_game_state);
    rune_telemetry_record(RUNE_TELEMETRY_SESSION_START);
    *result = state_json();
    return true;
}

static bool ep_game_rune_telemetry(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    *result = rune_telemetry_json();
    return true;
}

static bool ep_game_rune_telemetry_reset(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    rune_telemetry_reset();
    rune_telemetry_record(RUNE_TELEMETRY_SESSION_START);
    *result = rune_telemetry_json();
    return true;
}

static bool ep_game_action_cycle(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_fishing_primary_action);
    *result = state_json();
    return true;
}

static bool ep_game_fishing_primary(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_fishing_primary_action);
    *result = state_json();
    return true;
}

static bool ep_game_fishing_cast(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_fishing_cast);
    *result = state_json();
    return true;
}

static bool ep_game_fishing_reel(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_fishing_reel);
    *result = state_json();
    return true;
}

static bool ep_game_fishing_sell_all(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_fishing_sell_all);
    *result = state_json();
    return true;
}

static bool ep_game_fishing_upgrade_better_line(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_fishing_buy_better_line);
    *result = state_json();
    return true;
}

static bool ep_game_rune_scout(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_scout);
    *result = state_json();
    return true;
}

static bool ep_game_rune_primary_action(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_primary_action);
    *result = state_json();
    return true;
}

static bool ep_game_rune_scout_east(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_scout_east);
    *result = state_json();
    return true;
}

static bool ep_game_rune_scout_greenfen(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_scout_greenfen);
    *result = state_json();
    return true;
}

static bool ep_game_rune_strike(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_strike);
    *result = state_json();
    return true;
}

static bool ep_game_rune_spark(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_spark);
    *result = state_json();
    return true;
}

static bool ep_game_rune_guard(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_guard);
    *result = state_json();
    return true;
}

static bool ep_game_rune_retreat(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_retreat);
    *result = state_json();
    return true;
}

static bool ep_game_rune_rest(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_rest);
    *result = state_json();
    return true;
}

static bool ep_game_rune_buy_spark_ward(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_buy_spark_ward);
    *result = state_json();
    return true;
}

static bool ep_game_rune_study_rune_lore(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_study_rune_lore);
    *result = state_json();
    return true;
}

static bool ep_game_rune_bell_rope_silver(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_choose_bell_rope_silver);
    *result = state_json();
    return true;
}

static bool ep_game_rune_bell_rope_kindness(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_choose_bell_rope_kindness);
    *result = state_json();
    return true;
}

static bool ep_game_rune_inspect_tower(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_inspect_tower);
    *result = state_json();
    return true;
}

static bool ep_game_rune_light_moss_shrine(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_light_moss_shrine);
    *result = state_json();
    return true;
}

static bool ep_game_rune_open_causeway(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_open_causeway);
    *result = state_json();
    return true;
}

static bool ep_game_rune_choose_briar_gate(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_choose_briar_gate);
    *result = state_json();
    return true;
}

static bool ep_game_rune_choose_moonwell(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_choose_moonwell);
    *result = state_json();
    return true;
}

static bool ep_game_rune_scout_briar_gate(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_scout_briar_gate);
    *result = state_json();
    return true;
}

static bool ep_game_rune_scout_moonwell(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_scout_moonwell);
    *result = state_json();
    return true;
}

static bool ep_game_rune_discover_ashen_cairn(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_discover_ashen_cairn);
    *result = state_json();
    return true;
}

static bool ep_game_rune_discover_starfall_grotto(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    action_click(game_rune_discover_starfall_grotto);
    *result = state_json();
    return true;
}

static bool ep_game_audio_status(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    const GameAudioStatus status = game_audio_status();
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "implemented", status.implemented);
    cJSON_AddBoolToObject(root, "initialized", status.initialized);
    cJSON_AddBoolToObject(root, "device_enabled", status.device_enabled);
    cJSON_AddStringToObject(root, "backend", status.backend ? status.backend : "unknown");
    cJSON_AddNumberToObject(root, "total_play_count", status.total_play_count);
    cJSON *cues = cJSON_AddObjectToObject(root, "cue_play_counts");
    for (int i = 0; i < GAME_AUDIO_CUE_COUNT; ++i) {
        cJSON_AddNumberToObject(cues, game_audio_cue_name((GameAudioCue)i), status.cue_play_count[i]);
    }
    *result = root;
    return true;
}

static bool ep_game_capture_framebuffer(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
#ifdef NT_PLATFORM_WEB
    (void)params;
    (void)result;
    (void)error;
    (void)error_cap;
    return false;
#else
    const cJSON *output = cJSON_GetObjectItemCaseSensitive(params, "output");
    if (!cJSON_IsString(output) || output->valuestring[0] == '\0' || strlen(output->valuestring) >= sizeof(s_pending_capture_path)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "valid output path is required");
        return false;
    }
    (void)snprintf(s_pending_capture_path, sizeof(s_pending_capture_path), "%s", output->valuestring);
    *result = cJSON_CreateObject();
    cJSON_AddStringToObject(*result, "output", s_pending_capture_path);
    cJSON_AddBoolToObject(*result, "pending", true);
    return true;
#endif
}

static void register_ui_devapi(float w, float h) {
    nt_devapi_set_view(w, h, w, h);
    nt_devapi_clear_ui_elements();
    (void)nt_devapi_register_ui_node("root", "", "screen", "Splash Rods", g_game_state.fishing_objective, 0.0F, 0.0F, w, h, true, true);
    (void)nt_devapi_register_ui_node("seed.preview", "root", "panel", "Compatibility Preview", g_game_state.test_label_text, w * 0.05F, h * 0.16F, w * 0.58F, h * 0.52F, true, true);
    (void)nt_devapi_register_ui_node("seed.cycle", "root", "button", "Compatibility Primary", g_game_state.test_button_text, s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, true);
    (void)nt_devapi_register_ui_node("scene.dock", "root", "scene", "Toy Dock", "3D fishing stage: dock, water, avatar, rod", w * 0.08F, h * 0.28F, w * 0.55F, h * 0.55F, true, true);
    (void)nt_devapi_register_ui_node("scene.bobber", "scene.dock", "target", "Bobber", game_state_fishing_phase_name(g_game_state.fishing_phase), w * 0.63F, h * 0.50F, 70.0F, 70.0F, true, true);
    (void)nt_devapi_register_ui_node("status.coins", "root", "meter", "Coins", g_game_state.fishing_next_unlock, w * 0.28F, 18.0F, w * 0.12F, 28.0F, true, true);
    (void)nt_devapi_register_ui_node("status.level", "root", "meter", "Level", g_game_state.fishing_objective, w * 0.45F, 18.0F, w * 0.12F, 28.0F, true, true);
    (void)nt_devapi_register_ui_node("status.backpack", "root", "meter", "Backpack", "Fish held before selling", w * 0.62F, 18.0F, w * 0.12F, 28.0F, true, true);
    (void)nt_devapi_register_ui_node("status.index", "root", "meter", "Index", "Collection progress", w * 0.78F, 18.0F, w * 0.12F, 28.0F, true, true);
    (void)nt_devapi_register_ui_node("status.reel_meter", "root", "meter", "Reel Meter", "Tap reel until full", w * 0.35F, h - 148.0F, w * 0.30F, 18.0F, true, game_fishing_can_reel(&g_game_state));
    (void)nt_devapi_register_ui_node("reward.last", "root", "status", "Reward", g_game_state.fishing_last_reward, w * 0.62F, h * 0.62F, w * 0.30F, 28.0F, true, true);
    (void)nt_devapi_register_ui_node("fish.last", "root", "status", "Last Fish", g_game_state.fishing_last_fish, w * 0.62F, h * 0.56F, w * 0.30F, 28.0F, true, true);
    (void)nt_devapi_register_ui_node("action.primary", "root", "button", "Cast Reel Primary", fishing_primary_label(), s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, true);
    (void)nt_devapi_register_ui_node("action.cast", "root", "button", "Cast", "Throw bobber to sparkle water", s_strike_box.x, s_strike_box.y, s_strike_box.w, s_strike_box.h, true, game_fishing_can_cast(&g_game_state));
    (void)nt_devapi_register_ui_node("action.reel", "root", "button", "Reel", "Fill the reel meter", s_spark_box.x, s_spark_box.y, s_spark_box.w, s_spark_box.h, true, game_fishing_can_reel(&g_game_state));
    (void)nt_devapi_register_ui_node("action.sell", "root", "button", "Sell", "Convert backpack fish to coins", s_rest_box.x, s_rest_box.y, s_rest_box.w, s_rest_box.h, true, game_fishing_can_sell(&g_game_state));
    (void)nt_devapi_register_ui_node("upgrade.better_line", "root", "button", "Better Line", g_game_state.fishing_next_unlock, s_upgrade_box.x, s_upgrade_box.y, s_upgrade_box.w, s_upgrade_box.h, true, game_fishing_can_buy_better_line(&g_game_state));
    return;

    (void)nt_devapi_register_ui_node("root", "", "screen", "Rune Marches", g_game_state.rune_objective, 0.0F, 0.0F, w, h, true, true);
    (void)nt_devapi_register_ui_node("seed.preview", "root", "panel", "Compatibility Preview", g_game_state.test_label_text, w * 0.05F, h * 0.16F, w * 0.58F, h * 0.52F, true, true);
    (void)nt_devapi_register_ui_node("seed.cycle", "root", "button", "Compatibility Scout", g_game_state.test_button_text, s_cycle_box.x, s_cycle_box.y, s_cycle_box.w, s_cycle_box.h, true, g_game_state.rune_encounter == GAME_STATE_RUNE_ENCOUNTER_NONE);
    (void)nt_devapi_register_ui_node("status.hp", "root", "meter", "HP", "", w * 0.04F, h * 0.03F, w * 0.18F, h * 0.03F, true, true);
    (void)nt_devapi_register_ui_node("status.mana", "root", "meter", "Mana", "", w * 0.04F, h * 0.07F, w * 0.18F, h * 0.02F, true, true);
    (void)nt_devapi_register_ui_node("status.silver", "root", "meter", "Silver", "", w * 0.26F, h * 0.05F, w * 0.20F, h * 0.03F, true, true);
    (void)nt_devapi_register_ui_node("reward.last", "root", "status", "Reward", g_game_state.rune_reward_text, w * 0.70F, h * 0.49F, w * 0.22F, h * 0.04F, g_game_state.rune_reward_text[0] != '\0', true);
    (void)nt_devapi_register_ui_node("map.miregate", "seed.preview", "location", "Miregate", "Safe hamlet and rest point", w * 0.15F, h * 0.40F, 60.0F, 60.0F, true, true);
    (void)nt_devapi_register_ui_node("map.wispfen_road", "seed.preview", "location", "Wispfen Road", g_game_state.rune_combat_log, w * 0.33F, h * 0.40F, 64.0F, 64.0F, true, true);
    (void)nt_devapi_register_ui_node("map.old_bell_tower", "seed.preview", "location", "Old Bell Tower", g_game_state.rune_tower_unlocked ? "Unlocked" : "Locked by a sleeping road stone", w * 0.48F, h * 0.28F, 68.0F, 68.0F, true, g_game_state.rune_tower_unlocked);
    (void)nt_devapi_register_ui_node("map.reedmere_crossing", "seed.preview", "location", "Reedmere Crossing", g_game_state.rune_east_road_unlocked ? "Next road open" : "Listen at the tower first", w * 0.53F, h * 0.46F, 68.0F, 58.0F, true, g_game_state.rune_east_road_unlocked);
    (void)nt_devapi_register_ui_node("map.greenfen_causeway", "seed.preview", "location", "Greenfen Causeway", g_game_state.rune_causeway_unlocked ? "Next region route" : "Needs spirit favor", w * 0.58F, h * 0.27F, 58.0F, 52.0F, true, g_game_state.rune_causeway_unlocked);
    (void)nt_devapi_register_ui_node("map.briar_gate", "seed.preview", "location", "Briar Gate", "Main road beyond Greenfen", w * 0.44F, h * 0.20F, 58.0F, 52.0F, true, g_game_state.rune_spell_level >= 2 || g_game_state.rune_briar_gate_unlocked);
    (void)nt_devapi_register_ui_node("map.moonwell", "seed.preview", "location", "Moonwell", "Optional oath route", w * 0.56F, h * 0.18F, 58.0F, 52.0F, true, g_game_state.rune_spell_level >= 2 || g_game_state.rune_moonwell_unlocked);
    (void)nt_devapi_register_ui_node("map.ashen_cairn", "seed.preview", "location", "Ashen Cairn", "Main-road dungeon hook", w * 0.50F, h * 0.15F, 54.0F, 46.0F, true, g_game_state.rune_briar_gate_safety > 0 || g_game_state.rune_ashen_cairn_unlocked);
    (void)nt_devapi_register_ui_node("map.starfall_grotto", "seed.preview", "location", "Starfall Grotto", "Moonwell magic cave hook", w * 0.58F, h * 0.24F, 54.0F, 46.0F, true, g_game_state.rune_moonwell_safety > 0 || g_game_state.rune_starfall_grotto_unlocked);
    (void)nt_devapi_register_ui_node("action.scout_road", "root", "button", "Scout Road", g_game_state.rune_objective, s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, g_game_state.rune_encounter == GAME_STATE_RUNE_ENCOUNTER_NONE);
    (void)nt_devapi_register_ui_node("action.scout_east", "root", "button", "Scout Reedmere", "Open the next road", s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, g_game_state.rune_east_road_unlocked && g_game_state.rune_encounter == GAME_STATE_RUNE_ENCOUNTER_NONE);
    (void)nt_devapi_register_ui_node("action.scout_greenfen", "root", "button", "Scout Greenfen", "First beat beyond the causeway", s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, game_rune_can_scout_greenfen(&g_game_state));
    (void)nt_devapi_register_ui_node("combat.strike", "root", "button", "Strike", "Deal 3 damage", s_strike_box.x, s_strike_box.y, s_strike_box.w, s_strike_box.h, true, g_game_state.rune_encounter != GAME_STATE_RUNE_ENCOUNTER_NONE);
    (void)nt_devapi_register_ui_node("combat.spark", "root", "button", "Spark", "Spend 3 mana for magic damage", s_spark_box.x, s_spark_box.y, s_spark_box.w, s_spark_box.h, true, g_game_state.rune_encounter != GAME_STATE_RUNE_ENCOUNTER_NONE && g_game_state.rune_mana >= 3);
    (void)nt_devapi_register_ui_node("combat.guard", "root", "button", "Guard", "Reduce next hit", s_guard_box.x, s_guard_box.y, s_guard_box.w, s_guard_box.h, true, g_game_state.rune_encounter != GAME_STATE_RUNE_ENCOUNTER_NONE);
    (void)nt_devapi_register_ui_node("combat.retreat", "root", "button", "Retreat", "Return to Miregate", s_retreat_box.x, s_retreat_box.y, s_retreat_box.w, s_retreat_box.h, true, g_game_state.rune_encounter != GAME_STATE_RUNE_ENCOUNTER_NONE);
    (void)nt_devapi_register_ui_node("action.rest", "root", "button", "Rest", "Recover HP and mana", s_rest_box.x, s_rest_box.y, s_rest_box.w, s_rest_box.h, true, true);
    (void)nt_devapi_register_ui_node("upgrade.spark_ward_1", "root", "button", "Spark Ward I", "Cost: 12 silver and 1 spark", s_upgrade_box.x, s_upgrade_box.y, s_upgrade_box.w, s_upgrade_box.h, true, g_game_state.rune_spell_level == 0 && g_game_state.rune_silver >= 12 && g_game_state.rune_sparks >= 1);
    (void)nt_devapi_register_ui_node("upgrade.spark_ward_2", "root", "button", "Spark Ward II", "Cost: 1 rune lore", s_upgrade_box.x, s_upgrade_box.y, s_upgrade_box.w, s_upgrade_box.h, true, game_rune_can_study_rune_lore(&g_game_state));
    (void)nt_devapi_register_ui_node("quest.bell_rope.silver", "root", "button", "Take Silver", "Claim +6 silver", s_rest_box.x, s_rest_box.y, s_rest_box.w, s_rest_box.h, true, g_game_state.rune_side_quest_step == 2 && g_game_state.rune_bell_rope_charm);
    (void)nt_devapi_register_ui_node("quest.bell_rope.kindness", "root", "button", "Give Rope", "Return charm for kindness", s_upgrade_box.x, s_upgrade_box.y, s_upgrade_box.w, s_upgrade_box.h, true, g_game_state.rune_side_quest_step == 2 && g_game_state.rune_bell_rope_charm);
    (void)nt_devapi_register_ui_node("action.inspect_tower", "root", "button", "Inspect Tower", "Listen for the next road", s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, g_game_state.rune_tower_unlocked && !g_game_state.rune_tower_inspected);
    (void)nt_devapi_register_ui_node("action.light_moss_shrine", "root", "button", "Light Moss Shrine", "Spend kindness for favor", s_upgrade_box.x, s_upgrade_box.y, s_upgrade_box.w, s_upgrade_box.h, true, game_rune_can_light_moss_shrine(&g_game_state));
    (void)nt_devapi_register_ui_node("action.open_causeway", "root", "button", "Open Causeway", "Spend favor to open Greenfen route", s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, game_rune_can_open_causeway(&g_game_state));
    (void)nt_devapi_register_ui_node("action.choose_briar_gate", "root", "button", "Briar Gate", "Mark the main route", s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, game_rune_can_choose_next_route(&g_game_state));
    (void)nt_devapi_register_ui_node("action.choose_moonwell", "root", "button", "Moonwell", "Mark the side oath route", s_upgrade_box.x, s_upgrade_box.y, s_upgrade_box.w, s_upgrade_box.h, true, game_rune_can_choose_next_route(&g_game_state));
    (void)nt_devapi_register_ui_node("action.scout_briar_gate", "root", "button", "Scout Briar Gate", "Clear the first gate threat", s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, game_rune_can_scout_briar_gate(&g_game_state));
    (void)nt_devapi_register_ui_node("action.scout_moonwell", "root", "button", "Moonwell Trial", "Calm the first well sentinel", s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, game_rune_can_scout_moonwell(&g_game_state));
    (void)nt_devapi_register_ui_node("action.discover_ashen_cairn", "root", "button", "Map Ashen Cairn", "Reveal the main-road dungeon hook", s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, game_rune_can_discover_ashen_cairn(&g_game_state));
    (void)nt_devapi_register_ui_node("action.discover_starfall_grotto", "root", "button", "Map Starfall Grotto", "Reveal the Moonwell cave hook", s_scout_box.x, s_scout_box.y, s_scout_box.w, s_scout_box.h, true, game_rune_can_discover_starfall_grotto(&g_game_state));
}

static void register_game_endpoints(void) {
    nt_devapi_register_builtins();
    game_state_register_devapi();
    nt_devapi_register("game.state", ep_game_state, NULL);
    nt_devapi_register("game.reset_playtest", ep_game_reset_playtest, NULL);
    nt_devapi_register("game.action.cycle", ep_game_action_cycle, NULL);
    nt_devapi_register("game.fishing.primary", ep_game_fishing_primary, NULL);
    nt_devapi_register("game.fishing.cast", ep_game_fishing_cast, NULL);
    nt_devapi_register("game.fishing.reel", ep_game_fishing_reel, NULL);
    nt_devapi_register("game.fishing.sell_all", ep_game_fishing_sell_all, NULL);
    nt_devapi_register("game.fishing.upgrade.better_line", ep_game_fishing_upgrade_better_line, NULL);
    nt_devapi_register("game.rune.primary", ep_game_rune_primary_action, NULL);
    nt_devapi_register("game.rune.scout", ep_game_rune_scout, NULL);
    nt_devapi_register("game.rune.scout_east", ep_game_rune_scout_east, NULL);
    nt_devapi_register("game.rune.scout_greenfen", ep_game_rune_scout_greenfen, NULL);
    nt_devapi_register("game.rune.strike", ep_game_rune_strike, NULL);
    nt_devapi_register("game.rune.spark", ep_game_rune_spark, NULL);
    nt_devapi_register("game.rune.guard", ep_game_rune_guard, NULL);
    nt_devapi_register("game.rune.retreat", ep_game_rune_retreat, NULL);
    nt_devapi_register("game.rune.rest", ep_game_rune_rest, NULL);
    nt_devapi_register("game.rune.upgrade.spark_ward", ep_game_rune_buy_spark_ward, NULL);
    nt_devapi_register("game.rune.upgrade.spark_ward_2", ep_game_rune_study_rune_lore, NULL);
    nt_devapi_register("game.rune.quest.bell_rope.silver", ep_game_rune_bell_rope_silver, NULL);
    nt_devapi_register("game.rune.quest.bell_rope.kindness", ep_game_rune_bell_rope_kindness, NULL);
    nt_devapi_register("game.rune.inspect_tower", ep_game_rune_inspect_tower, NULL);
    nt_devapi_register("game.rune.light_moss_shrine", ep_game_rune_light_moss_shrine, NULL);
    nt_devapi_register("game.rune.open_causeway", ep_game_rune_open_causeway, NULL);
    nt_devapi_register("game.rune.choose_briar_gate", ep_game_rune_choose_briar_gate, NULL);
    nt_devapi_register("game.rune.choose_moonwell", ep_game_rune_choose_moonwell, NULL);
    nt_devapi_register("game.rune.scout_briar_gate", ep_game_rune_scout_briar_gate, NULL);
    nt_devapi_register("game.rune.scout_moonwell", ep_game_rune_scout_moonwell, NULL);
    nt_devapi_register("game.rune.discover_ashen_cairn", ep_game_rune_discover_ashen_cairn, NULL);
    nt_devapi_register("game.rune.discover_starfall_grotto", ep_game_rune_discover_starfall_grotto, NULL);
    nt_devapi_register("game.rune.telemetry", ep_game_rune_telemetry, NULL);
    nt_devapi_register("game.rune.telemetry.reset", ep_game_rune_telemetry_reset, NULL);
    nt_devapi_register("game.audio.status", ep_game_audio_status, NULL);
    nt_devapi_register("game.capture.framebuffer", ep_game_capture_framebuffer, NULL);
}
#endif

static void frame(void) {
    nt_window_poll();
    nt_input_poll();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_set_frame((uint64_t)g_nt_app.frame);
        nt_devapi_net_poll();
        nt_devapi_apply_pending();
    }
#endif

    const float w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    layout(w, h);
    handle_input();
    game_audio_update();
    fishing_mesh_system_step();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        register_ui_devapi(w, h);
    }
#endif

    draw_game(w, h);
    autosave_if_dirty();

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Rune Marches";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    game_state_init();
    parse_args(argc, argv);
    load_default_save_if_available();
    game_audio_init();
    game_audio_set_device_enabled(!s_devapi_enabled);
    game_audio_set_volume(g_game_state.settings_master_volume, g_game_state.settings_sfx_volume);

    g_nt_window.title = "Rune Marches";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    nt_gfx_init(&gfx_desc);
    nt_gfx_register_global_block("Globals", 0);
    fishing_mesh_system_init();
    rune_textures_init();
    nt_shape_renderer_init();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_init();
        register_game_endpoints();
        if (!nt_devapi_net_start(s_devapi_port)) {
            (void)fprintf(stderr, "Failed to start DevAPI on port %u\n", (unsigned)s_devapi_port);
        }
    }
#endif

#ifdef NT_PLATFORM_WEB
    nt_platform_web_loading_complete();
#endif

    g_nt_app.target_dt = 1.0F / 60.0F;
    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_net_stop();
        nt_devapi_shutdown();
    }
#endif
    game_audio_shutdown();
    nt_shape_renderer_shutdown();
    rune_textures_shutdown();
    fishing_mesh_system_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif

    return 0;
}
