#include "app/nt_app.h"
/* Temporary debug renderer debt: this first playable uses nt_shape_renderer as
 * an integration layer until accepted project-local models/materials land. */
#include "core/nt_core.h"
#include "core/nt_platform.h"
#if NT_DEVAPI_ENABLED
#include "devapi/nt_devapi.h"
#include "devapi/nt_devapi_net.h"
#include "game_devapi_ui.h"
#endif
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "math/nt_math.h"
#include "renderers/nt_shape_renderer.h"
#include "window/nt_window.h"

#include "blockfell_authored_assets.h"

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#else
#include <glad/gl.h>
#endif

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_DEVAPI_PORT_DEFAULT 9123
#define RUNE_TARGET 3
#define RUNE_SITE_COUNT 3
#define ENEMY_COUNT 4
#define PLAYER_MAX_HP 6
#define WORLD_HALF 9.0F
#define PLAYER_SPEED 4.4F
#define ENEMY_SPEED 1.35F
#define INTERACT_RADIUS 1.38F
#define ATTACK_RADIUS 1.65F
#define MATERIAL_TEX_SIZE 64
#define ASSET_MAX_VERTICES 32
#define ASSET_MAX_INDICES 64

typedef struct UiBox {
    float x;
    float y;
    float w;
    float h;
} UiBox;

typedef struct RuneSite {
    float x;
    float z;
    bool claimed;
    bool requires_combat;
    bool requires_chest;
} RuneSite;

typedef struct Enemy {
    float x;
    float z;
    float home_x;
    float home_z;
    int hp;
    bool alive;
    float attack_cd;
    float hit_flash;
} Enemy;

typedef struct MaterialVertex {
    float pos[3];
    float uv[2];
    float color[4];
    float normal[3];
} MaterialVertex;

typedef enum MaterialKind {
    MAT_GRASS = 0,
    MAT_PATH,
    MAT_STONE,
    MAT_WOOD,
    MAT_CLOTH,
    MAT_RUNE,
    MAT_COUNT,
} MaterialKind;

typedef enum ObjectiveStage {
    OBJ_FIRST_RUNE = 0,
    OBJ_CLEAR_CAMP,
    OBJ_OPEN_CHEST,
    OBJ_COMBAT_RUNE,
    OBJ_LOOT_RUNE,
    OBJ_ENTER_GATE,
    OBJ_DONE,
} ObjectiveStage;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = GAME_DEVAPI_PORT_DEFAULT;
static int s_window_width = 960;
static int s_window_height = 540;
static float s_player_x = -6.2F;
static float s_player_z = -5.4F;
static float s_player_facing = 0.15F;
static int s_player_hp = PLAYER_MAX_HP;
static float s_player_hit_flash;
static float s_attack_cd;
static float s_slash_timer;
static bool s_chest_open;
static UiBox s_action_box;
static UiBox s_attack_box;

static RuneSite s_runes[RUNE_SITE_COUNT];
static Enemy s_enemies[ENEMY_COUNT];

static nt_shader_t s_mat_vs;
static nt_shader_t s_mat_fs;
static nt_pipeline_t s_mat_pipeline;
static nt_buffer_t s_mat_vbo;
static nt_buffer_t s_mat_ibo;
static nt_texture_t s_mat_textures[MAT_COUNT];
static bool s_material_pass_ready;
static nt_shader_t s_asset_vs;
static nt_shader_t s_asset_fs;
static nt_pipeline_t s_asset_pipeline;
static nt_buffer_t s_asset_vbo;
static nt_buffer_t s_asset_ibo;
static bool s_asset_pass_ready;

static const float s_chest_x = 3.7F;
static const float s_chest_z = -2.6F;
static const float s_gate_x = 6.2F;
static const float s_gate_z = 3.7F;
static const float s_camp_x = 1.8F;
static const float s_camp_z = 2.5F;

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static char s_pending_capture_path[260];
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

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static float dist2(float ax, float az, float bx, float bz) {
    const float dx = ax - bx;
    const float dz = az - bz;
    return dx * dx + dz * dz;
}

static bool near_point(float x, float z, float radius) {
    return dist2(s_player_x, s_player_z, x, z) <= radius * radius;
}

static int alive_enemy_count(void) {
    int count = 0;
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        if (s_enemies[i].alive) {
            count += 1;
        }
    }
    return count;
}

static bool combat_cleared(void) {
    return alive_enemy_count() == 0;
}

static bool is_near_chest(void) {
    return near_point(s_chest_x, s_chest_z, INTERACT_RADIUS);
}

static bool is_near_gate(void) {
    return near_point(s_gate_x, s_gate_z, 3.1F);
}

static int nearest_rune_index(void) {
    int best = -1;
    float best_d = INTERACT_RADIUS * INTERACT_RADIUS;
    for (int i = 0; i < RUNE_SITE_COUNT; ++i) {
        const float d = dist2(s_player_x, s_player_z, s_runes[i].x, s_runes[i].z);
        if (d <= best_d) {
            best = i;
            best_d = d;
        }
    }
    return best;
}

static bool rune_unlocked(const RuneSite *site) {
    return (!site->requires_combat || combat_cleared()) && (!site->requires_chest || s_chest_open);
}

static bool action_ready(void) {
    const int rune_index = nearest_rune_index();
    if (rune_index >= 0 && !s_runes[rune_index].claimed && rune_unlocked(&s_runes[rune_index])) {
        return true;
    }
    return !s_chest_open && combat_cleared() && is_near_chest();
}

static ObjectiveStage objective_stage(void) {
    if (g_game_state.tutorial_done && is_near_gate()) {
        return OBJ_DONE;
    }
    if (g_game_state.tutorial_done) {
        return OBJ_ENTER_GATE;
    }
    if (!s_runes[0].claimed) {
        return OBJ_FIRST_RUNE;
    }
    if (!combat_cleared()) {
        return OBJ_CLEAR_CAMP;
    }
    if (!s_chest_open) {
        return OBJ_OPEN_CHEST;
    }
    if (!s_runes[1].claimed) {
        return OBJ_COMBAT_RUNE;
    }
    if (!s_runes[2].claimed) {
        return OBJ_LOOT_RUNE;
    }
    return OBJ_ENTER_GATE;
}

static void objective_target(float *out_x, float *out_z) {
    const ObjectiveStage stage = objective_stage();
    if (stage == OBJ_FIRST_RUNE) {
        *out_x = s_runes[0].x;
        *out_z = s_runes[0].z;
    } else if (stage == OBJ_CLEAR_CAMP) {
        *out_x = s_camp_x;
        *out_z = s_camp_z;
    } else if (stage == OBJ_OPEN_CHEST) {
        *out_x = s_chest_x;
        *out_z = s_chest_z;
    } else if (stage == OBJ_COMBAT_RUNE) {
        *out_x = s_runes[1].x;
        *out_z = s_runes[1].z;
    } else if (stage == OBJ_LOOT_RUNE) {
        *out_x = s_runes[2].x;
        *out_z = s_runes[2].z;
    } else {
        *out_x = s_gate_x;
        *out_z = s_gate_z;
    }
}

static bool objective_step_complete(int step) {
    if (step == 0) {
        return s_runes[0].claimed;
    }
    if (step == 1) {
        return combat_cleared();
    }
    if (step == 2) {
        return s_chest_open;
    }
    if (step == 3) {
        return s_runes[1].claimed;
    }
    if (step == 4) {
        return s_runes[2].claimed;
    }
    return g_game_state.tutorial_done;
}

static void set_text(char *target, size_t cap, const char *text) {
    if (cap == 0) {
        return;
    }
    (void)snprintf(target, cap, "%s", text);
    target[cap - 1] = '\0';
}

static void sync_labels(void) {
    char label[GAME_STATE_STRING_MAX];
    if (s_player_hp <= 0) {
        set_text(g_game_state.test_label_text, sizeof(g_game_state.test_label_text), "Downed: press R");
        set_text(g_game_state.test_button_text, sizeof(g_game_state.test_button_text), "Reset");
        return;
    }
    if (g_game_state.tutorial_done) {
        set_text(g_game_state.test_label_text, sizeof(g_game_state.test_label_text), "Gate open: cross the pass");
        set_text(g_game_state.test_button_text, sizeof(g_game_state.test_button_text), "Enter pass");
        return;
    }
    if (alive_enemy_count() > 0 && g_game_state.wallet_soft >= 1) {
        (void)snprintf(label, sizeof(label), "Clear camp: %d enemies", alive_enemy_count());
        set_text(g_game_state.test_label_text, sizeof(g_game_state.test_label_text), label);
        set_text(g_game_state.test_button_text, sizeof(g_game_state.test_button_text), "Strike");
        return;
    }
    if (!s_chest_open && combat_cleared()) {
        set_text(g_game_state.test_label_text, sizeof(g_game_state.test_label_text), "Open ruin chest");
        set_text(g_game_state.test_button_text, sizeof(g_game_state.test_button_text), "Open chest");
        return;
    }
    (void)snprintf(label, sizeof(label), "Runes: %d/%d", g_game_state.wallet_soft, RUNE_TARGET);
    set_text(g_game_state.test_label_text, sizeof(g_game_state.test_label_text), label);
    set_text(g_game_state.test_button_text, sizeof(g_game_state.test_button_text), "Claim rune");
}

static void refresh_gate(void) {
    g_game_state.tutorial_done = g_game_state.wallet_soft >= RUNE_TARGET;
    sync_labels();
    game_state_mark_dirty();
}

static void reset_slice(void) {
    game_state_init_defaults(&g_game_state);
    g_game_state.wallet_soft = 0;
    g_game_state.wallet_hard = 0;
    g_game_state.tutorial_done = false;
    s_player_x = -6.2F;
    s_player_z = -5.4F;
    s_player_facing = 0.15F;
    s_player_hp = PLAYER_MAX_HP;
    s_player_hit_flash = 0.0F;
    s_attack_cd = 0.0F;
    s_slash_timer = 0.0F;
    s_chest_open = false;
    s_runes[0] = (RuneSite){.x = -4.9F, .z = -4.0F, .claimed = false, .requires_combat = false, .requires_chest = false};
    s_runes[1] = (RuneSite){.x = 1.2F, .z = 3.6F, .claimed = false, .requires_combat = true, .requires_chest = false};
    s_runes[2] = (RuneSite){.x = 5.5F, .z = -1.4F, .claimed = false, .requires_combat = false, .requires_chest = true};
    s_enemies[0] = (Enemy){.x = 0.6F, .z = 2.2F, .home_x = 0.6F, .home_z = 2.2F, .hp = 2, .alive = true};
    s_enemies[1] = (Enemy){.x = 2.2F, .z = 2.0F, .home_x = 2.2F, .home_z = 2.0F, .hp = 2, .alive = true};
    s_enemies[2] = (Enemy){.x = 1.8F, .z = 3.6F, .home_x = 1.8F, .home_z = 3.6F, .hp = 3, .alive = true};
    s_enemies[3] = (Enemy){.x = 3.1F, .z = 2.9F, .home_x = 3.1F, .home_z = 2.9F, .hp = 2, .alive = true};
    sync_labels();
    game_state_mark_dirty();
}

static bool claim_rune(void) {
    const int rune_index = nearest_rune_index();
    if (rune_index < 0 || s_runes[rune_index].claimed || !rune_unlocked(&s_runes[rune_index])) {
        return false;
    }
    s_runes[rune_index].claimed = true;
    if (g_game_state.wallet_soft < RUNE_TARGET) {
        g_game_state.wallet_soft += 1;
    }
    refresh_gate();
    return true;
}

static bool open_chest(void) {
    if (s_chest_open || !combat_cleared() || !is_near_chest()) {
        return false;
    }
    s_chest_open = true;
    g_game_state.wallet_hard += 25;
    refresh_gate();
    return true;
}

static bool interact(void) {
    if (claim_rune()) {
        return true;
    }
    return open_chest();
}

static bool attack(void) {
    if (s_attack_cd > 0.0F || s_player_hp <= 0) {
        return false;
    }
    s_attack_cd = 0.28F;
    s_slash_timer = 0.18F;
    bool hit = false;
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy *enemy = &s_enemies[i];
        if (!enemy->alive || dist2(s_player_x, s_player_z, enemy->x, enemy->z) > ATTACK_RADIUS * ATTACK_RADIUS) {
            continue;
        }
        enemy->hp -= 1;
        enemy->hit_flash = 0.20F;
        hit = true;
        if (enemy->hp <= 0) {
            enemy->alive = false;
            g_game_state.wallet_hard += 5;
        }
    }
    refresh_gate();
    return hit;
}

static void parse_args(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--devapi") == 0) {
            s_devapi_enabled = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                s_devapi_port = (uint16_t)strtoul(argv[++i], NULL, 10);
            }
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

static void layout(float w) {
    const float button_w = w < 620.0F ? w * 0.36F : 180.0F;
    s_action_box = (UiBox){.x = w * 0.5F - button_w - 8.0F, .y = 24.0F, .w = button_w, .h = 46.0F};
    s_attack_box = (UiBox){.x = w * 0.5F + 8.0F, .y = 24.0F, .w = button_w, .h = 46.0F};
}

static const char *s_mat_vs_src = "precision mediump float;\n"
                                  "layout(location = 0) in vec3 a_position;\n"
                                  "layout(location = 1) in vec2 a_uv;\n"
                                  "layout(location = 2) in vec4 a_color;\n"
                                  "layout(location = 3) in vec3 a_normal;\n"
                                  "uniform mat4 u_vp;\n"
                                  "out vec2 v_uv;\n"
                                  "out vec4 v_color;\n"
                                  "out vec3 v_normal;\n"
                                  "out vec3 v_world;\n"
                                  "void main() {\n"
                                  "    v_uv = a_uv;\n"
                                  "    v_color = a_color;\n"
                                  "    v_normal = normalize(a_normal);\n"
                                  "    v_world = a_position;\n"
                                  "    gl_Position = u_vp * vec4(a_position, 1.0);\n"
                                  "}\n";

static const char *s_mat_fs_src = "precision mediump float;\n"
                                  "uniform sampler2D u_tex;\n"
                                  "in vec2 v_uv;\n"
                                  "in vec4 v_color;\n"
                                  "in vec3 v_normal;\n"
                                  "in vec3 v_world;\n"
                                  "out vec4 frag_color;\n"
                                  "void main() {\n"
                                  "    vec4 tex = texture(u_tex, v_uv);\n"
                                  "    vec3 n = normalize(v_normal);\n"
                                  "    vec3 key = normalize(vec3(-0.56, 0.76, -0.34));\n"
                                  "    vec3 fill = normalize(vec3(0.36, 0.42, 0.68));\n"
                                  "    float key_l = max(dot(n, key), 0.0);\n"
                                  "    float fill_l = max(dot(n, fill), 0.0);\n"
                                  "    float rim = pow(1.0 - abs(n.y), 2.0) * 0.16;\n"
                                  "    float height = clamp(v_world.y * 0.055, 0.0, 0.16);\n"
                                  "    vec3 lit = tex.rgb * v_color.rgb * (0.50 + key_l * 0.42 + fill_l * 0.13 + rim + height);\n"
                                  "    vec3 sky = vec3(0.50, 0.70, 0.86);\n"
                                  "    float fog = clamp((v_world.z + 8.0) * 0.020 + max(v_world.y - 1.0, 0.0) * 0.030, 0.0, 0.28);\n"
                                  "    frag_color = vec4(mix(lit, sky, fog), tex.a * v_color.a);\n"
                                  "}\n";

static const char *s_asset_vs_src = "precision mediump float;\n"
                                    "layout(location = 0) in vec3 a_position;\n"
                                    "layout(location = 1) in vec3 a_normal;\n"
                                    "layout(location = 2) in vec4 a_color;\n"
                                    "uniform mat4 u_vp;\n"
                                    "out vec3 v_normal;\n"
                                    "out vec4 v_color;\n"
                                    "out vec3 v_world;\n"
                                    "void main() {\n"
                                    "    v_normal = normalize(a_normal);\n"
                                    "    v_color = a_color;\n"
                                    "    v_world = a_position;\n"
                                    "    gl_Position = u_vp * vec4(a_position, 1.0);\n"
                                    "}\n";

static const char *s_asset_fs_src = "precision mediump float;\n"
                                    "in vec3 v_normal;\n"
                                    "in vec4 v_color;\n"
                                    "in vec3 v_world;\n"
                                    "out vec4 frag_color;\n"
                                    "void main() {\n"
                                    "    vec3 n = normalize(v_normal);\n"
                                    "    vec3 key = normalize(vec3(-0.52, 0.80, -0.30));\n"
                                    "    vec3 fill = normalize(vec3(0.48, 0.32, 0.72));\n"
                                    "    float l = 0.46 + max(dot(n, key), 0.0) * 0.44 + max(dot(n, fill), 0.0) * 0.14;\n"
                                    "    float rim = pow(1.0 - abs(n.y), 2.0) * 0.18;\n"
                                    "    float height = clamp(v_world.y * 0.06, 0.0, 0.18);\n"
                                    "    vec3 color = v_color.rgb * (l + rim + height);\n"
                                    "    frag_color = vec4(color, v_color.a);\n"
                                    "}\n";

static uint8_t u8_clamp(int v) {
    if (v < 0) {
        return 0U;
    }
    if (v > 255) {
        return 255U;
    }
    return (uint8_t)v;
}

static uint32_t tex_noise(int x, int y, int seed) {
    uint32_t v = (uint32_t)(x * 73856093) ^ (uint32_t)(y * 19349663) ^ (uint32_t)(seed * 83492791);
    v ^= v >> 13U;
    v *= 1274126177U;
    return v ^ (v >> 16U);
}

static void fill_texel(uint8_t *pixels, int x, int y, int r, int g, int b, int a) {
    const int i = (y * MATERIAL_TEX_SIZE + x) * 4;
    pixels[i + 0] = u8_clamp(r);
    pixels[i + 1] = u8_clamp(g);
    pixels[i + 2] = u8_clamp(b);
    pixels[i + 3] = u8_clamp(a);
}

static void build_material_texture(MaterialKind kind, uint8_t pixels[MATERIAL_TEX_SIZE * MATERIAL_TEX_SIZE * 4]) {
    for (int y = 0; y < MATERIAL_TEX_SIZE; ++y) {
        for (int x = 0; x < MATERIAL_TEX_SIZE; ++x) {
            const int n = (int)(tex_noise(x, y, (int)kind + 7) & 31U) - 15;
            if (kind == MAT_GRASS) {
                const bool blade = ((x + y * 3) % 11) == 0 || ((x * 2 + y) % 17) == 0 || ((x + y) % 23) == 0;
                const bool clover = (tex_noise(x / 4, y / 4, 91) & 31U) == 0U;
                fill_texel(pixels, x, y, clover ? 76 + n : 50 + n, blade ? 130 + n : 88 + n, blade ? 54 + n : 42 + n, 255);
            } else if (kind == MAT_PATH) {
                const bool pebble = (tex_noise(x / 2, y / 2, 22) & 7U) == 0U;
                const bool rut = abs((x + y / 2) % 19 - 9) < 2;
                fill_texel(pixels, x, y, pebble ? 168 + n : 132 + n - (rut ? 18 : 0), pebble ? 130 + n : 102 + n - (rut ? 12 : 0), pebble ? 84 + n : 62 + n - (rut ? 8 : 0), 255);
            } else if (kind == MAT_STONE) {
                const bool block = (x % 16 == 0) || (y % 13 == 0);
                const bool crack = (x == 18 + (y / 5) || y == 36 + (x / 8) || ((x + y * 2) % 41) == 0);
                fill_texel(pixels, x, y, crack ? 42 : 82 + n - (block ? 16 : 0), crack ? 48 : 90 + n - (block ? 14 : 0), crack ? 54 : 98 + n - (block ? 12 : 0), 255);
            } else if (kind == MAT_WOOD) {
                const int wave = (x + (int)(sinf((float)y * 0.35F) * 4.0F)) % 13;
                const bool grain = wave == 0 || wave == 1 || ((x - 32) * (x - 32) + (y - 24) * (y - 24) < 28);
                fill_texel(pixels, x, y, grain ? 60 + n : 104 + n, grain ? 34 + n : 62 + n, grain ? 18 + n : 32 + n, 255);
            } else if (kind == MAT_CLOTH) {
                const bool seam = x == 4 || x == 59 || y == 9 || ((x + y) % 13) == 0;
                const bool weave = (x % 6 == 0) || (y % 7 == 0);
                fill_texel(pixels, x, y, seam ? 56 + n : 126 + n - (weave ? 16 : 0), seam ? 12 : 22 + n, seam ? 18 : 30 + n, 255);
            } else {
                const int dx = x - 32;
                const int dy = y - 32;
                const bool ring = dx * dx + dy * dy > 340 && dx * dx + dy * dy < 460;
                const bool glyph = abs(dx) < 3 || abs(dy) < 3 || abs(dx + dy) < 3 || abs(dx - dy) < 3 || ring;
                fill_texel(pixels, x, y, glyph ? 86 + n : 4, glyph ? 232 : 64 + n, glyph ? 250 : 82 + n, glyph ? 238 : 145);
            }
        }
    }
}

static void material_pass_shutdown(void) {
    if (s_mat_pipeline.id != 0U) {
        nt_gfx_destroy_pipeline(s_mat_pipeline);
    }
    if (s_mat_vs.id != 0U) {
        nt_gfx_destroy_shader(s_mat_vs);
    }
    if (s_mat_fs.id != 0U) {
        nt_gfx_destroy_shader(s_mat_fs);
    }
    if (s_mat_vbo.id != 0U) {
        nt_gfx_destroy_buffer(s_mat_vbo);
    }
    if (s_mat_ibo.id != 0U) {
        nt_gfx_destroy_buffer(s_mat_ibo);
    }
    for (int i = 0; i < MAT_COUNT; ++i) {
        if (s_mat_textures[i].id != 0U) {
            nt_gfx_destroy_texture(s_mat_textures[i]);
        }
        s_mat_textures[i] = (nt_texture_t){0};
    }
    s_mat_vs = (nt_shader_t){0};
    s_mat_fs = (nt_shader_t){0};
    s_mat_pipeline = (nt_pipeline_t){0};
    s_mat_vbo = (nt_buffer_t){0};
    s_mat_ibo = (nt_buffer_t){0};
    s_material_pass_ready = false;
}

static void material_pass_init(void) {
    static const uint16_t indices[6] = {0U, 1U, 2U, 0U, 2U, 3U};
    material_pass_shutdown();
    s_mat_vs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_VERTEX, .source = s_mat_vs_src, .label = "blockfell_material_vs"});
    s_mat_fs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_FRAGMENT, .source = s_mat_fs_src, .label = "blockfell_material_fs"});
    const nt_vertex_layout_t layout_desc = {
        .attrs = {
            {.location = 0, .format = NT_FORMAT_FLOAT3, .offset = (uint16_t)offsetof(MaterialVertex, pos)},
            {.location = 1, .format = NT_FORMAT_FLOAT2, .offset = (uint16_t)offsetof(MaterialVertex, uv)},
            {.location = 2, .format = NT_FORMAT_FLOAT4, .offset = (uint16_t)offsetof(MaterialVertex, color)},
            {.location = 3, .format = NT_FORMAT_FLOAT3, .offset = (uint16_t)offsetof(MaterialVertex, normal)},
        },
        .attr_count = 4,
        .stride = (uint16_t)sizeof(MaterialVertex),
    };
    s_mat_pipeline = nt_gfx_make_pipeline(&(nt_pipeline_desc_t){
        .vertex_shader = s_mat_vs,
        .fragment_shader = s_mat_fs,
        .layout = layout_desc,
        .depth_test = true,
        .depth_write = false,
        .depth_func = NT_DEPTH_LEQUAL,
        .cull_mode = 0,
        .blend = true,
        .blend_src = NT_BLEND_SRC_ALPHA,
        .blend_dst = NT_BLEND_ONE_MINUS_SRC_ALPHA,
        .label = "blockfell_material_pipeline",
    });
    s_mat_vbo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_VERTEX,
        .usage = NT_USAGE_DYNAMIC,
        .size = (uint32_t)(sizeof(MaterialVertex) * 4U),
        .label = "blockfell_material_vbo",
    });
    s_mat_ibo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_INDEX,
        .usage = NT_USAGE_IMMUTABLE,
        .data = indices,
        .size = (uint32_t)sizeof(indices),
        .index_type = NT_INDEX_UINT16,
        .label = "blockfell_material_ibo",
    });
    for (int i = 0; i < MAT_COUNT; ++i) {
        uint8_t pixels[MATERIAL_TEX_SIZE * MATERIAL_TEX_SIZE * 4];
        build_material_texture((MaterialKind)i, pixels);
        s_mat_textures[i] = nt_gfx_make_texture(&(nt_texture_desc_t){
            .width = MATERIAL_TEX_SIZE,
            .height = MATERIAL_TEX_SIZE,
            .data = pixels,
            .format = NT_PIXEL_RGBA8,
            .min_filter = NT_FILTER_LINEAR,
            .mag_filter = NT_FILTER_NEAREST,
            .wrap_u = NT_WRAP_REPEAT,
            .wrap_v = NT_WRAP_REPEAT,
            .label = "blockfell_proc_material",
        });
    }
    s_material_pass_ready = s_mat_pipeline.id != 0U && s_mat_vbo.id != 0U && s_mat_ibo.id != 0U;
}

static void asset_pass_shutdown(void) {
    if (s_asset_pipeline.id != 0U) {
        nt_gfx_destroy_pipeline(s_asset_pipeline);
    }
    if (s_asset_vs.id != 0U) {
        nt_gfx_destroy_shader(s_asset_vs);
    }
    if (s_asset_fs.id != 0U) {
        nt_gfx_destroy_shader(s_asset_fs);
    }
    if (s_asset_vbo.id != 0U) {
        nt_gfx_destroy_buffer(s_asset_vbo);
    }
    if (s_asset_ibo.id != 0U) {
        nt_gfx_destroy_buffer(s_asset_ibo);
    }
    s_asset_vs = (nt_shader_t){0};
    s_asset_fs = (nt_shader_t){0};
    s_asset_pipeline = (nt_pipeline_t){0};
    s_asset_vbo = (nt_buffer_t){0};
    s_asset_ibo = (nt_buffer_t){0};
    s_asset_pass_ready = false;
}

static void asset_pass_init(void) {
    asset_pass_shutdown();
    s_asset_vs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_VERTEX, .source = s_asset_vs_src, .label = "blockfell_authored_asset_vs"});
    s_asset_fs = nt_gfx_make_shader(&(nt_shader_desc_t){.type = NT_SHADER_FRAGMENT, .source = s_asset_fs_src, .label = "blockfell_authored_asset_fs"});
    const nt_vertex_layout_t layout_desc = {
        .attrs = {
            {.location = 0, .format = NT_FORMAT_FLOAT3, .offset = (uint16_t)offsetof(BlockfellAssetVertex, pos)},
            {.location = 1, .format = NT_FORMAT_FLOAT3, .offset = (uint16_t)offsetof(BlockfellAssetVertex, normal)},
            {.location = 2, .format = NT_FORMAT_FLOAT4, .offset = (uint16_t)offsetof(BlockfellAssetVertex, color)},
        },
        .attr_count = 3,
        .stride = (uint16_t)sizeof(BlockfellAssetVertex),
    };
    s_asset_pipeline = nt_gfx_make_pipeline(&(nt_pipeline_desc_t){
        .vertex_shader = s_asset_vs,
        .fragment_shader = s_asset_fs,
        .layout = layout_desc,
        .depth_test = true,
        .depth_write = true,
        .depth_func = NT_DEPTH_LEQUAL,
        .cull_mode = 0,
        .blend = true,
        .blend_src = NT_BLEND_SRC_ALPHA,
        .blend_dst = NT_BLEND_ONE_MINUS_SRC_ALPHA,
        .label = "blockfell_authored_asset_pipeline",
    });
    s_asset_vbo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_VERTEX,
        .usage = NT_USAGE_DYNAMIC,
        .size = (uint32_t)(sizeof(BlockfellAssetVertex) * ASSET_MAX_VERTICES),
        .label = "blockfell_authored_asset_vbo",
    });
    s_asset_ibo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_INDEX,
        .usage = NT_USAGE_DYNAMIC,
        .size = (uint32_t)(sizeof(uint16_t) * ASSET_MAX_INDICES),
        .index_type = NT_INDEX_UINT16,
        .label = "blockfell_authored_asset_ibo",
    });
    s_asset_pass_ready = s_asset_pipeline.id != 0U && s_asset_vbo.id != 0U && s_asset_ibo.id != 0U;
}

static void material_quad(MaterialKind kind, const float a[3], const float b[3], const float c[3], const float d[3], const float uv[4], const float color[4], const float normal[3], const float vp[16]) {
    if (!s_material_pass_ready || s_mat_textures[kind].id == 0U) {
        return;
    }
    const MaterialVertex vertices[4] = {
        {.pos = {a[0], a[1], a[2]}, .uv = {uv[0], uv[1]}, .color = {color[0], color[1], color[2], color[3]}, .normal = {normal[0], normal[1], normal[2]}},
        {.pos = {b[0], b[1], b[2]}, .uv = {uv[2], uv[1]}, .color = {color[0], color[1], color[2], color[3]}, .normal = {normal[0], normal[1], normal[2]}},
        {.pos = {c[0], c[1], c[2]}, .uv = {uv[2], uv[3]}, .color = {color[0], color[1], color[2], color[3]}, .normal = {normal[0], normal[1], normal[2]}},
        {.pos = {d[0], d[1], d[2]}, .uv = {uv[0], uv[3]}, .color = {color[0], color[1], color[2], color[3]}, .normal = {normal[0], normal[1], normal[2]}},
    };
    nt_gfx_bind_pipeline(s_mat_pipeline);
    nt_gfx_set_uniform_mat4("u_vp", vp);
    nt_gfx_set_uniform_int("u_tex", 0);
    nt_gfx_bind_texture(s_mat_textures[kind], 0);
    nt_gfx_update_buffer(s_mat_vbo, vertices, (uint32_t)sizeof(vertices));
    nt_gfx_bind_vertex_buffer(s_mat_vbo);
    nt_gfx_bind_index_buffer(s_mat_ibo);
    nt_gfx_draw_indexed(0, 6, 4);
}

static void material_floor(MaterialKind kind, float x, float z, float sx, float sz, float repeat, const float color[4], const float vp[16]) {
    const float y = 0.046F;
    const float a[3] = {x - sx * 0.5F, y, z - sz * 0.5F};
    const float b[3] = {x + sx * 0.5F, y, z - sz * 0.5F};
    const float c[3] = {x + sx * 0.5F, y, z + sz * 0.5F};
    const float d[3] = {x - sx * 0.5F, y, z + sz * 0.5F};
    material_quad(kind, a, b, c, d, (float[4]){0.0F, 0.0F, repeat, repeat}, color, (float[3]){0.0F, 1.0F, 0.0F}, vp);
}

static void material_wall_z(MaterialKind kind, float x, float y, float z, float sx, float sy, float repeat, const float color[4], const float vp[16]) {
    const float a[3] = {x - sx * 0.5F, y - sy * 0.5F, z};
    const float b[3] = {x + sx * 0.5F, y - sy * 0.5F, z};
    const float c[3] = {x + sx * 0.5F, y + sy * 0.5F, z};
    const float d[3] = {x - sx * 0.5F, y + sy * 0.5F, z};
    material_quad(kind, a, b, c, d, (float[4]){0.0F, 0.0F, repeat, repeat}, color, (float[3]){0.0F, 0.0F, -1.0F}, vp);
}

static void material_wall_x(MaterialKind kind, float x, float y, float z, float sz, float sy, float repeat, const float color[4], const float vp[16]) {
    const float a[3] = {x, y - sy * 0.5F, z - sz * 0.5F};
    const float b[3] = {x, y - sy * 0.5F, z + sz * 0.5F};
    const float c[3] = {x, y + sy * 0.5F, z + sz * 0.5F};
    const float d[3] = {x, y + sy * 0.5F, z - sz * 0.5F};
    material_quad(kind, a, b, c, d, (float[4]){0.0F, 0.0F, repeat, repeat}, color, (float[3]){-1.0F, 0.0F, 0.0F}, vp);
}

static void draw_authored_mesh(const BlockfellAssetMesh *mesh, float x, float z, float scale, float yaw, const float tint[4], const float vp[16]) {
    if (!s_asset_pass_ready || mesh->vertex_count > ASSET_MAX_VERTICES || mesh->index_count > ASSET_MAX_INDICES) {
        return;
    }
    const float c = cosf(yaw);
    const float s = sinf(yaw);
    BlockfellAssetVertex vertices[ASSET_MAX_VERTICES];
    uint16_t indices[ASSET_MAX_INDICES];
    for (uint16_t i = 0; i < mesh->vertex_count; ++i) {
        const BlockfellAssetVertex *src = &mesh->vertices[i];
        const float px = src->pos[0] * scale;
        const float py = src->pos[1] * scale;
        const float pz = src->pos[2] * scale;
        const float nx = src->normal[0];
        const float nz = src->normal[2];
        vertices[i] = *src;
        vertices[i].pos[0] = x + px * c + pz * s;
        vertices[i].pos[1] = py;
        vertices[i].pos[2] = z - px * s + pz * c;
        vertices[i].normal[0] = nx * c + nz * s;
        vertices[i].normal[1] = src->normal[1];
        vertices[i].normal[2] = -nx * s + nz * c;
        vertices[i].color[0] = src->color[0] * tint[0];
        vertices[i].color[1] = src->color[1] * tint[1];
        vertices[i].color[2] = src->color[2] * tint[2];
        vertices[i].color[3] = src->color[3] * tint[3];
    }
    for (uint16_t i = 0; i < mesh->index_count; ++i) {
        indices[i] = mesh->indices[i];
    }
    nt_gfx_bind_pipeline(s_asset_pipeline);
    nt_gfx_set_uniform_mat4("u_vp", vp);
    nt_gfx_update_buffer(s_asset_vbo, vertices, (uint32_t)(sizeof(BlockfellAssetVertex) * mesh->vertex_count));
    nt_gfx_update_buffer(s_asset_ibo, indices, (uint32_t)(sizeof(uint16_t) * mesh->index_count));
    nt_gfx_bind_vertex_buffer(s_asset_vbo);
    nt_gfx_bind_index_buffer(s_asset_ibo);
    nt_gfx_draw_indexed(0, mesh->index_count, mesh->vertex_count);
}

static void cube3(const float center[3], const float size[3], const float color[4]) {
    nt_shape_renderer_cube(center, size, color);
    nt_shape_renderer_cube_wire(center, size, (float[4]){0.02F, 0.03F, 0.04F, 0.42F});
}

static void rect2(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void diamond2(float x, float y, float radius, const float color[4]) {
    nt_shape_renderer_triangle((float[3]){x, y + radius, 0.0F}, (float[3]){x + radius, y, 0.0F}, (float[3]){x, y - radius, 0.0F}, color);
    nt_shape_renderer_triangle((float[3]){x, y + radius, 0.0F}, (float[3]){x, y - radius, 0.0F}, (float[3]){x - radius, y, 0.0F}, color);
}

static void tri3(const float a[3], const float b[3], const float c[3], const float color[4]) {
    nt_shape_renderer_triangle(a, b, c, color);
    nt_shape_renderer_triangle_wire(a, b, c, (float[4]){0.02F, 0.03F, 0.04F, 0.32F});
}

static void quad3(const float a[3], const float b[3], const float c[3], const float d[3], const float color[4]) {
    tri3(a, b, c, color);
    tri3(a, c, d, color);
}

static void diamond3(float x, float y, float z, float radius, const float color[4]) {
    tri3((float[3]){x, y + radius, z}, (float[3]){x + radius, y, z}, (float[3]){x, y, z + radius}, color);
    tri3((float[3]){x, y + radius, z}, (float[3]){x, y, z + radius}, (float[3]){x - radius, y, z}, color);
    tri3((float[3]){x, y - radius, z}, (float[3]){x, y, z + radius}, (float[3]){x + radius, y, z}, color);
    tri3((float[3]){x, y - radius, z}, (float[3]){x - radius, y, z}, (float[3]){x, y, z + radius}, color);
}

static void ground_shadow(float x, float z, float radius, float alpha) {
    nt_shape_renderer_cylinder((float[3]){x + 0.28F, 0.022F, z - 0.24F}, radius, 0.018F, (float[4]){0.02F, 0.03F, 0.03F, alpha});
}

static void light_pool(float x, float z, float radius, const float color[4]) {
    nt_shape_renderer_cylinder((float[3]){x, 0.031F, z}, radius, 0.016F, color);
    nt_shape_renderer_cylinder_wire((float[3]){x, 0.047F, z}, radius * 0.66F, 0.03F, (float[4]){color[0], color[1], color[2], color[3] * 0.74F});
}

static void long_shadow(float x, float z, float length, float width, float alpha) {
    const float dir_x = 0.74F;
    const float dir_z = -0.52F;
    const float side_x = -dir_z;
    const float side_z = dir_x;
    const float y = 0.026F;
    const float a[3] = {x + side_x * width, y, z + side_z * width};
    const float b[3] = {x - side_x * width, y, z - side_z * width};
    const float c[3] = {x + dir_x * length - side_x * width * 0.44F, y, z + dir_z * length - side_z * width * 0.44F};
    const float d[3] = {x + dir_x * length + side_x * width * 0.44F, y, z + dir_z * length + side_z * width * 0.44F};
    quad3(a, b, c, d, (float[4]){0.01F, 0.02F, 0.02F, alpha});
}

static void ground_patch(float x, float z, float sx, float sz, const float color[4]) {
    const float floor_rot[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
    nt_shape_renderer_rect_rot((float[3]){x, 0.028F, z}, (float[2]){sx, sz}, floor_rot, color);
}

static void draw_grass_tuft(float x, float z, float scale) {
    nt_shape_renderer_line((float[3]){x, 0.05F, z}, (float[3]){x - 0.10F * scale, 0.36F * scale, z + 0.04F}, (float[4]){0.09F, 0.42F, 0.18F, 1.0F});
    nt_shape_renderer_line((float[3]){x, 0.05F, z}, (float[3]){x + 0.08F * scale, 0.34F * scale, z - 0.02F}, (float[4]){0.11F, 0.50F, 0.20F, 1.0F});
    nt_shape_renderer_line((float[3]){x, 0.05F, z}, (float[3]){x + 0.02F, 0.42F * scale, z + 0.10F * scale}, (float[4]){0.13F, 0.56F, 0.23F, 1.0F});
}

static void draw_crystal_cluster(float x, float z, float scale) {
    ground_shadow(x, z, 0.62F * scale, 0.25F);
    cube3((float[3]){x, 0.38F * scale, z}, (float[3]){0.22F * scale, 0.76F * scale, 0.22F * scale}, (float[4]){0.12F, 0.78F, 0.95F, 0.92F});
    cube3((float[3]){x + 0.28F * scale, 0.28F * scale, z - 0.12F * scale}, (float[3]){0.18F * scale, 0.56F * scale, 0.18F * scale}, (float[4]){0.16F, 0.88F, 0.70F, 0.86F});
    cube3((float[3]){x - 0.24F * scale, 0.24F * scale, z + 0.14F * scale}, (float[3]){0.16F * scale, 0.48F * scale, 0.16F * scale}, (float[4]){0.38F, 0.94F, 1.0F, 0.82F});
    nt_shape_renderer_sphere_wire((float[3]){x, 0.74F * scale, z}, 0.52F * scale, (float[4]){0.60F, 0.96F, 1.0F, 0.40F});
}

static void draw_banner(float x, float z, float height, const float color[4]) {
    cube3((float[3]){x, height * 0.5F, z}, (float[3]){0.12F, height, 0.12F}, (float[4]){0.16F, 0.10F, 0.07F, 1.0F});
    cube3((float[3]){x + 0.34F, height - 0.26F, z}, (float[3]){0.58F, 0.50F, 0.08F}, color);
    cube3((float[3]){x + 0.22F, height - 0.62F, z}, (float[3]){0.34F, 0.22F, 0.08F}, color);
}

static void draw_block_tree(float x, float z, float scale) {
    ground_shadow(x, z, 0.62F * scale, 0.24F);
    cube3((float[3]){x, 0.42F * scale, z}, (float[3]){0.28F * scale, 0.84F * scale, 0.28F * scale}, (float[4]){0.30F, 0.18F, 0.11F, 1.0F});
    cube3((float[3]){x, 1.02F * scale, z}, (float[3]){1.05F * scale, 0.78F * scale, 1.05F * scale}, (float[4]){0.05F, 0.34F, 0.20F, 1.0F});
    cube3((float[3]){x, 1.54F * scale, z}, (float[3]){0.72F * scale, 0.58F * scale, 0.72F * scale}, (float[4]){0.04F, 0.45F, 0.24F, 1.0F});
    cube3((float[3]){x + 0.22F * scale, 1.28F * scale, z - 0.28F * scale}, (float[3]){0.36F * scale, 0.26F * scale, 0.36F * scale}, (float[4]){0.12F, 0.55F, 0.28F, 1.0F});
}

static void draw_mountain(float x, float z, float h, float shade) {
    ground_shadow(x, z, 1.7F, 0.20F);
    cube3((float[3]){x, h * 0.18F, z}, (float[3]){2.8F, h * 0.36F, 2.8F}, (float[4]){0.22F * shade, 0.26F * shade, 0.31F * shade, 1.0F});
    cube3((float[3]){x, h * 0.47F, z}, (float[3]){2.0F, h * 0.30F, 2.0F}, (float[4]){0.28F * shade, 0.31F * shade, 0.35F * shade, 1.0F});
    cube3((float[3]){x, h * 0.74F, z}, (float[3]){1.05F, h * 0.25F, 1.05F}, (float[4]){0.78F, 0.87F, 0.93F, 1.0F});
    cube3((float[3]){x - 0.48F, h * 0.91F, z - 0.22F}, (float[3]){0.44F, h * 0.08F, 0.54F}, (float[4]){0.92F, 0.96F, 1.0F, 1.0F});
}

static void draw_world_floor(void) {
    const float floor_rot[4] = {0.7071068F, 0.0F, 0.0F, 0.7071068F};
    nt_shape_renderer_rect_rot((float[3]){0.0F, 0.0F, 0.0F}, (float[2]){22.0F, 22.0F}, floor_rot, (float[4]){0.26F, 0.48F, 0.33F, 1.0F});
    nt_shape_renderer_rect_rot((float[3]){0.2F, 0.012F, -0.8F}, (float[2]){14.8F, 1.28F}, floor_rot, (float[4]){0.58F, 0.51F, 0.35F, 1.0F});
    nt_shape_renderer_rect_rot((float[3]){2.2F, 0.014F, 2.8F}, (float[2]){5.2F, 1.20F}, floor_rot, (float[4]){0.66F, 0.59F, 0.39F, 1.0F});
    nt_shape_renderer_rect_rot((float[3]){4.7F, 0.016F, -1.9F}, (float[2]){3.9F, 1.10F}, floor_rot, (float[4]){0.63F, 0.56F, 0.36F, 1.0F});
    ground_patch(-5.2F, -3.9F, 1.2F, 0.56F, (float[4]){0.17F, 0.36F, 0.23F, 0.55F});
    ground_patch(-3.1F, -3.2F, 0.7F, 0.42F, (float[4]){0.70F, 0.64F, 0.44F, 0.72F});
    ground_patch(1.1F, 2.8F, 1.1F, 0.50F, (float[4]){0.43F, 0.34F, 0.25F, 0.72F});
    ground_patch(3.6F, -2.7F, 1.0F, 0.42F, (float[4]){0.72F, 0.60F, 0.36F, 0.70F});
    ground_patch(5.7F, 3.7F, 1.6F, 0.64F, (float[4]){0.22F, 0.28F, 0.29F, 0.55F});
    for (int i = -8; i <= 8; i += 2) {
        nt_shape_renderer_line((float[3]){(float)i, 0.025F, -9.5F}, (float[3]){(float)i, 0.025F, 9.5F}, (float[4]){0.12F, 0.24F, 0.17F, 0.25F});
        nt_shape_renderer_line((float[3]){-9.5F, 0.025F, (float)i}, (float[3]){9.5F, 0.025F, (float)i}, (float[4]){0.12F, 0.24F, 0.17F, 0.25F});
    }
    draw_grass_tuft(-6.0F, -1.2F, 1.0F);
    draw_grass_tuft(-3.2F, 0.4F, 0.8F);
    draw_grass_tuft(0.4F, -4.8F, 0.9F);
    draw_grass_tuft(4.4F, 1.4F, 0.8F);
    draw_grass_tuft(6.8F, -3.0F, 0.9F);
}

static void draw_torch(float x, float z) {
    ground_shadow(x, z, 0.30F, 0.25F);
    light_pool(x, z, 0.96F, (float[4]){1.0F, 0.45F, 0.12F, 0.16F});
    nt_shape_renderer_cylinder((float[3]){x, 0.035F, z}, 0.70F, 0.025F, (float[4]){0.95F, 0.42F, 0.06F, 0.20F});
    cube3((float[3]){x, 0.46F, z}, (float[3]){0.14F, 0.92F, 0.14F}, (float[4]){0.20F, 0.12F, 0.08F, 1.0F});
    nt_shape_renderer_sphere((float[3]){x, 1.06F, z}, 0.18F, (float[4]){1.0F, 0.42F, 0.10F, 0.92F});
    nt_shape_renderer_sphere_wire((float[3]){x, 1.06F, z}, 0.32F, (float[4]){1.0F, 0.74F, 0.22F, 0.56F});
    nt_shape_renderer_line((float[3]){x, 1.10F, z}, (float[3]){x - 0.28F, 1.70F, z - 0.18F}, (float[4]){1.0F, 0.70F, 0.20F, 0.30F});
    nt_shape_renderer_line((float[3]){x, 1.06F, z}, (float[3]){x + 0.22F, 1.56F, z + 0.14F}, (float[4]){1.0F, 0.55F, 0.14F, 0.26F});
}

static void draw_sun_rig(void) {
    nt_shape_renderer_sphere((float[3]){-7.2F, 8.8F, -2.4F}, 0.58F, (float[4]){1.0F, 0.80F, 0.34F, 1.0F});
    nt_shape_renderer_sphere_wire((float[3]){-7.2F, 8.8F, -2.4F}, 0.94F, (float[4]){1.0F, 0.88F, 0.48F, 0.55F});
    nt_shape_renderer_line((float[3]){-6.8F, 7.7F, -2.0F}, (float[3]){-4.8F, 3.6F, 0.8F}, (float[4]){1.0F, 0.86F, 0.45F, 0.32F});
    nt_shape_renderer_line((float[3]){-7.4F, 7.8F, -2.4F}, (float[3]){-5.2F, 3.8F, 2.4F}, (float[4]){1.0F, 0.86F, 0.45F, 0.24F});
    nt_shape_renderer_line((float[3]){-7.1F, 7.6F, -2.2F}, (float[3]){-0.8F, 3.2F, 4.8F}, (float[4]){1.0F, 0.92F, 0.58F, 0.18F});
    nt_shape_renderer_line((float[3]){-7.0F, 7.4F, -2.6F}, (float[3]){2.8F, 2.8F, 2.0F}, (float[4]){1.0F, 0.82F, 0.36F, 0.16F});
}

static void draw_atmosphere(void) {
    nt_shape_renderer_line((float[3]){-8.2F, 0.78F, 6.4F}, (float[3]){7.6F, 0.92F, 6.0F}, (float[4]){0.70F, 0.84F, 0.90F, 0.16F});
    nt_shape_renderer_line((float[3]){-6.6F, 1.20F, 4.8F}, (float[3]){6.8F, 1.34F, 4.5F}, (float[4]){0.70F, 0.84F, 0.90F, 0.12F});
    light_pool(-7.2F, -2.0F, 1.35F, (float[4]){0.50F, 0.88F, 0.62F, 0.07F});
    light_pool(5.4F, -1.4F, 1.22F, (float[4]){0.25F, 0.92F, 1.0F, 0.12F});
    light_pool(6.3F, 3.2F, 1.36F, (float[4]){0.22F, 0.84F, 1.0F, 0.12F});
}

static void draw_environment(void) {
    draw_world_floor();
    draw_sun_rig();
    draw_mountain(-8.0F, 6.0F, 5.2F, 1.0F);
    draw_mountain(-5.2F, 8.0F, 4.2F, 0.92F);
    draw_mountain(7.3F, 7.0F, 5.7F, 0.96F);
    draw_mountain(9.0F, -1.5F, 4.7F, 0.88F);
    draw_block_tree(-7.0F, -2.0F, 1.0F);
    draw_block_tree(-4.5F, 3.2F, 0.9F);
    draw_block_tree(2.6F, 5.8F, 1.05F);
    draw_block_tree(7.2F, -4.0F, 0.95F);
    draw_block_tree(-1.5F, -6.6F, 0.8F);
    draw_block_tree(5.6F, 1.2F, 0.74F);
    draw_block_tree(-7.6F, 4.6F, 0.82F);
    ground_shadow(-2.4F, 4.2F, 1.4F, 0.30F);
    cube3((float[3]){-2.4F, 0.18F, 4.2F}, (float[3]){2.2F, 0.36F, 1.0F}, (float[4]){0.34F, 0.35F, 0.34F, 1.0F});
    cube3((float[3]){-2.4F, 0.72F, 4.2F}, (float[3]){1.45F, 0.72F, 0.82F}, (float[4]){0.26F, 0.27F, 0.28F, 1.0F});
    cube3((float[3]){-2.4F, 1.34F, 4.2F}, (float[3]){1.05F, 0.52F, 0.72F}, (float[4]){0.40F, 0.36F, 0.30F, 1.0F});
    cube3((float[3]){-2.92F, 1.74F, 3.86F}, (float[3]){0.24F, 0.18F, 0.14F}, (float[4]){0.12F, 0.75F, 0.94F, 1.0F});
    cube3((float[3]){-2.12F, 1.70F, 3.86F}, (float[3]){0.24F, 0.14F, 0.14F}, (float[4]){0.12F, 0.75F, 0.94F, 1.0F});
    ground_shadow(s_camp_x, s_camp_z, 1.45F, 0.34F);
    cube3((float[3]){s_camp_x, 0.14F, s_camp_z}, (float[3]){2.8F, 0.28F, 2.4F}, (float[4]){0.30F, 0.27F, 0.24F, 1.0F});
    cube3((float[3]){s_camp_x - 0.35F, 0.32F, s_camp_z - 0.98F}, (float[3]){1.0F, 0.13F, 0.18F}, (float[4]){0.20F, 0.12F, 0.08F, 1.0F});
    cube3((float[3]){s_camp_x + 0.62F, 0.34F, s_camp_z - 0.76F}, (float[3]){0.86F, 0.12F, 0.18F}, (float[4]){0.26F, 0.16F, 0.10F, 1.0F});
    cube3((float[3]){s_camp_x - 1.1F, 0.42F, s_camp_z + 0.8F}, (float[3]){0.34F, 0.84F, 0.34F}, (float[4]){0.13F, 0.11F, 0.09F, 1.0F});
    cube3((float[3]){s_camp_x + 1.1F, 0.42F, s_camp_z + 0.8F}, (float[3]){0.34F, 0.84F, 0.34F}, (float[4]){0.13F, 0.11F, 0.09F, 1.0F});
    cube3((float[3]){s_camp_x, 0.84F, s_camp_z + 0.8F}, (float[3]){2.6F, 0.16F, 0.22F}, (float[4]){0.46F, 0.08F, 0.10F, 1.0F});
    draw_banner(s_camp_x - 1.34F, s_camp_z + 1.15F, 1.55F, (float[4]){0.56F, 0.04F, 0.08F, 1.0F});
    draw_banner(s_camp_x + 0.78F, s_camp_z + 1.20F, 1.42F, (float[4]){0.20F, 0.04F, 0.06F, 1.0F});
    draw_torch(s_camp_x - 1.45F, s_camp_z - 0.75F);
    draw_torch(s_camp_x + 1.45F, s_camp_z - 0.75F);
    ground_shadow(s_chest_x, s_chest_z, 0.9F, 0.30F);
    cube3((float[3]){3.7F, 0.22F, -2.6F}, (float[3]){1.7F, 0.44F, 1.1F}, (float[4]){0.35F, 0.24F, 0.15F, 1.0F});
    cube3((float[3]){3.7F, s_chest_open ? 0.78F : 0.58F, -2.9F}, (float[3]){1.5F, 0.22F, 0.26F}, s_chest_open ? (float[4]){0.95F, 0.68F, 0.17F, 1.0F} : (float[4]){0.18F, 0.12F, 0.08F, 1.0F});
    cube3((float[3]){3.18F, 0.48F, -2.02F}, (float[3]){0.14F, 0.28F, 0.10F}, (float[4]){0.92F, 0.68F, 0.18F, 1.0F});
    cube3((float[3]){4.22F, 0.48F, -2.02F}, (float[3]){0.14F, 0.28F, 0.10F}, (float[4]){0.92F, 0.68F, 0.18F, 1.0F});
    nt_shape_renderer_sphere((float[3]){-6.7F, 0.10F, 6.8F}, 0.62F, (float[4]){0.20F, 0.58F, 0.82F, 0.72F});
    nt_shape_renderer_sphere((float[3]){-6.1F, 0.08F, 6.2F}, 0.50F, (float[4]){0.20F, 0.58F, 0.82F, 0.60F});
    draw_crystal_cluster(-6.2F, -4.4F, 0.62F);
    draw_crystal_cluster(5.4F, -1.4F, 0.58F);
    draw_crystal_cluster(6.3F, 3.2F, 0.64F);
    draw_atmosphere();
}

static void draw_rune_sites(void) {
    for (int i = 0; i < RUNE_SITE_COUNT; ++i) {
        const RuneSite *site = &s_runes[i];
        const bool near = nearest_rune_index() == i;
        const bool unlocked = rune_unlocked(site);
        const float glow = near && unlocked && !site->claimed ? 1.0F : 0.58F;
        ground_shadow(site->x, site->z, 0.92F, 0.30F);
        light_pool(site->x, site->z, 1.28F, unlocked ? (float[4]){0.20F, 0.88F, 1.0F, 0.13F} : (float[4]){0.05F, 0.08F, 0.10F, 0.08F});
        nt_shape_renderer_cylinder((float[3]){site->x, 0.034F, site->z}, 1.08F, 0.026F, unlocked ? (float[4]){0.18F, 0.80F, 0.96F, 0.18F} : (float[4]){0.02F, 0.03F, 0.04F, 0.18F});
        cube3((float[3]){site->x, 0.22F, site->z}, (float[3]){1.28F, 0.44F, 1.28F}, (float[4]){0.30F, 0.31F, 0.34F, 1.0F});
        cube3((float[3]){site->x, 0.74F, site->z}, (float[3]){0.62F, 0.58F, 0.62F}, unlocked ? (float[4]){0.43F, 0.44F, 0.50F, 1.0F} : (float[4]){0.17F, 0.18F, 0.21F, 1.0F});
        cube3((float[3]){site->x - 0.36F, 0.56F, site->z - 0.36F}, (float[3]){0.16F, 0.10F, 0.16F}, unlocked ? (float[4]){0.12F, 0.78F, 0.96F, 1.0F} : (float[4]){0.09F, 0.10F, 0.12F, 1.0F});
        cube3((float[3]){site->x + 0.36F, 0.56F, site->z + 0.36F}, (float[3]){0.16F, 0.10F, 0.16F}, unlocked ? (float[4]){0.12F, 0.78F, 0.96F, 1.0F} : (float[4]){0.09F, 0.10F, 0.12F, 1.0F});
        if (site->claimed) {
            nt_shape_renderer_sphere((float[3]){site->x, 1.20F, site->z}, 0.34F, (float[4]){0.18F, 0.96F, 0.74F, 0.92F});
        } else {
            nt_shape_renderer_sphere((float[3]){site->x, 1.20F, site->z}, 0.26F + 0.06F * glow, unlocked ? (float[4]){0.22F, 0.86F, 1.0F, 0.90F} : (float[4]){0.12F, 0.14F, 0.16F, 0.80F});
        }
        if (near) {
            nt_shape_renderer_cylinder_wire((float[3]){site->x, 0.08F, site->z}, INTERACT_RADIUS, 0.10F, unlocked ? (float[4]){0.98F, 0.84F, 0.24F, 1.0F} : (float[4]){0.74F, 0.18F, 0.16F, 1.0F});
        }
    }
}

static void draw_gate(void) {
    const bool open = g_game_state.tutorial_done;
    ground_shadow(s_gate_x, s_gate_z, 1.7F, 0.36F);
    light_pool(s_gate_x, s_gate_z, 1.85F, open ? (float[4]){0.20F, 0.85F, 1.0F, 0.18F} : (float[4]){0.04F, 0.08F, 0.10F, 0.08F});
    cube3((float[3]){s_gate_x - 1.0F, 1.1F, s_gate_z}, (float[3]){0.65F, 2.2F, 0.78F}, (float[4]){0.30F, 0.32F, 0.36F, 1.0F});
    cube3((float[3]){s_gate_x + 1.0F, 1.1F, s_gate_z}, (float[3]){0.65F, 2.2F, 0.78F}, (float[4]){0.30F, 0.32F, 0.36F, 1.0F});
    cube3((float[3]){s_gate_x, 2.25F, s_gate_z}, (float[3]){2.7F, 0.55F, 0.82F}, (float[4]){0.24F, 0.25F, 0.29F, 1.0F});
    cube3((float[3]){s_gate_x - 1.0F, 2.32F, s_gate_z - 0.38F}, (float[3]){0.36F, 0.18F, 0.12F}, (float[4]){0.15F, 0.85F, 1.0F, 1.0F});
    cube3((float[3]){s_gate_x + 1.0F, 2.32F, s_gate_z - 0.38F}, (float[3]){0.36F, 0.18F, 0.12F}, (float[4]){0.15F, 0.85F, 1.0F, 1.0F});
    if (open) {
        nt_shape_renderer_sphere((float[3]){s_gate_x, 1.1F, s_gate_z - 0.18F}, 0.86F, (float[4]){0.18F, 0.75F, 0.95F, 0.58F});
        nt_shape_renderer_sphere_wire((float[3]){s_gate_x, 1.1F, s_gate_z - 0.18F}, 1.10F, (float[4]){0.78F, 0.98F, 1.0F, 0.75F});
    } else {
        for (int i = 0; i < 4; ++i) {
            const float x = s_gate_x - 0.54F + (float)i * 0.36F;
            cube3((float[3]){x, 1.03F, s_gate_z - 0.06F}, (float[3]){0.14F, 1.55F, 0.18F}, (float[4]){0.17F, 0.19F, 0.21F, 1.0F});
        }
        if (is_near_gate()) {
            nt_shape_renderer_cylinder_wire((float[3]){s_gate_x, 0.08F, s_gate_z}, 1.45F, 0.10F, (float[4]){0.95F, 0.30F, 0.22F, 1.0F});
        }
    }
}

static void draw_objective_marker(void) {
    float tx = 0.0F;
    float tz = 0.0F;
    objective_target(&tx, &tz);
    const ObjectiveStage stage = objective_stage();
    const bool done = stage == OBJ_DONE;
    const float marker_y = stage == OBJ_ENTER_GATE || stage == OBJ_DONE ? 3.1F : 2.25F;
    const float radius = done ? 0.52F : 0.42F;
    const float color[4] = {done ? 0.28F : 1.0F, done ? 1.0F : 0.82F, done ? 0.58F : 0.18F, 0.92F};
    nt_shape_renderer_line((float[3]){s_player_x, 0.12F, s_player_z}, (float[3]){tx, 0.12F, tz}, (float[4]){color[0], color[1], color[2], 0.34F});
    nt_shape_renderer_cylinder_wire((float[3]){tx, 0.10F, tz}, 0.78F, 0.08F, color);
    nt_shape_renderer_line((float[3]){tx, 0.24F, tz}, (float[3]){tx, marker_y, tz}, (float[4]){color[0], color[1], color[2], 0.52F});
    diamond3(tx, marker_y, tz, radius, color);
    nt_shape_renderer_sphere_wire((float[3]){tx, marker_y, tz}, radius * 0.86F, (float[4]){color[0], color[1], color[2], 0.70F});
    if (stage == OBJ_CLEAR_CAMP) {
        nt_shape_renderer_cylinder_wire((float[3]){s_camp_x, 0.18F, s_camp_z}, 1.76F, 0.12F, (float[4]){0.94F, 0.18F, 0.12F, 0.84F});
    } else if (stage == OBJ_OPEN_CHEST) {
        nt_shape_renderer_sphere_wire((float[3]){s_chest_x, 0.78F, s_chest_z}, 0.76F, (float[4]){1.0F, 0.72F, 0.18F, 0.84F});
    }
}

static void draw_enemy(const Enemy *enemy) {
    if (!enemy->alive) {
        ground_shadow(enemy->x, enemy->z, 0.46F, 0.20F);
        long_shadow(enemy->x, enemy->z, 0.92F, 0.28F, 0.20F);
        cube3((float[3]){enemy->x, 0.08F, enemy->z}, (float[3]){0.72F, 0.16F, 0.54F}, (float[4]){0.18F, 0.12F, 0.12F, 1.0F});
        return;
    }
    const float flash = enemy->hit_flash > 0.0F ? 1.0F : 0.0F;
    ground_shadow(enemy->x, enemy->z, 0.54F, 0.32F);
    long_shadow(enemy->x, enemy->z, 1.25F, 0.34F, 0.24F);
    cube3((float[3]){enemy->x, 0.52F, enemy->z}, (float[3]){0.58F, 0.72F, 0.46F}, (float[4]){0.56F + flash * 0.28F, 0.13F, 0.14F, 1.0F});
    cube3((float[3]){enemy->x - 0.01F, 0.71F, enemy->z - 0.245F}, (float[3]){0.50F, 0.055F, 0.035F}, (float[4]){0.82F, 0.28F, 0.12F, 1.0F});
    quad3((float[3]){enemy->x - 0.33F, 0.92F, enemy->z - 0.24F}, (float[3]){enemy->x + 0.33F, 0.92F, enemy->z - 0.24F}, (float[3]){enemy->x + 0.22F, 0.36F, enemy->z - 0.50F}, (float[3]){enemy->x - 0.22F, 0.36F, enemy->z - 0.50F}, (float[4]){0.15F, 0.05F, 0.06F, 1.0F});
    tri3((float[3]){enemy->x - 0.30F, 0.90F, enemy->z + 0.24F}, (float[3]){enemy->x + 0.30F, 0.90F, enemy->z + 0.24F}, (float[3]){enemy->x, 1.22F, enemy->z + 0.14F}, (float[4]){0.82F, 0.34F, 0.16F, 1.0F});
    cube3((float[3]){enemy->x, 1.08F, enemy->z}, (float[3]){0.44F, 0.40F, 0.44F}, (float[4]){0.80F, 0.55F, 0.38F, 1.0F});
    cube3((float[3]){enemy->x - 0.10F, 1.10F, enemy->z - 0.225F}, (float[3]){0.07F, 0.055F, 0.035F}, (float[4]){1.0F, 0.30F, 0.12F, 1.0F});
    cube3((float[3]){enemy->x + 0.10F, 1.10F, enemy->z - 0.225F}, (float[3]){0.07F, 0.055F, 0.035F}, (float[4]){1.0F, 0.30F, 0.12F, 1.0F});
    cube3((float[3]){enemy->x - 0.19F, 1.36F, enemy->z - 0.12F}, (float[3]){0.12F, 0.20F, 0.12F}, (float[4]){0.22F, 0.08F, 0.05F, 1.0F});
    cube3((float[3]){enemy->x + 0.19F, 1.36F, enemy->z - 0.12F}, (float[3]){0.12F, 0.20F, 0.12F}, (float[4]){0.22F, 0.08F, 0.05F, 1.0F});
    tri3((float[3]){enemy->x - 0.30F, 1.26F, enemy->z}, (float[3]){enemy->x - 0.68F, 1.36F, enemy->z - 0.08F}, (float[3]){enemy->x - 0.34F, 1.10F, enemy->z - 0.12F}, (float[4]){0.72F, 0.64F, 0.48F, 1.0F});
    tri3((float[3]){enemy->x + 0.30F, 1.26F, enemy->z}, (float[3]){enemy->x + 0.68F, 1.36F, enemy->z - 0.08F}, (float[3]){enemy->x + 0.34F, 1.10F, enemy->z - 0.12F}, (float[4]){0.72F, 0.64F, 0.48F, 1.0F});
    cube3((float[3]){enemy->x - 0.40F, 0.48F, enemy->z}, (float[3]){0.16F, 0.58F, 0.16F}, (float[4]){0.38F, 0.08F, 0.09F, 1.0F});
    cube3((float[3]){enemy->x + 0.40F, 0.48F, enemy->z}, (float[3]){0.16F, 0.58F, 0.16F}, (float[4]){0.38F, 0.08F, 0.09F, 1.0F});
    cube3((float[3]){enemy->x + 0.52F, 0.64F, enemy->z - 0.08F}, (float[3]){0.12F, 0.42F, 0.42F}, (float[4]){0.14F, 0.12F, 0.12F, 1.0F});
    cube3((float[3]){enemy->x + 0.59F, 0.68F, enemy->z - 0.30F}, (float[3]){0.035F, 0.22F, 0.22F}, (float[4]){0.84F, 0.38F, 0.18F, 1.0F});
    nt_shape_renderer_line((float[3]){enemy->x - 0.45F, 0.78F, enemy->z}, (float[3]){enemy->x - 0.92F, 0.92F, enemy->z + 0.36F}, (float[4]){0.86F, 0.82F, 0.68F, 1.0F});
    tri3((float[3]){enemy->x - 0.92F, 0.92F, enemy->z + 0.36F}, (float[3]){enemy->x - 1.12F, 0.78F, enemy->z + 0.52F}, (float[3]){enemy->x - 0.88F, 0.68F, enemy->z + 0.64F}, (float[4]){0.72F, 0.72F, 0.66F, 1.0F});
}

static void draw_enemies(void) {
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        draw_enemy(&s_enemies[i]);
    }
}

static void draw_player(void) {
    const float x = s_player_x;
    const float z = s_player_z;
    const float flash = s_player_hit_flash > 0.0F ? 0.25F : 0.0F;
    ground_shadow(x, z, 0.58F, 0.34F);
    long_shadow(x, z, 1.46F, 0.38F, 0.26F);
    quad3((float[3]){x - 0.34F, 1.02F, z - 0.30F}, (float[3]){x + 0.34F, 1.02F, z - 0.30F}, (float[3]){x + 0.44F, 0.12F, z - 0.62F}, (float[3]){x - 0.44F, 0.12F, z - 0.62F}, (float[4]){0.08F, 0.12F, 0.24F, 1.0F});
    cube3((float[3]){x, 0.66F, z - 0.31F}, (float[3]){0.48F, 0.70F, 0.12F}, (float[4]){0.10F, 0.16F, 0.30F, 1.0F});
    cube3((float[3]){x, 0.58F, z}, (float[3]){0.58F, 0.78F, 0.42F}, (float[4]){0.22F + flash, 0.45F, 0.82F, 1.0F});
    cube3((float[3]){x, 0.83F, z - 0.225F}, (float[3]){0.42F, 0.055F, 0.035F}, (float[4]){0.84F, 0.94F, 1.0F, 1.0F});
    diamond3(x, 0.64F, z - 0.235F, 0.085F, (float[4]){0.18F, 0.92F, 1.0F, 1.0F});
    tri3((float[3]){x - 0.28F, 0.96F, z + 0.24F}, (float[3]){x + 0.28F, 0.96F, z + 0.24F}, (float[3]){x, 1.22F, z + 0.18F}, (float[4]){0.72F, 0.84F, 0.96F, 1.0F});
    cube3((float[3]){x, 1.16F, z}, (float[3]){0.46F, 0.42F, 0.46F}, (float[4]){0.93F, 0.77F, 0.55F, 1.0F});
    cube3((float[3]){x, 1.43F, z}, (float[3]){0.56F, 0.18F, 0.52F}, (float[4]){0.12F, 0.14F, 0.18F, 1.0F});
    tri3((float[3]){x - 0.20F, 1.54F, z - 0.16F}, (float[3]){x, 1.88F, z}, (float[3]){x + 0.20F, 1.54F, z - 0.16F}, (float[4]){0.78F, 0.86F, 0.94F, 1.0F});
    cube3((float[3]){x - 0.43F, 0.52F, z}, (float[3]){0.20F, 0.62F, 0.20F}, (float[4]){0.17F, 0.32F, 0.62F, 1.0F});
    cube3((float[3]){x + 0.43F, 0.52F, z}, (float[3]){0.20F, 0.62F, 0.20F}, (float[4]){0.17F, 0.32F, 0.62F, 1.0F});
    cube3((float[3]){x - 0.58F, 0.70F, z + 0.10F}, (float[3]){0.12F, 0.54F, 0.42F}, (float[4]){0.16F, 0.20F, 0.24F, 1.0F});
    cube3((float[3]){x - 0.64F, 0.74F, z - 0.12F}, (float[3]){0.035F, 0.36F, 0.22F}, (float[4]){0.84F, 0.94F, 1.0F, 1.0F});
    nt_shape_renderer_sphere_wire((float[3]){x - 0.62F, 0.74F, z + 0.12F}, 0.32F, (float[4]){0.72F, 0.82F, 0.88F, 0.88F});
    cube3((float[3]){x - 0.18F, 0.15F, z}, (float[3]){0.18F, 0.32F, 0.20F}, (float[4]){0.09F, 0.10F, 0.12F, 1.0F});
    cube3((float[3]){x + 0.18F, 0.15F, z}, (float[3]){0.18F, 0.32F, 0.20F}, (float[4]){0.09F, 0.10F, 0.12F, 1.0F});
    const float sx = x + sinf(s_player_facing) * 0.82F;
    const float sz = z + cosf(s_player_facing) * 0.82F;
    nt_shape_renderer_line((float[3]){x + 0.34F, 0.74F, z}, (float[3]){sx, 0.98F, sz}, (float[4]){0.88F, 0.93F, 0.96F, 1.0F});
    tri3((float[3]){sx, 0.98F, sz}, (float[3]){sx + sinf(s_player_facing) * 0.26F + 0.08F, 1.06F, sz + cosf(s_player_facing) * 0.26F}, (float[3]){sx + sinf(s_player_facing) * 0.26F - 0.08F, 0.88F, sz + cosf(s_player_facing) * 0.26F}, (float[4]){0.88F, 0.94F, 1.0F, 1.0F});
    nt_shape_renderer_line((float[3]){x + 0.34F, 0.74F, z}, (float[3]){sx + 0.18F, 1.08F, sz + 0.16F}, (float[4]){0.60F, 0.92F, 1.0F, 0.62F});
    if (s_slash_timer > 0.0F) {
        nt_shape_renderer_cylinder_wire((float[3]){sx, 0.72F, sz}, ATTACK_RADIUS * 0.48F, 0.08F, (float[4]){0.94F, 0.96F, 1.0F, 0.90F});
    }
}

static void draw_material_overlays(const float vp[16]) {
    material_floor(MAT_GRASS, -2.6F, 1.8F, 10.8F, 10.6F, 7.0F, (float[4]){0.86F, 1.0F, 0.88F, 0.34F}, vp);
    material_floor(MAT_PATH, -2.8F, -4.0F, 4.0F, 1.4F, 4.0F, (float[4]){1.0F, 0.94F, 0.78F, 0.62F}, vp);
    material_floor(MAT_PATH, 0.1F, -0.8F, 14.8F, 1.28F, 8.0F, (float[4]){1.0F, 0.92F, 0.72F, 0.48F}, vp);
    material_floor(MAT_PATH, 2.2F, 2.8F, 5.2F, 1.20F, 3.0F, (float[4]){1.0F, 0.90F, 0.68F, 0.52F}, vp);
    material_floor(MAT_WOOD, s_camp_x, s_camp_z, 2.7F, 2.2F, 3.0F, (float[4]){0.92F, 0.74F, 0.56F, 0.70F}, vp);
    material_floor(MAT_STONE, s_gate_x, s_gate_z, 2.8F, 1.2F, 3.0F, (float[4]){0.82F, 0.88F, 0.92F, 0.62F}, vp);
    material_floor(MAT_STONE, -2.4F, 4.2F, 2.2F, 1.1F, 2.0F, (float[4]){0.72F, 0.78F, 0.82F, 0.58F}, vp);
    material_floor(MAT_WOOD, s_chest_x, s_chest_z, 1.8F, 1.2F, 2.0F, (float[4]){0.96F, 0.78F, 0.48F, 0.62F}, vp);

    for (int i = 0; i < RUNE_SITE_COUNT; ++i) {
        const float alpha = s_runes[i].claimed ? 0.70F : 0.46F;
        material_floor(MAT_RUNE, s_runes[i].x, s_runes[i].z, 1.38F, 1.38F, 1.0F, (float[4]){0.70F, 1.0F, 1.0F, alpha}, vp);
    }

    material_wall_z(MAT_STONE, s_gate_x, 1.08F, s_gate_z - 0.45F, 1.6F, 1.56F, 2.0F, (float[4]){0.82F, 0.94F, 1.0F, 0.40F}, vp);
    material_wall_z(MAT_CLOTH, s_camp_x, 0.86F, s_camp_z + 0.68F, 2.2F, 0.54F, 2.0F, (float[4]){0.92F, 0.76F, 0.76F, 0.76F}, vp);
    material_wall_z(MAT_WOOD, s_chest_x, 0.52F, s_chest_z - 0.56F, 1.55F, 0.48F, 2.0F, (float[4]){1.0F, 0.84F, 0.56F, 0.74F}, vp);
    material_wall_x(MAT_STONE, -2.92F, 0.94F, 4.2F, 0.92F, 1.2F, 2.0F, (float[4]){0.80F, 0.88F, 0.92F, 0.42F}, vp);
    material_wall_x(MAT_STONE, -1.88F, 0.94F, 4.2F, 0.92F, 1.2F, 2.0F, (float[4]){0.80F, 0.88F, 0.92F, 0.42F}, vp);
}

static void draw_authored_asset_overlays(const float vp[16]) {
    for (int i = 0; i < RUNE_SITE_COUNT; ++i) {
        const float tint = rune_unlocked(&s_runes[i]) ? 1.0F : 0.58F;
        draw_authored_mesh(&BF_MESH_RUNE_SPIRE, s_runes[i].x, s_runes[i].z, 1.0F, 0.78F, (float[4]){tint, tint, tint, 1.0F}, vp);
        draw_authored_mesh(&BF_MESH_RUNE_GLYPH, s_runes[i].x, s_runes[i].z, 1.0F, 0.78F, s_runes[i].claimed ? (float[4]){0.70F, 1.0F, 0.84F, 1.0F} : (float[4]){tint, tint, tint, 1.0F}, vp);
    }
    draw_authored_mesh(&BF_MESH_GATE_KEYSTONE, s_gate_x, s_gate_z, 1.08F, 0.0F, g_game_state.tutorial_done ? (float[4]){0.75F, 1.0F, 1.15F, 1.0F} : (float[4]){0.88F, 0.92F, 1.0F, 1.0F}, vp);
    draw_authored_mesh(&BF_MESH_CHEST_LOCK, s_chest_x, s_chest_z, 1.0F, 0.0F, s_chest_open ? (float[4]){1.15F, 1.05F, 0.70F, 1.0F} : (float[4]){1.0F, 1.0F, 1.0F, 1.0F}, vp);
    draw_authored_mesh(&BF_MESH_CAMP_STANDARD, s_camp_x - 1.20F, s_camp_z + 1.08F, 1.0F, 0.12F, (float[4]){1.0F, 1.0F, 1.0F, 1.0F}, vp);
    draw_authored_mesh(&BF_MESH_CAMP_STANDARD, s_camp_x + 0.92F, s_camp_z + 1.20F, 0.82F, -0.18F, (float[4]){0.74F, 0.78F, 0.86F, 1.0F}, vp);
    draw_authored_mesh(&BF_MESH_HERO_CAPE, s_player_x, s_player_z, 1.05F, s_player_facing, (float[4]){1.0F, 1.0F, 1.0F, 1.0F}, vp);
    draw_authored_mesh(&BF_MESH_HERO_CUIRASS, s_player_x, s_player_z, 1.04F, s_player_facing, (float[4]){1.0F, 1.0F, 1.0F, 1.0F}, vp);
    draw_authored_mesh(&BF_MESH_HERO_CREST, s_player_x, s_player_z, 1.02F, s_player_facing, (float[4]){1.0F, 1.0F, 1.0F, 1.0F}, vp);
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        const Enemy *enemy = &s_enemies[i];
        if (!enemy->alive) {
            continue;
        }
        const float tint = enemy->hit_flash > 0.0F ? 1.35F : 1.0F;
        draw_authored_mesh(&BF_MESH_ENEMY_MASK, enemy->x, enemy->z, 1.0F, 0.0F, (float[4]){tint, 1.0F, 1.0F, 1.0F}, vp);
        draw_authored_mesh(&BF_MESH_ENEMY_HORNS, enemy->x, enemy->z, 1.0F, 0.0F, (float[4]){tint, 1.0F, 1.0F, 1.0F}, vp);
    }
}

static void draw_3d_scene(float w, float h) {
    const float aspect = h > 0.0F ? w / h : 1.0F;
    vec3 eye = {s_player_x + 6.8F, 7.4F, s_player_z - 10.2F};
    vec3 center = {s_player_x + 2.4F, 0.90F, s_player_z + 5.0F};
    vec3 up = {0.0F, 1.0F, 0.0F};
    mat4 view;
    mat4 proj;
    mat4 vp;
    glm_lookat(eye, center, up, view);
    glm_perspective(glm_rad(58.0F), aspect, 0.1F, 70.0F, proj);
    glm_mat4_mul(proj, view, vp);
    nt_shape_renderer_set_vp((float *)vp);
    nt_shape_renderer_set_cam_pos((float[3]){eye[0], eye[1], eye[2]});
    nt_shape_renderer_set_depth(true);
    nt_shape_renderer_set_line_width(0.035F);
    draw_environment();
    draw_rune_sites();
    draw_gate();
    draw_objective_marker();
    draw_enemies();
    draw_player();
    nt_shape_renderer_flush();
    draw_material_overlays((const float *)vp);
    draw_authored_asset_overlays((const float *)vp);
}

static void draw_hud(float w, float h) {
    float vp[16];
    ortho(0.0F, w, 0.0F, h, -1.0F, 1.0F, vp);
    nt_shape_renderer_set_vp(vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(2.0F);

    rect2(16.0F, h - 58.0F, 166.0F, 42.0F, (float[4]){0.05F, 0.08F, 0.10F, 0.62F});
    for (int i = 0; i < RUNE_TARGET; ++i) {
        const float x = 44.0F + (float)i * 46.0F;
        const bool lit = i < g_game_state.wallet_soft;
        diamond2(x, h - 37.0F, 15.0F, lit ? (float[4]){0.15F, 0.92F, 1.0F, 1.0F} : (float[4]){0.20F, 0.28F, 0.32F, 1.0F});
        nt_shape_renderer_circle_wire((float[3]){x, h - 37.0F, 0.0F}, 19.0F, lit ? (float[4]){0.74F, 0.98F, 1.0F, 0.80F} : (float[4]){0.08F, 0.12F, 0.14F, 0.80F});
    }

    rect2(202.0F, h - 58.0F, 188.0F, 42.0F, (float[4]){0.05F, 0.08F, 0.10F, 0.62F});
    for (int i = 0; i < PLAYER_MAX_HP; ++i) {
        rect2(220.0F + (float)i * 26.0F, h - 44.0F, 18.0F, 18.0F, i < s_player_hp ? (float[4]){0.88F, 0.12F, 0.18F, 1.0F} : (float[4]){0.20F, 0.11F, 0.13F, 1.0F});
    }

    rect2(w - 184.0F, h - 58.0F, 168.0F, 42.0F, (float[4]){0.05F, 0.08F, 0.10F, 0.62F});
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        const bool alive = i < alive_enemy_count();
        diamond2(w - 154.0F + (float)i * 34.0F, h - 37.0F, 11.0F, alive ? (float[4]){0.86F, 0.16F, 0.18F, 1.0F} : (float[4]){0.16F, 0.24F, 0.18F, 1.0F});
    }

    const ObjectiveStage stage = objective_stage();
    const float chain_x = w * 0.5F - 168.0F;
    const float chain_y = h - 140.0F;
    rect2(chain_x - 18.0F, chain_y - 19.0F, 372.0F, 38.0F, (float[4]){0.04F, 0.07F, 0.09F, 0.50F});
    for (int i = 0; i < 6; ++i) {
        const float x = chain_x + (float)i * 66.0F;
        const bool complete = objective_step_complete(i);
        const bool current = i == (int)stage || (stage == OBJ_DONE && i == 5);
        const float color_done[4] = {0.18F, 0.86F, 0.54F, 1.0F};
        const float color_current[4] = {1.0F, 0.82F, 0.18F, 1.0F};
        const float color_locked[4] = {0.18F, 0.24F, 0.28F, 1.0F};
        const float *color = complete ? color_done : (current ? color_current : color_locked);
        if (i < 5) {
            rect2(x + 16.0F, chain_y - 2.0F, 34.0F, 4.0F, complete ? (float[4]){0.16F, 0.70F, 0.46F, 1.0F} : (float[4]){0.16F, 0.20F, 0.23F, 1.0F});
        }
        if (i == 1) {
            diamond2(x, chain_y, current ? 15.0F : 12.0F, color);
        } else if (i == 2) {
            rect2(x - 12.0F, chain_y - 9.0F, current ? 26.0F : 22.0F, current ? 18.0F : 16.0F, color);
            rect2(x - 8.0F, chain_y + 7.0F, current ? 18.0F : 14.0F, 5.0F, (float[4]){0.95F, 0.66F, 0.17F, 1.0F});
        } else if (i == 5) {
            rect2(x - 13.0F, chain_y - 14.0F, 6.0F, 28.0F, color);
            rect2(x + 7.0F, chain_y - 14.0F, 6.0F, 28.0F, color);
            rect2(x - 13.0F, chain_y + 10.0F, 26.0F, 6.0F, color);
        } else {
            diamond2(x, chain_y, current ? 15.0F : 12.0F, color);
            nt_shape_renderer_circle_wire((float[3]){x, chain_y, 0.0F}, current ? 19.0F : 15.0F, color);
        }
    }

    rect2(s_action_box.x, s_action_box.y, s_action_box.w, s_action_box.h, (float[4]){0.05F, 0.07F, 0.09F, 0.70F});
    rect2(s_attack_box.x, s_attack_box.y, s_attack_box.w, s_attack_box.h, (float[4]){0.05F, 0.07F, 0.09F, 0.70F});
    nt_shape_renderer_circle((float[3]){s_action_box.x + 30.0F, s_action_box.y + 23.0F, 0.0F}, 15.0F, action_ready() ? (float[4]){0.14F, 0.88F, 1.0F, 1.0F} : (float[4]){0.34F, 0.36F, 0.36F, 1.0F});
    diamond2(s_action_box.x + 30.0F, s_action_box.y + 23.0F, 10.0F, (float[4]){0.98F, 0.91F, 0.42F, 1.0F});
    rect2(s_action_box.x + 58.0F, s_action_box.y + 14.0F, s_action_box.w - 76.0F, 18.0F, action_ready() ? (float[4]){0.12F, 0.72F, 0.86F, 1.0F} : (float[4]){0.20F, 0.24F, 0.26F, 1.0F});
    nt_shape_renderer_circle((float[3]){s_attack_box.x + 30.0F, s_attack_box.y + 23.0F, 0.0F}, 15.0F, s_attack_cd <= 0.0F ? (float[4]){0.90F, 0.22F, 0.17F, 1.0F} : (float[4]){0.35F, 0.14F, 0.14F, 1.0F});
    nt_shape_renderer_line((float[3]){s_attack_box.x + 22.0F, s_attack_box.y + 16.0F, 0.0F}, (float[3]){s_attack_box.x + 38.0F, s_attack_box.y + 32.0F, 0.0F}, (float[4]){0.96F, 0.94F, 0.88F, 1.0F});
    rect2(s_attack_box.x + 58.0F, s_attack_box.y + 14.0F, s_attack_box.w - 76.0F, 18.0F, (float[4]){0.68F, 0.14F, 0.12F, 1.0F});

    const float gate_meter = g_game_state.tutorial_done ? 1.0F : (float)g_game_state.wallet_soft / (float)RUNE_TARGET;
    rect2(w * 0.5F - 118.0F, h - 92.0F, 236.0F, 12.0F, (float[4]){0.04F, 0.08F, 0.10F, 0.58F});
    rect2(w * 0.5F - 118.0F, h - 92.0F, 236.0F * gate_meter, 12.0F,
          g_game_state.tutorial_done ? (float[4]){0.22F, 0.88F, 0.54F, 1.0F} : (float[4]){0.12F, 0.75F, 0.96F, 1.0F});
    const float loot_w = clampf((float)g_game_state.wallet_hard / 45.0F, 0.0F, 1.0F);
    rect2(w * 0.5F - 118.0F, h - 112.0F, 236.0F, 8.0F, (float[4]){0.04F, 0.08F, 0.10F, 0.50F});
    rect2(w * 0.5F - 118.0F, h - 112.0F, 236.0F * loot_w, 8.0F, (float[4]){0.95F, 0.66F, 0.17F, 1.0F});
}

static void update_enemies(float dt) {
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        Enemy *enemy = &s_enemies[i];
        enemy->attack_cd = fmaxf(0.0F, enemy->attack_cd - dt);
        enemy->hit_flash = fmaxf(0.0F, enemy->hit_flash - dt);
        if (!enemy->alive || s_player_hp <= 0) {
            continue;
        }
        const float d = dist2(enemy->x, enemy->z, s_player_x, s_player_z);
        float target_x = enemy->home_x;
        float target_z = enemy->home_z;
        if (d < 36.0F) {
            target_x = s_player_x;
            target_z = s_player_z;
        }
        const float dx = target_x - enemy->x;
        const float dz = target_z - enemy->z;
        const float len = sqrtf(dx * dx + dz * dz);
        if (len > 0.06F && d > 1.05F) {
            enemy->x += dx / len * ENEMY_SPEED * dt;
            enemy->z += dz / len * ENEMY_SPEED * dt;
        }
        if (d < 1.55F && enemy->attack_cd <= 0.0F) {
            enemy->attack_cd = 0.92F;
            s_player_hp = s_player_hp > 0 ? s_player_hp - 1 : 0;
            s_player_hit_flash = 0.25F;
            sync_labels();
        }
    }
}

static void handle_input(void) {
    s_attack_cd = fmaxf(0.0F, s_attack_cd - g_nt_app.dt);
    s_slash_timer = fmaxf(0.0F, s_slash_timer - g_nt_app.dt);
    s_player_hit_flash = fmaxf(0.0F, s_player_hit_flash - g_nt_app.dt);
    if (s_player_hp <= 0) {
        if (nt_input_key_is_pressed(NT_KEY_R)) {
            reset_slice();
        }
        return;
    }

    float dx = 0.0F;
    float dz = 0.0F;
    if (nt_input_key_is_down(NT_KEY_A) || nt_input_key_is_down(NT_KEY_ARROW_LEFT)) {
        dx -= 1.0F;
    }
    if (nt_input_key_is_down(NT_KEY_D) || nt_input_key_is_down(NT_KEY_ARROW_RIGHT)) {
        dx += 1.0F;
    }
    if (nt_input_key_is_down(NT_KEY_W) || nt_input_key_is_down(NT_KEY_ARROW_UP)) {
        dz += 1.0F;
    }
    if (nt_input_key_is_down(NT_KEY_S) || nt_input_key_is_down(NT_KEY_ARROW_DOWN)) {
        dz -= 1.0F;
    }
    const float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.001F) {
        dx /= len;
        dz /= len;
        s_player_x = clampf(s_player_x + dx * PLAYER_SPEED * g_nt_app.dt, -WORLD_HALF, WORLD_HALF);
        s_player_z = clampf(s_player_z + dz * PLAYER_SPEED * g_nt_app.dt, -WORLD_HALF, WORLD_HALF);
        s_player_facing = atan2f(dx, dz);
    }
    if (nt_input_key_is_pressed(NT_KEY_E) || nt_input_key_is_pressed(NT_KEY_SPACE) || nt_input_key_is_pressed(NT_KEY_ENTER)) {
        (void)interact();
    }
    if (nt_input_key_is_pressed(NT_KEY_F)) {
        (void)attack();
    }
    if (nt_input_key_is_pressed(NT_KEY_R)) {
        reset_slice();
    }
    update_enemies(g_nt_app.dt);
}

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static cJSON *state_json(void) {
    cJSON *root = game_state_to_json(&g_game_state);
    const ObjectiveStage stage = objective_stage();
    cJSON_AddStringToObject(root, "runtime", "blockfell_runes");
    cJSON_AddNumberToObject(root, "player_x", (double)s_player_x);
    cJSON_AddNumberToObject(root, "player_z", (double)s_player_z);
    cJSON_AddNumberToObject(root, "player_hp", (double)s_player_hp);
    cJSON_AddNumberToObject(root, "objective_stage", (double)stage);
    cJSON_AddNumberToObject(root, "enemies_alive", (double)alive_enemy_count());
    cJSON_AddBoolToObject(root, "combat_cleared", combat_cleared());
    cJSON_AddBoolToObject(root, "chest_open", s_chest_open);
    cJSON_AddBoolToObject(root, "near_chest", is_near_chest());
    cJSON_AddBoolToObject(root, "near_gate", is_near_gate());
    cJSON_AddBoolToObject(root, "gate_open", g_game_state.tutorial_done);
    cJSON_AddBoolToObject(root, "action_ready", action_ready());
    cJSON *runes = cJSON_CreateArray();
    for (int i = 0; i < RUNE_SITE_COUNT; ++i) {
        cJSON *site = cJSON_CreateObject();
        cJSON_AddNumberToObject(site, "index", (double)i);
        cJSON_AddNumberToObject(site, "x", (double)s_runes[i].x);
        cJSON_AddNumberToObject(site, "z", (double)s_runes[i].z);
        cJSON_AddBoolToObject(site, "claimed", s_runes[i].claimed);
        cJSON_AddBoolToObject(site, "unlocked", rune_unlocked(&s_runes[i]));
        cJSON_AddItemToArray(runes, site);
    }
    cJSON_AddItemToObject(root, "rune_sites", runes);
    return root;
}

static bool emit_json(cJSON *result_obj, cJSON *src) {
    if (!src) {
        return false;
    }
    cJSON *child = src->child;
    while (child) {
        cJSON *next = child->next;
        cJSON_DetachItemViaPointer(src, child);
        cJSON_AddItemToObject(result_obj, child->string, child);
        child = next;
    }
    cJSON_Delete(src);
    return true;
}

static bool ep_game_state(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    return emit_json(result_obj, state_json());
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    reset_slice();
    return emit_json(result_obj, state_json());
}

static bool ep_game_action_claim(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    (void)interact();
    return emit_json(result_obj, state_json());
}

static bool ep_game_action_attack(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    (void)attack();
    return emit_json(result_obj, state_json());
}

static bool ep_game_action_cycle(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    if (!interact()) {
        (void)attack();
    }
    return emit_json(result_obj, state_json());
}

static bool ep_game_playtest_move_to_rune(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)err;
    (void)user;
    int index = 0;
    const cJSON *index_json = cJSON_GetObjectItemCaseSensitive(params, "index");
    if (cJSON_IsNumber(index_json)) {
        index = (int)index_json->valuedouble;
    }
    index = index < 0 ? 0 : (index >= RUNE_SITE_COUNT ? RUNE_SITE_COUNT - 1 : index);
    s_player_x = s_runes[index].x - 0.22F;
    s_player_z = s_runes[index].z - 0.28F;
    s_player_facing = 0.0F;
    return emit_json(result_obj, state_json());
}

static bool ep_game_playtest_move_to_camp(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    s_player_x = s_camp_x;
    s_player_z = s_camp_z - 0.55F;
    s_player_facing = 0.0F;
    return emit_json(result_obj, state_json());
}

static bool ep_game_playtest_move_to_chest(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    s_player_x = s_chest_x - 0.25F;
    s_player_z = s_chest_z - 0.30F;
    s_player_facing = 0.0F;
    return emit_json(result_obj, state_json());
}

static bool ep_game_playtest_complete_slice(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    s_player_x = s_runes[0].x - 0.20F;
    s_player_z = s_runes[0].z - 0.25F;
    (void)claim_rune();
    for (int i = 0; i < ENEMY_COUNT; ++i) {
        s_enemies[i].alive = false;
        s_enemies[i].hp = 0;
    }
    g_game_state.wallet_hard += 20;
    s_player_x = s_chest_x - 0.25F;
    s_player_z = s_chest_z - 0.30F;
    (void)open_chest();
    for (int i = 1; i < RUNE_SITE_COUNT; ++i) {
        s_player_x = s_runes[i].x - 0.20F;
        s_player_z = s_runes[i].z - 0.25F;
        (void)claim_rune();
    }
    s_player_x = s_gate_x - 0.35F;
    s_player_z = s_gate_z - 0.70F;
    refresh_gate();
    return emit_json(result_obj, state_json());
}

#ifndef NT_PLATFORM_WEB
static bool write_framebuffer_ppm(const char *path) {
    const int width = (int)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const int height = (int)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    if (!path || path[0] == '\0' || width <= 0 || height <= 0) {
        return false;
    }
    const size_t stride = (size_t)width * 3U;
    const size_t size = stride * (size_t)height;
    unsigned char *pixels = (unsigned char *)malloc(size);
    if (!pixels) {
        return false;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    FILE *file = fopen(path, "wb");
    if (!file) {
        free(pixels);
        return false;
    }
    (void)fprintf(file, "P6\n%d %d\n255\n", width, height);
    for (int y = height - 1; y >= 0; --y) {
        (void)fwrite(pixels + (size_t)y * stride, 1, stride, file);
    }
    const bool ok = ferror(file) == 0;
    (void)fclose(file);
    free(pixels);
    return ok;
}

static void flush_pending_capture(void) {
    if (s_pending_capture_path[0] == '\0') {
        return;
    }
    (void)write_framebuffer_ppm(s_pending_capture_path);
    s_pending_capture_path[0] = '\0';
}

static bool ep_game_capture_framebuffer(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)err;
    (void)user;
    const cJSON *output = cJSON_GetObjectItemCaseSensitive(params, "output");
    if (!cJSON_IsString(output) || !output->valuestring || output->valuestring[0] == '\0') {
        cJSON_AddStringToObject(result_obj, "error", "output path is required");
        return false;
    }
    (void)snprintf(s_pending_capture_path, sizeof(s_pending_capture_path), "%s", output->valuestring);
    s_pending_capture_path[sizeof(s_pending_capture_path) - 1U] = '\0';
    cJSON_AddStringToObject(result_obj, "output", s_pending_capture_path);
    return true;
}
#endif

static void register_game_endpoints(void) {
    game_state_register_devapi();
    static const nt_devapi_command_desc descs[] = {
        {"game.state", "game", "Return the current Blockfell Runes state.", "", "state object", "immediate", "none"},
        {"game.reset_playtest", "game", "Reset the Blockfell Runes slice.", "", "state object", "immediate", "mutates state"},
        {"game.action.cycle", "game", "Try the current context action, otherwise strike.", "", "state object", "immediate", "mutates state"},
        {"game.action.claim", "game", "Claim a rune or open the chest when the hero is close enough.", "", "state object", "immediate", "mutates state"},
        {"game.action.attack", "game", "Swing the sword at nearby enemies.", "", "state object", "immediate", "mutates state"},
        {"game.playtest.move_to_rune", "game", "Move the hero into a rune interaction ring.", "{\"index\":0}", "state object", "immediate", "mutates state"},
        {"game.playtest.move_to_camp", "game", "Move the hero into the bandit camp.", "", "state object", "immediate", "mutates state"},
        {"game.playtest.move_to_chest", "game", "Move the hero into the chest interaction ring.", "", "state object", "immediate", "mutates state"},
        {"game.playtest.complete_slice", "game", "Complete the full combat, loot, rune, and gate route.", "", "state object", "immediate", "mutates state"},
#ifndef NT_PLATFORM_WEB
        {"game.capture.framebuffer", "game", "Capture the current framebuffer to a PPM file on the next rendered frame.", "{\"output\":\"path.ppm\"}", "capture object", "immediate", "writes file"},
#endif
    };
    (void)nt_devapi_register(&descs[0], ep_game_state, NULL);
    (void)nt_devapi_register(&descs[1], ep_game_reset_playtest, NULL);
    (void)nt_devapi_register(&descs[2], ep_game_action_cycle, NULL);
    (void)nt_devapi_register(&descs[3], ep_game_action_claim, NULL);
    (void)nt_devapi_register(&descs[4], ep_game_action_attack, NULL);
    (void)nt_devapi_register(&descs[5], ep_game_playtest_move_to_rune, NULL);
    (void)nt_devapi_register(&descs[6], ep_game_playtest_move_to_camp, NULL);
    (void)nt_devapi_register(&descs[7], ep_game_playtest_move_to_chest, NULL);
    (void)nt_devapi_register(&descs[8], ep_game_playtest_complete_slice, NULL);
#ifndef NT_PLATFORM_WEB
    (void)nt_devapi_register(&descs[9], ep_game_capture_framebuffer, NULL);
#endif
    game_devapi_ui_register();
}

static void register_ui_devapi(float w, float h) {
    game_devapi_ui_clear();
    (void)game_devapi_ui_register_node("root", "", "screen", "Blockfell Runes", "Cubic fantasy action RPG slice", 0.0F, 0.0F, w, h, true, true);
    (void)game_devapi_ui_register_node("rune.progress", "root", "meter", "Rune Progress", g_game_state.test_label_text, 16.0F, h - 58.0F, 166.0F, 42.0F, true, true);
    (void)game_devapi_ui_register_node("quest.chain", "root", "meter", "Quest Chain", "Route progress", w * 0.5F - 186.0F, h - 159.0F, 372.0F, 38.0F, true, true);
    (void)game_devapi_ui_register_node("combat.status", "root", "meter", "Combat", "Enemy camp status", w - 184.0F, h - 58.0F, 168.0F, 42.0F, true, true);
    (void)game_devapi_ui_register_node("loot.status", "root", "meter", "Loot", "Chest and gold progress", w * 0.5F - 118.0F, h - 112.0F, 236.0F, 8.0F, true, true);
    (void)game_devapi_ui_register_node("action.claim", "root", "button", "Context Action", g_game_state.test_button_text, s_action_box.x, s_action_box.y, s_action_box.w, s_action_box.h, action_ready(), true);
    (void)game_devapi_ui_register_node("action.attack", "root", "button", "Attack", "Strike", s_attack_box.x, s_attack_box.y, s_attack_box.w, s_attack_box.h, s_attack_cd <= 0.0F, true);
}
#endif

static void frame(void) {
    nt_window_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_update();
    }
#endif
    nt_input_poll();

    const float w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    layout(w);
    handle_input();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        register_ui_devapi(w, h);
    }
#endif

#ifndef NT_PLATFORM_WEB
    if (nt_window_should_close() || nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        nt_app_quit();
    }
#endif

    nt_gfx_begin_frame();
    if (g_nt_gfx.context_restored) {
        nt_shape_renderer_restore_gpu();
        material_pass_init();
        asset_pass_init();
    }
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.50F, 0.70F, 0.86F, 1.0F}, .clear_depth = 1.0F});
    draw_3d_scene(w, h);
    nt_shape_renderer_flush();
    draw_hud(w, h);
    nt_shape_renderer_flush();
#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
    flush_pending_capture();
#endif
    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Blockfell Runes";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    parse_args(argc, argv);
    reset_slice();

    g_nt_window.title = "Blockfell Runes";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    nt_gfx_init(&gfx_desc);
    nt_shape_renderer_init();
    material_pass_init();
    asset_pass_init();

#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        if (nt_devapi_init() != NT_OK) {
            (void)fprintf(stderr, "Failed to init DevAPI\n");
            s_devapi_enabled = false;
        } else {
            register_game_endpoints();
            if (!nt_devapi_net_start(s_devapi_port)) {
                (void)fprintf(stderr, "Failed to start DevAPI on port %u\n", (unsigned)s_devapi_port);
            }
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
    nt_shape_renderer_shutdown();
    material_pass_shutdown();
    asset_pass_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif
    return 0;
}
