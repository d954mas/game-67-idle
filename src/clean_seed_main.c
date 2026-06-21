/* Temporary debug renderer debt: replace shape-composed RPG proof with product bitmap UI/assets before visual acceptance. */

#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "devapi/nt_devapi_net.h"
#include "ember_road_base.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "game_actions.h"
#include "game_devapi_ui.h"
#include "game_state.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "material/nt_material.h"
#include "math/nt_math.h"
#include "nt_pack_format.h"
#include "render/nt_render_defs.h"
#include "renderers/nt_shape_renderer.h"
#include "renderers/nt_sprite_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "window/nt_window.h"

#if !defined(NT_PLATFORM_WEB)
#include <glad/gl.h>
#endif

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EMBER_DEVAPI_PORT_DEFAULT 9123
#define EMBER_LEVEL_2_XP 20

typedef struct UiBox {
    float x;
    float y;
    float w;
    float h;
} UiBox;

static bool s_devapi_enabled;
static uint16_t s_devapi_port = EMBER_DEVAPI_PORT_DEFAULT;
static int s_window_width = 1280;
static int s_window_height = 720;
static float s_view_h = 720.0F;

static UiBox s_primary_box;
static UiBox s_map_town_box;
static UiBox s_map_road_box;
static UiBox s_map_mine_box;
static UiBox s_equip_box;
static UiBox s_claim_box;
static UiBox s_scene_box;
static UiBox s_quest_box;
static UiBox s_battle_preview_box;
static UiBox s_mine_choice_box;
static UiBox s_mine_scout_box;
static UiBox s_mine_back_box;
static UiBox s_forge_workbench_box;
static char s_last_message[128] = "Welcome to Old Gate.";
static nt_hash32_t s_asset_pack_id;
static nt_resource_t s_ui_font_resource;
static nt_resource_t s_old_gate_atlas_resource;
static nt_resource_t s_old_gate_atlas_texture0_resource;
static nt_font_t s_ui_font;
static nt_material_t s_sprite_material;
static nt_material_t s_text_material;
static nt_buffer_t s_frame_ubo;

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static bool s_capture_pending;
static char s_capture_path[512];
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

static float sy(float y, float h) {
    /* Game/UI layout is Y-up; convert only at the renderer boundary. */
    return s_view_h - y - h;
}

static bool contains_y_up(UiBox box, float x, float y) {
    return x >= box.x && x <= box.x + box.w && y >= box.y && y <= box.y + box.h;
}

static void set_message(const char *text) {
    (void)snprintf(s_last_message, sizeof(s_last_message), "%s", text ? text : "");
    s_last_message[sizeof(s_last_message) - 1] = '\0';
}

static bool invoke_action(GameActionResult (*fn)(char *, int)) {
    char message[128];
    const GameActionResult result = fn(message, (int)sizeof(message));
    set_message(message);
    return result == GAME_ACTION_OK;
}

static bool invoke_primary_action(void) {
    const char *id = game_actions_primary_action_id();
    if (strcmp(id, "ember.accept_quest") == 0) {
        return invoke_action(game_actions_accept_quest);
    }
    if (strcmp(id, "ember.travel_north_road") == 0) {
        return invoke_action(game_actions_travel_north_road);
    }
    if (strcmp(id, "ember.auto_battle") == 0) {
        return invoke_action(game_actions_auto_battle);
    }
    if (strcmp(id, "ember.equip_ring") == 0) {
        return invoke_action(game_actions_equip_ring);
    }
    if (strcmp(id, "ember.claim_reward") == 0) {
        return invoke_action(game_actions_claim_reward);
    }
    if (strcmp(id, "ember.enter_old_mine") == 0) {
        return invoke_action(game_actions_enter_old_mine);
    }
    if (strcmp(id, "ember.scout_old_mine") == 0) {
        return invoke_action(game_actions_scout_old_mine);
    }
    if (strcmp(id, "ember.resolve_old_mine_depth") == 0) {
        return invoke_action(game_actions_resolve_old_mine_depth);
    }
    if (strcmp(id, "ember.delve_old_mine") == 0) {
        return invoke_action(game_actions_delve_old_mine);
    }
    if (strcmp(id, "ember.return_old_gate") == 0) {
        return invoke_action(game_actions_return_old_gate);
    }
    if (strcmp(id, "ember.forge_mine_lantern") == 0) {
        return invoke_action(game_actions_forge_mine_lantern);
    }
    set_message("Choose the next Ember Road route.");
    return false;
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

static void layout(float w, float h) {
    const float margin = w < 900.0F ? 18.0F : 28.0F;
    const float top_h = 92.0F;
    const float bottom_h = 112.0F;
    s_scene_box = (UiBox){.x = margin, .y = bottom_h, .w = w - margin * 2.0F, .h = h - bottom_h - top_h - 18.0F};
    const float rail_w = s_scene_box.w < 960.0F ? 330.0F : 380.0F;
    s_quest_box = (UiBox){.x = s_scene_box.x + s_scene_box.w - rail_w - 22.0F, .y = s_scene_box.y + 28.0F, .w = rail_w, .h = s_scene_box.h - 56.0F};
    s_primary_box = (UiBox){.x = s_quest_box.x + 28.0F, .y = s_quest_box.y + 32.0F, .w = s_quest_box.w - 56.0F, .h = 62.0F};
    s_map_town_box = (UiBox){.x = s_scene_box.x + 76.0F, .y = s_scene_box.y + 42.0F, .w = 150.0F, .h = 54.0F};
    s_map_road_box = (UiBox){.x = s_scene_box.x + 278.0F, .y = s_scene_box.y + 56.0F, .w = 160.0F, .h = 58.0F};
    s_map_mine_box = (UiBox){.x = s_scene_box.x + 506.0F, .y = s_scene_box.y + 42.0F, .w = 150.0F, .h = 54.0F};
    s_forge_workbench_box = (UiBox){.x = s_scene_box.x + s_scene_box.w * 0.43F, .y = s_scene_box.y + 154.0F, .w = 250.0F, .h = 150.0F};
    s_battle_preview_box = (UiBox){.x = s_quest_box.x + 26.0F, .y = s_quest_box.y + s_quest_box.h - 270.0F, .w = s_quest_box.w - 52.0F, .h = 106.0F};
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED && g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE) {
        s_mine_choice_box = (UiBox){.x = s_quest_box.x + 30.0F, .y = s_quest_box.y + 112.0F, .w = s_quest_box.w - 60.0F, .h = 188.0F};
        const float mine_button_w = (s_mine_choice_box.w - 56.0F) * 0.5F;
        s_mine_scout_box = (UiBox){.x = s_mine_choice_box.x + 20.0F, .y = s_mine_choice_box.y + 34.0F, .w = mine_button_w, .h = 52.0F};
        s_mine_back_box = (UiBox){.x = s_mine_scout_box.x + mine_button_w + 16.0F, .y = s_mine_choice_box.y + 34.0F, .w = mine_button_w, .h = 52.0F};
    } else {
        s_mine_choice_box = (UiBox){.x = s_quest_box.x + 30.0F, .y = s_quest_box.y + 112.0F, .w = s_quest_box.w - 60.0F, .h = 188.0F};
        s_mine_scout_box = (UiBox){.x = s_mine_choice_box.x + 20.0F, .y = s_mine_choice_box.y + 34.0F, .w = 128.0F, .h = 52.0F};
        s_mine_back_box = (UiBox){.x = s_mine_choice_box.x + s_mine_choice_box.w - 148.0F, .y = s_mine_choice_box.y + 34.0F, .w = 128.0F, .h = 52.0F};
    }
    const float reward_button_w = (s_quest_box.w - 72.0F) * 0.5F;
    s_equip_box = (UiBox){.x = s_quest_box.x + 28.0F, .y = s_quest_box.y + 32.0F, .w = reward_button_w, .h = 54.0F};
    s_claim_box = (UiBox){.x = s_equip_box.x + reward_button_w + 16.0F, .y = s_quest_box.y + 32.0F, .w = reward_button_w, .h = 54.0F};
}

static void rect(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect((float[3]){x + w * 0.5F, sy(y, h) + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void rect_wire(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect_wire((float[3]){x + w * 0.5F, sy(y, h) + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void circle(float x, float y, float radius, const float color[4]) {
    nt_shape_renderer_circle((float[3]){x, sy(y - radius, radius * 2.0F) + radius, 0.0F}, radius, color);
}

static void line(float ax, float ay, float bx, float by, const float color[4]) {
    nt_shape_renderer_line((float[3]){ax, sy(ay, 0.0F), 0.0F}, (float[3]){bx, sy(by, 0.0F), 0.0F}, color);
}

static void text_model(float x, float y, float out[16]) {
    memset(out, 0, sizeof(float) * 16);
    out[0] = 1.0F;
    out[5] = 1.0F;
    out[10] = 1.0F;
    out[12] = x;
    out[13] = y;
    out[15] = 1.0F;
}

static void sprite_model(float x, float y, float w, float h, const nt_texture_region_t *region, float out[16]) {
    memset(out, 0, sizeof(float) * 16);
    const float source_w = region && region->source_w > 0 ? (float)region->source_w : w;
    const float source_h = region && region->source_h > 0 ? (float)region->source_h : h;
    out[0] = source_w > 0.0F ? w / source_w : 1.0F;
    out[5] = source_h > 0.0F ? h / source_h : 1.0F;
    out[10] = 1.0F;
    out[12] = x + w * 0.5F;
    out[13] = y + h * 0.5F;
    out[15] = 1.0F;
}

static bool sprite_assets_ready(void) {
    const nt_material_info_t *info = nt_material_get_info(s_sprite_material);
    return info && info->ready && nt_resource_is_ready(s_old_gate_atlas_resource);
}

static bool old_gate_region(nt_hash64_t name, uint32_t *region_index) {
    if (!sprite_assets_ready()) {
        return false;
    }
    const uint32_t region = nt_atlas_find_region(s_old_gate_atlas_resource, name.value);
    if (region == NT_ATLAS_INVALID_REGION) {
        return false;
    }
    *region_index = region;
    return true;
}

static void sprite_fit_y_up(nt_hash64_t name, float x, float y, float w, float h) {
    uint32_t region_index;
    if (!old_gate_region(name, &region_index)) {
        return;
    }
    const nt_texture_region_t *region = nt_atlas_get_region(s_old_gate_atlas_resource, region_index);
    float model[16];
    sprite_model(x, y, w, h, region, model);
    nt_sprite_renderer_set_material(s_sprite_material);
    nt_sprite_renderer_emit_region(s_old_gate_atlas_resource, region_index, model, 0.5F, 0.5F, 0xFFFFFFFFU, 0);
}

static void slice9_y_up(nt_hash64_t name, float x, float y, float w, float h) {
    uint32_t region_index;
    if (!old_gate_region(name, &region_index)) {
        return;
    }
    nt_sprite_renderer_set_material(s_sprite_material);
    nt_sprite_renderer_emit_slice9(s_old_gate_atlas_resource, region_index, x, y, w, h, NULL, 1.0F, 0xFFFFFFFFU, 0, NT_MATH_MAT4_IDENTITY);
}

static float text_size(float scale) {
    return scale * 7.0F;
}

static void draw_text_panel(const char *text, float x, float y, float scale) {
    const nt_material_info_t *info = nt_material_get_info(s_text_material);
    if (!text || !info || !info->ready || !nt_resource_is_ready(s_ui_font_resource)) {
        return;
    }
    float model[16];
    nt_text_renderer_set_material(s_text_material);
    nt_text_renderer_set_font(s_ui_font);
    text_model(x + 1.0F, y - 1.0F, model);
    nt_text_renderer_draw(text, model, text_size(scale), (float[4]){0.04F, 0.025F, 0.015F, 0.86F}, 0.0F, 0.0F);
    text_model(x, y, model);
    nt_text_renderer_draw(text, model, text_size(scale), (float[4]){1.0F, 0.86F, 0.52F, 1.0F}, 0.0F, 0.0F);
}

static void bar(float x, float y, float w, float h, float frac, const float back[4], const float fill[4]) {
    if (frac < 0.0F) frac = 0.0F;
    if (frac > 1.0F) frac = 1.0F;
    rect(x, y, w, h, back);
    rect(x, y, w * frac, h, fill);
    rect_wire(x, y, w, h, (float[4]){0.95F, 0.78F, 0.42F, 0.75F});
}

static void panel(UiBox box, const float fill[4]) {
    rect(box.x + 5.0F, box.y - 5.0F, box.w, box.h, (float[4]){0.06F, 0.035F, 0.025F, 0.35F});
    rect(box.x, box.y, box.w, box.h, fill);
    rect_wire(box.x, box.y, box.w, box.h, (float[4]){0.94F, 0.70F, 0.32F, 1.0F});
}

static void draw_mine_scene_anchors(UiBox scene, bool with_sprites) {
    if (g_game_state.location_index != GAME_STATE_LOCATION_OLD_MINE) {
        return;
    }
    const bool scouted = g_game_state.old_mine_scouted;
    const bool depth_done = scouted && g_game_state.old_mine_depth_resolved;
    const bool cache_claimed = depth_done && g_game_state.old_mine_cache_claimed;
    const float cache_x = scene.x + scene.w * 0.60F;
    const float cache_y = scene.y + 206.0F;
    const float lock_x = scene.x + scene.w * 0.25F;
    const float lock_y = scene.y + 222.0F;
    const float threat_x = scene.x + scene.w * 0.39F;
    const float threat_y = scene.y + 286.0F;

    if (scouted && !depth_done) {
        if (with_sprites) {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_SWORD_AUTO_BATTLE, threat_x - 24.0F, threat_y - 10.0F, 48.0F, 48.0F);
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_WOLF_MARKER, threat_x + 20.0F, threat_y - 4.0F, 34.0F, 34.0F);
        } else {
            circle(threat_x, threat_y + 18.0F, 24.0F, (float[4]){0.62F, 0.14F, 0.12F, 0.90F});
            rect(threat_x + 20.0F, threat_y + 6.0F, 34.0F, 22.0F, (float[4]){0.72F, 0.56F, 0.22F, 0.92F});
        }
        draw_text_panel("BAT SIGNS", threat_x - 38.0F, threat_y - 2.0F, 1.65F);
    }

    if (depth_done) {
        if (with_sprites) {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_QUEST_HIGHLIGHT_RING, cache_x - 44.0F, cache_y - 12.0F, 88.0F, 88.0F);
            sprite_fit_y_up(cache_claimed ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_CLAIM_CHECK : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GEM_GOLD,
                            cache_x - 26.0F, cache_y + 8.0F, 52.0F, 52.0F);
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_LOCK_OVERLAY, lock_x - 24.0F, lock_y + 10.0F, 48.0F, 48.0F);
        } else {
            circle(cache_x, cache_y + 28.0F, 44.0F, (float[4]){0.94F, 0.64F, 0.10F, 0.78F});
            rect(cache_x - 18.0F, cache_y + 16.0F, 36.0F, 36.0F, cache_claimed ? (float[4]){0.16F, 0.64F, 0.34F, 1.0F} : (float[4]){0.90F, 0.72F, 0.18F, 1.0F});
            rect(lock_x - 22.0F, lock_y + 10.0F, 44.0F, 44.0F, (float[4]){0.12F, 0.10F, 0.09F, 0.92F});
        }
        draw_text_panel(cache_claimed ? "CACHE TAKEN" : "EMBER CACHE", cache_x - 56.0F, cache_y + 76.0F, 1.7F);
        draw_text_panel("D2 LOCKED", lock_x - 42.0F, lock_y + 68.0F, 1.65F);
        if (cache_claimed && with_sprites) {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GEM_SMALL, cache_x + 38.0F, cache_y + 12.0F, 30.0F, 30.0F);
        }
    }
}

static void draw_town_forge_scene_anchors(UiBox scene, bool with_sprites) {
    if (g_game_state.location_index != GAME_STATE_LOCATION_OLD_GATE ||
        g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED ||
        !g_game_state.old_mine_cache_claimed) {
        return;
    }

    const UiBox forge = s_forge_workbench_box;
    const bool forged = g_game_state.gear_mine_lantern;
    const float lantern_x = forge.x + forge.w * 0.52F;
    const float lantern_y = forge.y + 44.0F;
    const float shard_x = forge.x + 42.0F;
    const float shard_y = forge.y + 40.0F;
    const float unlock_x = forge.x + forge.w - 58.0F;
    const float unlock_y = forge.y + 38.0F;

    if (with_sprites) {
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_FORGE_FLOOR_PATCH_V2,
                        forge.x - 92.0F, forge.y - 24.0F, forge.w + 184.0F, 88.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_FORGE_WORKSHOP_V2,
                        forge.x - 112.0F, forge.y + 6.0F, 250.0F, 184.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_FORGE_SIGNPOST_V2,
                        forge.x - 86.0F, forge.y + 26.0F, 50.0F, 118.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_QUEST_HIGHLIGHT_RING,
                        forge.x - 18.0F, forge.y + 8.0F, forge.w + 36.0F, forge.h - 4.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_FORGE_WORKTABLE_V2,
                        forge.x + 2.0F, forge.y + 2.0F, forge.w - 10.0F, forge.h + 38.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GEM_GOLD,
                        shard_x - 8.0F, shard_y + 34.0F, 38.0F, 38.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GEM_SMALL,
                        shard_x + 22.0F, shard_y + 44.0F, 24.0F, 24.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_MINE_LANTERN_STANDALONE_V2,
                        lantern_x - 32.0F, lantern_y + 26.0F, 56.0F, 100.0F);
        sprite_fit_y_up(forged ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_FORGE_LANTERN_READY_BADGE_V2 : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_LOCK_OVERLAY,
                        unlock_x - (forged ? 40.0F : 22.0F), unlock_y + (forged ? -4.0F : 8.0F), forged ? 80.0F : 44.0F, forged ? 82.0F : 44.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_EMBER_PARTICLES,
                        lantern_x - 44.0F, lantern_y + 110.0F, 88.0F, 58.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_FORGE_ACTION_PANEL_V2,
                        forge.x + 18.0F, forge.y + forge.h - 60.0F, 190.0F, 62.0F);
    } else {
        panel(forge, (float[4]){0.24F, 0.15F, 0.10F, 0.94F});
        circle(shard_x, shard_y + 24.0F, 22.0F, (float[4]){0.92F, 0.56F, 0.12F, 0.95F});
        rect(lantern_x - 20.0F, lantern_y + 18.0F, 40.0F, 56.0F, (float[4]){0.92F, 0.62F, 0.16F, 0.94F});
        circle(lantern_x, lantern_y + 46.0F, 30.0F, (float[4]){0.96F, 0.60F, 0.12F, 0.44F});
        rect(unlock_x - 20.0F, unlock_y + 12.0F, 40.0F, 40.0F, forged ? (float[4]){0.16F, 0.64F, 0.34F, 1.0F} : (float[4]){0.16F, 0.10F, 0.08F, 0.92F});
        rect(forge.x + 18.0F, forge.y + forge.h - 60.0F, 190.0F, 62.0F, (float[4]){0.58F, 0.42F, 0.22F, 0.96F});
    }

    draw_text_panel(forged ? "LANTERN READY" : "FORGE LANTERN", forge.x + 42.0F, forge.y + forge.h - 34.0F, 2.0F);

    const float mine_marker_x = s_map_mine_box.x + s_map_mine_box.w * 0.5F;
    const float mine_marker_y = s_map_mine_box.y + s_map_mine_box.h + 6.0F;
    line(unlock_x, unlock_y + 34.0F, mine_marker_x, mine_marker_y, forged ? (float[4]){0.94F, 0.64F, 0.16F, 0.92F} : (float[4]){0.62F, 0.42F, 0.16F, 0.58F});
    (void)scene;
}

static void draw_old_gate_scene(float w, float h) {
    (void)w;
    (void)h;
    const UiBox scene = s_scene_box;
    const bool road = g_game_state.location_index == GAME_STATE_LOCATION_NORTH_ROAD;
    const bool mine = g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE;
    const bool town_forge_scene = !mine && !road &&
                                  g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
                                  g_game_state.old_mine_cache_claimed;
    const char *scene_title = town_forge_scene ? "Old Gate Town Forge" : game_actions_location_title();
    const char *scene_subtitle =
        mine ? (g_game_state.old_mine_cache_claimed ? "OLD MINE CACHE RECOVERED" : (g_game_state.old_mine_depth_resolved ? "OLD MINE NEXT DELVE" : (g_game_state.old_mine_scouted ? "OLD MINE SCOUT REPORT" : "OLD MINE ENTRY"))) :
               (road ? "NORTH ROAD ENCOUNTER" : (town_forge_scene ? (g_game_state.gear_mine_lantern ? "MINE LANTERN EQUIPPED" : "MINE LANTERN UPGRADE") : "OLD GATE QUEST HUB"));
    panel(scene, road || mine ? (float[4]){0.16F, 0.24F, 0.16F, 1.0F} : (float[4]){0.19F, 0.16F, 0.20F, 1.0F});
    if (sprite_assets_ready()) {
        const UiBox art = {.x = scene.x + 18.0F, .y = scene.y + 22.0F, .w = scene.w - 36.0F, .h = scene.h - 44.0F};
        sprite_fit_y_up(mine ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_OLD_MINE_BACKDROP : (road ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_NORTH_ROAD_BACKDROP : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_OLD_GATE_BACKDROP), art.x, art.y, art.w, art.h);

        const float hero_x = scene.x + scene.w * (mine ? 0.48F : (road ? 0.34F : 0.25F));
        const float hero_y = scene.y + 142.0F;
        sprite_fit_y_up(road || mine ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_HERO_COMBAT : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_HERO_BACK, hero_x - 44.0F, hero_y, 88.0F, 128.0F);

        const float warden_x = scene.x + scene.w * 0.50F;
        const float warden_y = scene.y + 142.0F;
        if (!mine && !town_forge_scene) {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GATE_WARDEN_STANDING, warden_x - 46.0F, warden_y, 92.0F, 132.0F);
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_HANGING_BRAZIER, warden_x + 30.0F, warden_y + 92.0F, 46.0F, 58.0F);
        } else {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_TORCH_WALL, scene.x + scene.w * 0.36F, scene.y + 178.0F, 54.0F, 84.0F);
        }

        if (road || g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_ACCEPTED) {
            const float enemy_x = scene.x + scene.w * 0.62F;
            const float enemy_y = scene.y + 154.0F;
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_ROAD_WOLF_COMBAT, enemy_x - 58.0F, enemy_y, 132.0F, 92.0F);
        }

        if (!mine && (g_game_state.battle_state_index == GAME_STATE_BATTLE_STATE_VICTORY || g_game_state.battle_state_index == GAME_STATE_BATTLE_STATE_LOW_HEALTH)) {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_QUEST_HIGHLIGHT_RING, scene.x + scene.w * 0.58F - 42.0F, scene.y + 194.0F, 84.0F, 84.0F);
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_RING_REWARD, scene.x + scene.w * 0.58F - 24.0F, scene.y + 214.0F, 48.0F, 48.0F);
        }

        draw_mine_scene_anchors(scene, true);
        draw_town_forge_scene_anchors(scene, true);

        draw_text_panel(scene_title, scene.x + 28.0F, scene.y + scene.h - 38.0F, 4.0F);
        draw_text_panel(scene_subtitle, scene.x + 28.0F, scene.y + scene.h - 70.0F, 3.0F);
        if (!mine && !town_forge_scene) {
            draw_text_panel("GATE WARDEN", warden_x - 54.0F, warden_y + 8.0F, 2.0F);
        }
        return;
    }
    rect(scene.x + 18.0F, scene.y + 22.0F, scene.w - 36.0F, scene.h - 44.0F, road || mine ? (float[4]){0.30F, 0.42F, 0.23F, 1.0F} : (float[4]){0.32F, 0.24F, 0.26F, 1.0F});
    rect(scene.x + 18.0F, scene.y + scene.h * 0.63F, scene.w - 36.0F, scene.h * 0.24F, (float[4]){0.48F, 0.58F, 0.74F, 1.0F});
    rect(scene.x + 18.0F, scene.y + 22.0F, scene.w - 36.0F, 112.0F, (float[4]){0.23F, 0.18F, 0.12F, 1.0F});
    rect(scene.x + 64.0F, scene.y + 116.0F, scene.w * 0.54F, 38.0F, road ? (float[4]){0.44F, 0.34F, 0.18F, 1.0F} : (float[4]){0.60F, 0.52F, 0.38F, 1.0F});
    rect(scene.x + 92.0F, scene.y + scene.h * 0.42F, 170.0F, 118.0F, (float[4]){0.42F, 0.30F, 0.20F, 1.0F});
    rect(scene.x + 108.0F, scene.y + scene.h * 0.49F, 138.0F, 84.0F, (float[4]){0.22F, 0.15F, 0.10F, 1.0F});
    rect(scene.x + 148.0F, scene.y + scene.h * 0.54F, 58.0F, 76.0F, (float[4]){0.10F, 0.07F, 0.05F, 1.0F});
    rect(scene.x + scene.w * 0.36F, scene.y + scene.h * 0.52F, 130.0F, 92.0F, (float[4]){0.34F, 0.22F, 0.14F, 1.0F});
    rect(scene.x + scene.w * 0.39F, scene.y + scene.h * 0.58F, 72.0F, 64.0F, (float[4]){0.15F, 0.10F, 0.08F, 1.0F});
    circle(scene.x + scene.w * 0.30F, scene.y + scene.h * 0.37F, 54.0F, (float[4]){0.16F, 0.34F, 0.18F, 1.0F});
    circle(scene.x + scene.w * 0.48F, scene.y + scene.h * 0.34F, 48.0F, (float[4]){0.22F, 0.40F, 0.20F, 1.0F});

    const float hero_x = scene.x + scene.w * (road || mine ? 0.34F : 0.25F);
    const float hero_y = scene.y + 155.0F;
    circle(hero_x, hero_y + 78.0F, 24.0F, (float[4]){0.86F, 0.72F, 0.50F, 1.0F});
    rect(hero_x - 22.0F, hero_y + 28.0F, 44.0F, 58.0F, (float[4]){0.22F, 0.32F, 0.44F, 1.0F});
    rect(hero_x - 34.0F, hero_y + 22.0F, 68.0F, 10.0F, (float[4]){0.76F, 0.62F, 0.28F, 1.0F});

    const float warden_x = scene.x + scene.w * 0.50F;
    const float warden_y = scene.y + 164.0F;
    if (!town_forge_scene) {
        circle(warden_x, warden_y + 78.0F, 23.0F, (float[4]){0.72F, 0.58F, 0.42F, 1.0F});
        rect(warden_x - 24.0F, warden_y + 28.0F, 48.0F, 58.0F, (float[4]){0.40F, 0.18F, 0.12F, 1.0F});
        rect(warden_x - 38.0F, warden_y + 20.0F, 76.0F, 12.0F, (float[4]){0.88F, 0.55F, 0.20F, 1.0F});
        circle(warden_x + 34.0F, warden_y + 102.0F, 13.0F, (float[4]){0.90F, 0.72F, 0.20F, 1.0F});
    }

    if (road || g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_ACCEPTED) {
        const float enemy_x = scene.x + scene.w * 0.62F;
        const float enemy_y = scene.y + 168.0F;
        circle(enemy_x, enemy_y + 46.0F, 30.0F, (float[4]){0.36F, 0.36F, 0.38F, 1.0F});
        circle(enemy_x + 22.0F, enemy_y + 58.0F, 14.0F, (float[4]){0.38F, 0.38F, 0.40F, 1.0F});
        line(enemy_x - 22.0F, enemy_y + 30.0F, enemy_x - 46.0F, enemy_y + 12.0F, (float[4]){0.30F, 0.30F, 0.34F, 1.0F});
    }

    if (!mine && (g_game_state.battle_state_index == GAME_STATE_BATTLE_STATE_VICTORY || g_game_state.battle_state_index == GAME_STATE_BATTLE_STATE_LOW_HEALTH)) {
        circle(scene.x + scene.w * 0.58F, scene.y + 236.0F, 32.0F, (float[4]){0.94F, 0.76F, 0.24F, 0.82F});
        rect(scene.x + scene.w * 0.58F - 12.0F, scene.y + 224.0F, 24.0F, 24.0F, (float[4]){0.20F, 0.82F, 0.52F, 1.0F});
    }

    draw_mine_scene_anchors(scene, false);
    draw_town_forge_scene_anchors(scene, false);

    draw_text_panel(scene_title, scene.x + 28.0F, scene.y + scene.h - 38.0F, 4.0F);
    draw_text_panel(scene_subtitle, scene.x + 28.0F, scene.y + scene.h - 70.0F, 3.0F);
    if (!town_forge_scene) {
        draw_text_panel("GATE WARDEN", warden_x - 54.0F, warden_y + 8.0F, 2.0F);
    }
}

static void draw_hud(float w, float h) {
    const UiBox top = {.x = 0.0F, .y = h - 88.0F, .w = w, .h = 88.0F};
    if (sprite_assets_ready()) {
        slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_TOP_STATUS_FRAME, top.x, top.y, top.w, top.h);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_MEDALLION_GOLD, 22.0F, h - 78.0F, 68.0F, 68.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PORTRAIT_ROUND_FRAME, 28.0F, h - 72.0F, 56.0F, 56.0F);
    } else {
        rect(top.x, top.y, top.w, top.h, (float[4]){0.09F, 0.065F, 0.08F, 1.0F});
        rect(top.x, top.y, top.w, 6.0F, (float[4]){0.92F, 0.60F, 0.18F, 1.0F});
        circle(62.0F, h - 44.0F, 34.0F, (float[4]){0.72F, 0.50F, 0.26F, 1.0F});
        circle(62.0F, h - 44.0F, 24.0F, (float[4]){0.22F, 0.30F, 0.42F, 1.0F});
    }

    const float hp_frac = (float)g_game_state.hero_hp / (float)g_game_state.hero_hp_max;
    const float xp_frac = (float)(g_game_state.hero_xp % EMBER_LEVEL_2_XP) / (float)EMBER_LEVEL_2_XP;
    bar(112.0F, h - 40.0F, 210.0F, 16.0F, hp_frac, (float[4]){0.25F, 0.04F, 0.04F, 1.0F}, (float[4]){0.82F, 0.10F, 0.10F, 1.0F});
    bar(112.0F, h - 66.0F, 210.0F, 12.0F, xp_frac, (float[4]){0.05F, 0.12F, 0.25F, 1.0F}, (float[4]){0.18F, 0.48F, 0.94F, 1.0F});
    draw_text_panel("EMBER ROAD", 360.0F, h - 34.0F, 4.0F);
    char hero_line[96];
    (void)snprintf(hero_line, sizeof(hero_line), "LV %d  HP %d/%d  ATK %d  GOLD %d", g_game_state.hero_level, g_game_state.hero_hp, g_game_state.hero_hp_max, g_game_state.hero_attack, g_game_state.hero_gold);
    draw_text_panel(hero_line, 360.0F, h - 62.0F, 3.0F);
    draw_text_panel("HP", 112.0F, h - 20.0F, 2.2F);
    draw_text_panel("XP", 112.0F, h - 84.0F, 2.2F);

    for (int i = 0; i < 7; ++i) {
        const float action_hot[4] = {0.72F, 0.18F, 0.18F, 1.0F};
        const float action_idle[4] = {0.26F, 0.20F, 0.17F, 1.0F};
        circle(w - 290.0F + (float)i * 38.0F, h - 44.0F, 16.0F, i == 3 ? action_hot : action_idle);
    }
}

static void draw_route_strip(void) {
    const UiBox strip = {.x = s_scene_box.x + 44.0F, .y = s_scene_box.y + 24.0F, .w = s_quest_box.x - s_scene_box.x - 72.0F, .h = 116.0F};
    const bool completed = g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED;
    const bool scouted_mine = completed && g_game_state.old_mine_scouted;
    const bool depth_done = scouted_mine && g_game_state.old_mine_depth_resolved;
    const bool cache_claimed = depth_done && g_game_state.old_mine_cache_claimed;
    if (sprite_assets_ready()) {
        slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_ROUTE_STRIP_BASE, strip.x, strip.y, strip.w, strip.h);
    } else {
        panel(strip, (float[4]){0.64F, 0.52F, 0.34F, 1.0F});
        rect(strip.x + 20.0F, strip.y + strip.h - 38.0F, strip.w - 40.0F, 24.0F, (float[4]){0.32F, 0.18F, 0.12F, 1.0F});
    }
    line(s_map_town_box.x + s_map_town_box.w * 0.5F, s_map_town_box.y + s_map_town_box.h * 0.5F,
         s_map_road_box.x + s_map_road_box.w * 0.5F, s_map_road_box.y + s_map_road_box.h * 0.5F, (float[4]){0.72F, 0.54F, 0.20F, 1.0F});
    line(s_map_road_box.x + s_map_road_box.w * 0.5F, s_map_road_box.y + s_map_road_box.h * 0.5F,
         s_map_mine_box.x + s_map_mine_box.w * 0.5F, s_map_mine_box.y + s_map_mine_box.h * 0.5F, (float[4]){0.50F, 0.38F, 0.20F, 0.52F});
    const float node_active[4] = {0.16F, 0.54F, 0.36F, 1.0F};
    const float node_town_idle[4] = {0.42F, 0.30F, 0.20F, 1.0F};
    const float node_road_idle[4] = {0.35F, 0.42F, 0.20F, 1.0F};
    const float node_mine_open[4] = {0.30F, 0.38F, 0.30F, 1.0F};
    const float node_mine_locked[4] = {0.18F, 0.15F, 0.14F, 0.72F};
    if (sprite_assets_ready()) {
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_ROUTE_PLAQUE_FRAME, s_map_town_box.x, s_map_town_box.y, s_map_town_box.w, s_map_town_box.h);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_ROUTE_PLAQUE_FRAME, s_map_road_box.x, s_map_road_box.y, s_map_road_box.w, s_map_road_box.h);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_ROUTE_PLAQUE_FRAME, s_map_mine_box.x, s_map_mine_box.y, s_map_mine_box.w, s_map_mine_box.h);
        if (g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE) {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_QUEST_MARKER, s_map_town_box.x + 60.0F, s_map_town_box.y + 8.0F, 32.0F, 32.0F);
        }
        if (g_game_state.location_index == GAME_STATE_LOCATION_NORTH_ROAD) {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_WOLF_MARKER, s_map_road_box.x + 64.0F, s_map_road_box.y + 10.0F, 34.0F, 34.0F);
        }
        if (g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE) {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_QUEST_MARKER, s_map_mine_box.x + 60.0F, s_map_mine_box.y + 8.0F, 32.0F, 32.0F);
        }
        if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED) {
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_LOCK_OVERLAY, s_map_mine_box.x + 62.0F, s_map_mine_box.y + 10.0F, 30.0F, 30.0F);
        }
    } else {
        panel(s_map_town_box, g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE ? node_active : node_town_idle);
        panel(s_map_road_box, g_game_state.location_index == GAME_STATE_LOCATION_NORTH_ROAD ? node_active : node_road_idle);
        panel(s_map_mine_box, g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE ? node_active : (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED ? node_mine_open : node_mine_locked));
    }
    draw_text_panel("ROUTE", strip.x + 34.0F, strip.y + strip.h - 22.0F, 3.0F);
    draw_text_panel("GATE", s_map_town_box.x + 36.0F, s_map_town_box.y + s_map_town_box.h - 15.0F, 2.2F);
    draw_text_panel("ROAD", s_map_road_box.x + 46.0F, s_map_road_box.y + s_map_road_box.h - 16.0F, 2.2F);
    draw_text_panel("MINE", s_map_mine_box.x + 40.0F, s_map_mine_box.y + s_map_mine_box.h - 16.0F, 2.2F);
    draw_text_panel(g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE ? "HERE" : "TOWN",
                    s_map_town_box.x + 42.0F, s_map_town_box.y + 8.0F, 1.75F);
    draw_text_panel(g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_WOLF_DEFEATED || completed ? "CLEAR" : (g_game_state.location_index == GAME_STATE_LOCATION_NORTH_ROAD ? "FIGHT" : "WOLF"),
                    s_map_road_box.x + 44.0F, s_map_road_box.y + 8.0F, 1.75F);
    draw_text_panel(g_game_state.old_mine_depth2_unlocked ? "D2 OPEN" : (cache_claimed ? (g_game_state.gear_mine_lantern ? "LANTERN" : "FORGE") : (depth_done ? "DELVE" : (scouted_mine ? "DEPTH 1" : (completed ? "OPEN" : "LV 2")))), s_map_mine_box.x + 32.0F, s_map_mine_box.y + 8.0F, 1.75F);
}

static void draw_bottom_log(float w) {
    const UiBox log = {.x = 116.0F, .y = 18.0F, .w = w - 232.0F, .h = 76.0F};
    if (sprite_assets_ready()) {
        slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_WIDE_PANEL, log.x, log.y, log.w, log.h);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_QUEST_MARKER, log.x + 18.0F, log.y + 20.0F, 38.0F, 38.0F);
    } else {
        panel(log, (float[4]){0.16F, 0.12F, 0.10F, 0.96F});
        rect(log.x + 18.0F, log.y + 20.0F, 38.0F, 38.0F, (float[4]){0.72F, 0.50F, 0.16F, 1.0F});
    }

    const bool mine = g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE;
    const bool scouted = mine && g_game_state.old_mine_scouted;
    const bool depth_done = scouted && g_game_state.old_mine_depth_resolved;
    const bool cache_claimed = depth_done && g_game_state.old_mine_cache_claimed;
    if (cache_claimed) {
        draw_text_panel("DELVE REWARD  First ember cache recovered.", log.x + 70.0F, log.y + 54.0F, 1.95F);
        draw_text_panel("+1 shard, +2 gold, +3 XP. Next depth locked.", log.x + 70.0F, log.y + 30.0F, 1.85F);
        return;
    }
    if (depth_done) {
        draw_text_panel("NEXT CHOICE  Cave Bat defeated. Cache visible.", log.x + 70.0F, log.y + 54.0F, 1.95F);
        draw_text_panel("Delve cache for +1 shard or return to Old Gate.", log.x + 70.0F, log.y + 30.0F, 1.85F);
        return;
    }
    if (scouted) {
        draw_text_panel("REPORT  D1 mapped. Bat signs ahead.", log.x + 70.0F, log.y + 54.0F, 1.95F);
        draw_text_panel("Found +3 ember shards. Route Gate > Road > Mine.", log.x + 70.0F, log.y + 30.0F, 1.85F);
        return;
    }
    if (mine) {
        draw_text_panel("LOG  Old Mine entrance reached. Scout before going deeper.", log.x + 70.0F, log.y + 54.0F, 1.95F);
        draw_text_panel("Route open: Gate > Road > Mine. Back remains safe.", log.x + 70.0F, log.y + 30.0F, 1.85F);
        return;
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED) {
        if (g_game_state.gear_mine_lantern) {
            draw_text_panel("GEAR READY  Mine Lantern forged.", log.x + 70.0F, log.y + 54.0F, 1.95F);
            draw_text_panel("Depth 2 route is lit for the next expedition.", log.x + 70.0F, log.y + 30.0F, 1.85F);
            return;
        }
        if (g_game_state.old_mine_cache_claimed) {
            draw_text_panel("TOWN FORGE  First ember cache returned.", log.x + 70.0F, log.y + 54.0F, 1.95F);
            draw_text_panel("Spend 6 shards to forge the Mine Lantern.", log.x + 70.0F, log.y + 30.0F, 1.85F);
            return;
        }
        draw_text_panel("LOG  Wolf quest complete. Old Mine route is open.", log.x + 70.0F, log.y + 54.0F, 1.95F);
        draw_text_panel("Level 2 reached. Enter the mine from the route strip.", log.x + 70.0F, log.y + 30.0F, 1.85F);
        return;
    }
    draw_text_panel(game_actions_objective_text(), log.x + 70.0F, log.y + 54.0F, 1.95F);
    draw_text_panel(game_actions_battle_text(), log.x + 70.0F, log.y + 30.0F, 1.85F);
}

static void draw_quest_focus(void) {
    const UiBox side = s_quest_box;
    const bool mine_entry = g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED && g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE;
    const bool mine_cache_ready = mine_entry && g_game_state.old_mine_depth_resolved && !g_game_state.old_mine_cache_claimed;
    const bool mine_cache_claimed = mine_entry && g_game_state.old_mine_cache_claimed;
    const bool town_mine_upgrade = !mine_entry && g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED && g_game_state.old_mine_cache_claimed;
    if (sprite_assets_ready()) {
        slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_QUEST_RAIL_PANEL, side.x, side.y, side.w, side.h);
        slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_WIDE_PANEL, side.x + 18.0F, side.y + side.h - 64.0F, side.w - 36.0F, 42.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PORTRAIT_ROUND_FRAME, side.x + 20.0F, side.y + side.h - 140.0F, 76.0F, 76.0F);
        if (town_mine_upgrade) {
            sprite_fit_y_up(g_game_state.gear_mine_lantern ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_FORGE_LANTERN_READY_BADGE_V2 : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_MINE_LANTERN_STANDALONE_V2,
                            side.x + (g_game_state.gear_mine_lantern ? 26.0F : 34.0F), side.y + side.h - 136.0F, g_game_state.gear_mine_lantern ? 64.0F : 44.0F, 72.0F);
        } else {
            sprite_fit_y_up(mine_entry ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_LOCKED_MINE : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GATE_WARDEN_PORTRAIT,
                            side.x + 28.0F, side.y + side.h - 132.0F, 60.0F, 60.0F);
        }
    } else {
        panel(side, (float[4]){0.78F, 0.66F, 0.48F, 1.0F});
        rect(side.x + 18.0F, side.y + side.h - 64.0F, side.w - 36.0F, 42.0F, (float[4]){0.46F, 0.12F, 0.10F, 1.0F});
        circle(side.x + 54.0F, side.y + side.h - 104.0F, 34.0F, (float[4]){0.68F, 0.48F, 0.30F, 1.0F});
        rect(side.x + 34.0F, side.y + side.h - 142.0F, 40.0F, 48.0F, (float[4]){0.36F, 0.15F, 0.10F, 1.0F});
    }
    draw_text_panel(mine_entry ? "OLD MINE ROUTE" : (town_mine_upgrade ? "MINE LANTERN" : "WOLVES AT NORTH ROAD"), side.x + 34.0F, side.y + side.h - 44.0F, 2.6F);
    draw_text_panel(mine_entry ? (mine_cache_claimed ? "Cache Recovered" : (mine_cache_ready ? "Cache Vein" : "Mine Entrance")) : (town_mine_upgrade ? "Town Forge" : "Gate Warden"), side.x + 104.0F, side.y + side.h - 94.0F, 2.6F);
    draw_text_panel(mine_entry ? (mine_cache_claimed ? "Return with proof" : (mine_cache_ready ? "Delve or return" : "Scout or return")) : (town_mine_upgrade ? (g_game_state.gear_mine_lantern ? "Route lit" : "Depth 2 key") : "Return with proof"), side.x + 104.0F, side.y + side.h - 124.0F, 2.1F);

    if (mine_entry) {
        return;
    }

    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED) {
        const UiBox progress = {.x = side.x + 30.0F, .y = side.y + 104.0F, .w = side.w - 60.0F, .h = 188.0F};
        const bool lantern_ready = g_game_state.old_mine_cache_claimed;
        if (sprite_assets_ready()) {
            slice9_y_up(town_mine_upgrade ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_FORGE_RESULT_STRIP_SLICE9_V2 : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_WIDE_PANEL,
                        progress.x, progress.y + (town_mine_upgrade ? 52.0F : 0.0F), progress.w, town_mine_upgrade ? 124.0F : progress.h);
            if (town_mine_upgrade) {
                sprite_fit_y_up(g_game_state.gear_mine_lantern ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_FORGE_LANTERN_READY_BADGE_V2 : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_MINE_LANTERN_STANDALONE_V2,
                                progress.x + 18.0F, progress.y + 74.0F, g_game_state.gear_mine_lantern ? 64.0F : 48.0F, 90.0F);
                sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GEM_GOLD, progress.x + 28.0F, progress.y + 30.0F, 32.0F, 32.0F);
            } else {
                sprite_fit_y_up(g_game_state.gear_mine_lantern ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_CLAIM_CHECK : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_LOCKED_MINE,
                                progress.x + 18.0F, progress.y + 118.0F, 48.0F, 48.0F);
                sprite_fit_y_up(lantern_ready ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GEM_GOLD : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_XP_SPARK,
                                progress.x + 24.0F, progress.y + 62.0F, 34.0F, 34.0F);
                sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GOLD_COIN, progress.x + 24.0F, progress.y + 26.0F, 34.0F, 34.0F);
                sprite_fit_y_up(g_game_state.gear_mine_lantern ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_TORCH_WALL : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GEM_SMALL,
                                progress.x + progress.w - 68.0F, progress.y + 22.0F, 42.0F, 54.0F);
            }
        } else {
            panel(progress, (float[4]){0.20F, 0.16F, 0.12F, 1.0F});
            circle(progress.x + 42.0F, progress.y + 142.0F, 22.0F, (float[4]){0.30F, 0.64F, 0.34F, 1.0F});
        }
        if (town_mine_upgrade) {
            draw_text_panel(g_game_state.gear_mine_lantern ? "LANTERN READY" : "FORGE NOW", progress.x + 86.0F, progress.y + 154.0F, 2.45F);
            draw_text_panel(g_game_state.gear_mine_lantern ? "DEPTH 2 ROUTE LIT" : "6 SHARDS -> DEPTH 2", progress.x + 86.0F, progress.y + 124.0F, 1.95F);
            char shard_line[96];
            (void)snprintf(shard_line, sizeof(shard_line), g_game_state.gear_mine_lantern ? "SHARDS LEFT %d" : "HAVE %d SHARDS", g_game_state.old_mine_ember_shards);
            draw_text_panel(shard_line, progress.x + 86.0F, progress.y + 92.0F, 1.85F);
            draw_text_panel(g_game_state.gear_mine_lantern ? "EQUIPPED" : "EMBER CACHE", progress.x + 70.0F, progress.y + 34.0F, 1.9F);
        } else if (lantern_ready) {
            draw_text_panel(g_game_state.gear_mine_lantern ? "LANTERN READY" : "MINE LANTERN", progress.x + 76.0F, progress.y + 160.0F, 2.3F);
            draw_text_panel(g_game_state.gear_mine_lantern ? "DEPTH 2 LIT" : "FORGE AT TOWN", progress.x + 76.0F, progress.y + 132.0F, 2.4F);
            char shard_line[96];
            (void)snprintf(shard_line, sizeof(shard_line), g_game_state.gear_mine_lantern ? "SHARDS LEFT %d" : "COST 6 SHARDS  HAVE %d", g_game_state.old_mine_ember_shards);
            draw_text_panel(shard_line, progress.x + 76.0F, progress.y + 100.0F, 1.95F);
            draw_text_panel(g_game_state.gear_mine_lantern ? "NEXT: DEPTH 2" : "UNLOCK DEPTH 2", progress.x + 66.0F, progress.y + 56.0F, 2.1F);
            draw_text_panel(g_game_state.gear_mine_lantern ? "GEAR EQUIPPED" : "CACHE PROOF", progress.x + 66.0F, progress.y + 28.0F, 2.0F);
        } else {
            draw_text_panel("QUEST COMPLETE", progress.x + 76.0F, progress.y + 160.0F, 2.3F);
            draw_text_panel("LEVEL 2 REACHED", progress.x + 76.0F, progress.y + 132.0F, 2.4F);
            char stats_line[96];
            (void)snprintf(stats_line, sizeof(stats_line), "HP %d/%d   ATK %d", g_game_state.hero_hp, g_game_state.hero_hp_max, g_game_state.hero_attack);
            draw_text_panel(stats_line, progress.x + 76.0F, progress.y + 100.0F, 2.0F);
            char gold_line[64];
            (void)snprintf(gold_line, sizeof(gold_line), "GOLD %d", g_game_state.hero_gold);
            draw_text_panel(gold_line, progress.x + 66.0F, progress.y + 56.0F, 2.1F);
            draw_text_panel(g_game_state.gear_ring_equipped ? "RING EQUIPPED" : "RING KEPT", progress.x + 66.0F, progress.y + 28.0F, 2.0F);
            draw_text_panel("MINE ROUTE OPEN", progress.x + 76.0F, progress.y + 78.0F, 2.0F);
        }
        return;
    }

    if (sprite_assets_ready()) {
        slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_SMALL_PANEL, s_battle_preview_box.x, s_battle_preview_box.y, s_battle_preview_box.w, s_battle_preview_box.h);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_ROAD_WOLF_SIDE, s_battle_preview_box.x + 18.0F, s_battle_preview_box.y + 18.0F, 82.0F, 68.0F);
        sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_SWORD_AUTO_BATTLE, s_battle_preview_box.x + s_battle_preview_box.w - 54.0F, s_battle_preview_box.y + 54.0F, 34.0F, 34.0F);
    } else {
        panel(s_battle_preview_box, (float[4]){0.20F, 0.15F, 0.12F, 1.0F});
        circle(s_battle_preview_box.x + 54.0F, s_battle_preview_box.y + 58.0F, 27.0F, (float[4]){0.34F, 0.34F, 0.36F, 1.0F});
        circle(s_battle_preview_box.x + 82.0F, s_battle_preview_box.y + 70.0F, 12.0F, (float[4]){0.40F, 0.40F, 0.42F, 1.0F});
    }
    draw_text_panel("AUTO BATTLE", s_battle_preview_box.x + 106.0F, s_battle_preview_box.y + 78.0F, 2.0F);
    draw_text_panel("Wolf  HP 18", s_battle_preview_box.x + 106.0F, s_battle_preview_box.y + 52.0F, 1.9F);
    draw_text_panel("XP  GOLD  RING", s_battle_preview_box.x + 106.0F, s_battle_preview_box.y + 28.0F, 1.8F);

    char quest_line[64];
    (void)snprintf(quest_line, sizeof(quest_line), "WOLF QUEST %d/1", g_game_state.quest_wolf_kills);
    if (g_game_state.reward_item_ready) {
        if (sprite_assets_ready()) {
            slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_REWARD_SLOT_A, side.x + 34.0F, side.y + 116.0F, 66.0F, 66.0F);
            sprite_fit_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_RING_REWARD, side.x + 46.0F, side.y + 128.0F, 42.0F, 42.0F);
        } else {
            rect(side.x + 34.0F, side.y + 116.0F, 66.0F, 66.0F, (float[4]){0.08F, 0.32F, 0.22F, 1.0F});
            circle(side.x + 67.0F, side.y + 149.0F, 20.0F, (float[4]){0.52F, 0.90F, 0.70F, 1.0F});
        }
        draw_text_panel("LOOT READY", side.x + 112.0F, side.y + 168.0F, 2.0F);
        draw_text_panel("IRON RING", side.x + 112.0F, side.y + 144.0F, 2.2F);
        draw_text_panel("+1 ATK", side.x + 112.0F, side.y + 120.0F, 2.0F);
    } else {
        bar(side.x + 28.0F, side.y + 112.0F, side.w - 56.0F, 18.0F, (float)g_game_state.quest_wolf_kills, (float[4]){0.20F, 0.14F, 0.12F, 1.0F}, (float[4]){0.86F, 0.62F, 0.20F, 1.0F});
        draw_text_panel(quest_line, side.x + 30.0F, side.y + 148.0F, 2.0F);
    }
}

static void draw_controls(void) {
    const bool completed = g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED;
    const bool mine_entry = completed && g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE;
    if ((!g_game_state.reward_item_ready || completed) && !mine_entry) {
        if (sprite_assets_ready()) {
            slice9_y_up(strcmp(game_actions_primary_action_id(), "ember.completed") == 0 ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PRIMARY_BUTTON_DISABLED : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PRIMARY_BUTTON_DEFAULT,
                        s_primary_box.x, s_primary_box.y, s_primary_box.w, s_primary_box.h);
        } else {
            panel(s_primary_box, (float[4]){0.60F, 0.16F, 0.10F, 1.0F});
            rect(s_primary_box.x + 22.0F, s_primary_box.y + 19.0F, s_primary_box.w - 44.0F, 10.0F, (float[4]){1.0F, 0.80F, 0.38F, 0.70F});
        }
        const char *primary_label = game_actions_primary_action_label();
        const bool long_primary_label = strlen(primary_label) >= 13U;
        draw_text_panel(primary_label, s_primary_box.x + (long_primary_label ? 44.0F : 20.0F), s_primary_box.y + (long_primary_label ? 36.0F : 42.0F), long_primary_label ? 2.2F : 3.0F);
    }
    if (g_game_state.reward_item_ready && !completed) {
        const float equip_done[4] = {0.22F, 0.34F, 0.24F, 1.0F};
        const float equip_ready[4] = {0.18F, 0.48F, 0.34F, 1.0F};
        if (sprite_assets_ready()) {
            slice9_y_up(g_game_state.gear_ring_equipped ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PRIMARY_BUTTON_SELECTED : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PRIMARY_BUTTON_DEFAULT,
                        s_equip_box.x, s_equip_box.y, s_equip_box.w, s_equip_box.h);
        } else {
            panel(s_equip_box, g_game_state.gear_ring_equipped ? equip_done : equip_ready);
        }
        draw_text_panel(g_game_state.gear_ring_equipped ? "RING ON" : "EQUIP", s_equip_box.x + 18.0F, s_equip_box.y + 34.0F, 2.6F);
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_WOLF_DEFEATED) {
        if (sprite_assets_ready()) {
            slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PRIMARY_BUTTON_SELECTED, s_claim_box.x, s_claim_box.y, s_claim_box.w, s_claim_box.h);
        } else {
            panel(s_claim_box, (float[4]){0.62F, 0.40F, 0.10F, 1.0F});
        }
        draw_text_panel("CLAIM", s_claim_box.x + 18.0F, s_claim_box.y + 34.0F, 2.6F);
    }
}

static void draw_mine_choice_surface(void) {
    if (g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED || g_game_state.location_index != GAME_STATE_LOCATION_OLD_MINE) {
        return;
    }
    const bool scouted = g_game_state.old_mine_scouted;
    const bool depth_done = g_game_state.old_mine_depth_resolved;
    const bool cache_claimed = g_game_state.old_mine_cache_claimed;
    const bool encounter_ready = scouted && !depth_done;
    if (sprite_assets_ready()) {
        slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_WIDE_PANEL, s_mine_choice_box.x, s_mine_choice_box.y, s_mine_choice_box.w, s_mine_choice_box.h);
        slice9_y_up(cache_claimed ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PRIMARY_BUTTON_SELECTED : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PRIMARY_BUTTON_DEFAULT,
                    s_mine_scout_box.x, s_mine_scout_box.y, s_mine_scout_box.w, s_mine_scout_box.h);
        slice9_y_up(ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_PRIMARY_BUTTON_DEFAULT,
                    s_mine_back_box.x, s_mine_back_box.y, s_mine_back_box.w, s_mine_back_box.h);
        sprite_fit_y_up(scouted ? ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_GOLD_COIN : ASSET_ATLAS_REGION_EMBER_ROAD_OLD_GATE_ATLAS_XP_SPARK,
                        s_mine_choice_box.x + 24.0F, s_mine_choice_box.y + s_mine_choice_box.h - 72.0F, 44.0F, 44.0F);
    } else {
        panel(s_mine_choice_box, (float[4]){0.18F, 0.15F, 0.12F, 0.96F});
        panel(s_mine_scout_box, depth_done ? (float[4]){0.18F, 0.48F, 0.34F, 1.0F} : (float[4]){0.62F, 0.40F, 0.10F, 1.0F});
        panel(s_mine_back_box, (float[4]){0.58F, 0.18F, 0.12F, 1.0F});
    }
    draw_text_panel(cache_claimed ? "CACHE RECOVERED" : (depth_done ? "NEXT DELVE CHOICE" : (scouted ? "SCOUT REPORT" : "OLD MINE ENTRANCE")), s_mine_choice_box.x + 78.0F, s_mine_choice_box.y + s_mine_choice_box.h - 44.0F, 2.7F);
    draw_text_panel(cache_claimed ? "First cache claimed. Depth 2 remains locked." : (depth_done ? "Depth 1 clear. Ember cache is visible." : (scouted ? "D1 mapped: Bat signs, +3 shards." : "Scout the threshold before going deeper.")),
                    s_mine_choice_box.x + 34.0F, s_mine_choice_box.y + s_mine_choice_box.h - 80.0F, 1.85F);
    draw_text_panel(cache_claimed ? "CLAIMED" : (depth_done ? "DELVE" : (encounter_ready ? "CLEAR" : "SCOUT")), s_mine_scout_box.x + 20.0F, s_mine_scout_box.y + 34.0F, 2.25F);
    draw_text_panel(cache_claimed ? "+1 SHARD" : (depth_done ? "CACHE" : (encounter_ready ? "CAVE BAT" : "ENTRANCE")), s_mine_scout_box.x + 20.0F, s_mine_scout_box.y + 14.0F, 1.65F);
    draw_text_panel("BACK", s_mine_back_box.x + 38.0F, s_mine_back_box.y + 34.0F, 2.4F);
    draw_text_panel("OLD GATE", s_mine_back_box.x + 28.0F, s_mine_back_box.y + 14.0F, 1.7F);
}

static void draw_scene(float w, float h) {
    s_view_h = h;
    float vp[16];
    ortho(0.0F, w, h, 0.0F, -1.0F, 1.0F, vp);
    nt_shape_renderer_set_vp(vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(3.0F);

    rect(0.0F, 0.0F, w, h, (float[4]){0.08F, 0.07F, 0.08F, 1.0F});
    draw_hud(w, h);
    draw_old_gate_scene(w, h);
    draw_route_strip();
    draw_quest_focus();
    draw_controls();
    draw_mine_choice_surface();
    draw_bottom_log(w);

    if (g_game_state.location_index != GAME_STATE_LOCATION_OLD_MINE &&
        (g_game_state.battle_state_index == GAME_STATE_BATTLE_STATE_VICTORY || g_game_state.battle_state_index == GAME_STATE_BATTLE_STATE_LOW_HEALTH)) {
        rect(s_scene_box.x + 250.0F, 36.0F, 390.0F, 54.0F, (float[4]){0.15F, 0.38F, 0.24F, 0.86F});
        rect_wire(s_scene_box.x + 250.0F, 36.0F, 390.0F, 54.0F, (float[4]){0.86F, 0.76F, 0.32F, 1.0F});
        draw_text_panel("VICTORY  LOOT READY", s_scene_box.x + 276.0F, 68.0F, 3.0F);
    }
}

static void init_text_assets(void) {
    nt_gfx_register_global_block("Globals", 0);
    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture);
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);
    nt_atlas_init();
    nt_font_init(&(nt_font_desc_t){.max_fonts = 4});
    nt_material_init(&(nt_material_desc_t){.max_materials = 8});
    nt_sprite_renderer_init(&(nt_sprite_renderer_desc_t){.max_pipelines = 16});
    nt_text_renderer_init();

    s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "ember_road_globals",
    });

    s_asset_pack_id = nt_hash32_str("ember_road_base");
    (void)nt_resource_mount(s_asset_pack_id, 100);
    (void)nt_resource_load_auto(s_asset_pack_id, EMBER_ROAD_BASE_PACK_PATH);

    nt_resource_t vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_VERT, NT_ASSET_SHADER_CODE);
    nt_resource_t fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SLUG_TEXT_FRAG, NT_ASSET_SHADER_CODE);
    nt_resource_t sprite_vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_VERT, NT_ASSET_SHADER_CODE);
    nt_resource_t sprite_fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_FRAG, NT_ASSET_SHADER_CODE);
    s_old_gate_atlas_resource = nt_resource_request(ASSET_ATLAS_EMBER_ROAD_OLD_GATE_ATLAS, NT_ASSET_ATLAS);
    s_old_gate_atlas_texture0_resource = nt_resource_request(ASSET_TEXTURE_EMBER_ROAD_OLD_GATE_ATLAS_TEX0, NT_ASSET_TEXTURE);
    s_sprite_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = sprite_vs,
        .fs = sprite_fs,
        .textures = {{.name = "u_texture", .resource = s_old_gate_atlas_texture0_resource}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "ember_road_old_gate_sprites",
    });
    s_text_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = vs,
        .fs = fs,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .params[0] = {.name = "u_alpha_cutoff", .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
        .param_count = 1,
        .label = "ember_road_slug_text",
    });

    s_ui_font = nt_font_create(&(nt_font_create_desc_t){
        .curve_texture_width = 1024,
        .curve_texture_height = 512,
        .band_texture_height = 256,
        .band_count = 8,
        .measure_cache_size = 256,
    });
    s_ui_font_resource = nt_resource_request(ASSET_FONT_EMBER_ROAD_FONT_UI, NT_ASSET_FONT);
    nt_font_add(s_ui_font, s_ui_font_resource);
    nt_resource_set_activate_time_budget(0);
}

static void step_text_assets(void) {
    nt_resource_step();
    nt_material_step();
    nt_font_step();
}

static void update_text_globals(float w, float h) {
    nt_frame_uniforms_t uniforms = {0};
    ortho(0.0F, w, 0.0F, h, -1.0F, 1.0F, uniforms.view_proj);
    memcpy(uniforms.view, (float[16]){
                              1.0F, 0.0F, 0.0F, 0.0F,
                              0.0F, 1.0F, 0.0F, 0.0F,
                              0.0F, 0.0F, 1.0F, 0.0F,
                              0.0F, 0.0F, 0.0F, 1.0F,
                          },
           sizeof(float) * 16);
    memcpy(uniforms.proj, uniforms.view_proj, sizeof(float) * 16);
    uniforms.camera_pos[2] = 1.0F;
    uniforms.time[0] = 0.0F;
    uniforms.resolution[0] = w;
    uniforms.resolution[1] = h;
    uniforms.resolution[2] = w > 0.0F ? 1.0F / w : 0.0F;
    uniforms.resolution[3] = h > 0.0F ? 1.0F / h : 0.0F;
    uniforms.near_far[0] = -1.0F;
    uniforms.near_far[1] = 1.0F;
    nt_gfx_update_buffer(s_frame_ubo, &uniforms, sizeof(uniforms));
    nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);
}

static void shutdown_text_assets(void) {
    nt_text_renderer_shutdown();
    nt_sprite_renderer_shutdown();
    if (s_ui_font.id != 0) {
        nt_font_destroy(s_ui_font);
    }
    nt_font_shutdown();
    if (s_sprite_material.id != 0) {
        nt_material_destroy(s_sprite_material);
    }
    if (s_text_material.id != 0) {
        nt_material_destroy(s_text_material);
    }
    nt_material_shutdown();
    nt_resource_shutdown();
    nt_fs_shutdown();
    nt_http_shutdown();
    nt_hash_shutdown();
    if (s_frame_ubo.id != 0) {
        nt_gfx_destroy_buffer(s_frame_ubo);
    }
}

static void handle_input(void) {
    if (nt_input_key_is_pressed(NT_KEY_SPACE) || nt_input_key_is_pressed(NT_KEY_ENTER)) {
        (void)invoke_primary_action();
    }
    if (!nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        return;
    }
    for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
        const nt_pointer_t pointer = g_nt_input.pointers[i];
        if (!pointer.active) {
            continue;
        }
        const float y_up = s_view_h - pointer.y;
        if (contains_y_up(s_mine_back_box, pointer.x, y_up) &&
            g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
            g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE) {
            (void)invoke_action(game_actions_return_old_gate);
            return;
        }
        if (contains_y_up(s_mine_scout_box, pointer.x, y_up) &&
            g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
            g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE) {
            if (!g_game_state.old_mine_scouted) {
                (void)invoke_action(game_actions_scout_old_mine);
            } else if (!g_game_state.old_mine_depth_resolved) {
                (void)invoke_action(game_actions_resolve_old_mine_depth);
            } else if (!g_game_state.old_mine_cache_claimed) {
                (void)invoke_action(game_actions_delve_old_mine);
            } else {
                set_message("First ember cache is already recovered.");
            }
            return;
        }
        if (contains_y_up(s_forge_workbench_box, pointer.x, y_up) &&
            g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
            g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE &&
            g_game_state.old_mine_cache_claimed) {
            if (!g_game_state.gear_mine_lantern) {
                (void)invoke_action(game_actions_forge_mine_lantern);
            } else {
                set_message("Mine Lantern is already equipped; Depth 2 is lit.");
            }
            return;
        }
        if (contains_y_up(s_primary_box, pointer.x, y_up)) {
            (void)invoke_primary_action();
            return;
        }
        if (contains_y_up(s_map_mine_box, pointer.x, y_up)) {
            (void)invoke_action(game_actions_enter_old_mine);
            return;
        }
        if (contains_y_up(s_map_road_box, pointer.x, y_up)) {
            (void)invoke_action(game_actions_travel_north_road);
            return;
        }
        if (contains_y_up(s_equip_box, pointer.x, y_up)) {
            (void)invoke_action(game_actions_equip_ring);
            return;
        }
        if (contains_y_up(s_claim_box, pointer.x, y_up)) {
            (void)invoke_action(game_actions_claim_reward);
            return;
        }
    }
}

#if NT_DEVAPI_ENABLED
void game_state_register_devapi(void);

static cJSON *state_json(void) {
    cJSON *root = game_state_to_json(&g_game_state);
    cJSON_AddStringToObject(root, "runtime", "ember_road");
    cJSON_AddStringToObject(root, "location", game_state_location_name(g_game_state.location_index));
    cJSON_AddStringToObject(root, "quest_stage", game_state_quest_stage_name(g_game_state.quest_stage_index));
    cJSON_AddStringToObject(root, "battle_state", game_state_battle_state_name(g_game_state.battle_state_index));
    cJSON_AddStringToObject(root, "primary_action_id", game_actions_primary_action_id());
    cJSON_AddStringToObject(root, "primary_action_label", game_actions_primary_action_label());
    cJSON_AddStringToObject(root, "objective", game_actions_objective_text());
    cJSON_AddStringToObject(root, "location_title", game_actions_location_title());
    cJSON_AddStringToObject(root, "battle_text", game_actions_battle_text());
    cJSON_AddStringToObject(root, "last_message", s_last_message);
    cJSON_AddBoolToObject(root, "modal_or_choice_open",
                          g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
                              g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE);
    cJSON_AddBoolToObject(root, "old_mine_scout_result_open",
                          g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
                              g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE &&
                              g_game_state.old_mine_scouted);
    cJSON_AddBoolToObject(root, "old_mine_depth_encounter_open",
                          g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
                              g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE &&
                              g_game_state.old_mine_depth_resolved);
    cJSON_AddBoolToObject(root, "old_mine_next_delve_choice_open",
                          g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
                              g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE &&
                              g_game_state.old_mine_depth_resolved &&
                              !g_game_state.old_mine_cache_claimed);
    cJSON_AddBoolToObject(root, "old_mine_delve_reward_open",
                          g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
                              g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE &&
                              g_game_state.old_mine_cache_claimed);
    cJSON_AddBoolToObject(root, "town_lantern_upgrade_open",
                          g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
                              g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE &&
                              g_game_state.old_mine_cache_claimed &&
                              !g_game_state.gear_mine_lantern);
    cJSON_AddBoolToObject(root, "town_lantern_forged_open",
                          g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
                              g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE &&
                              g_game_state.old_mine_cache_claimed &&
                              g_game_state.gear_mine_lantern);
    cJSON_AddBoolToObject(root, "old_mine_depth2_unlocked",
                          g_game_state.old_mine_depth2_unlocked);
    return root;
}

static bool state_emit(cJSON *result_obj, cJSON *src) {
    cJSON *child = src ? src->child : NULL;
    while (child) {
        cJSON *next = child->next;
        cJSON_DetachItemViaPointer(src, child);
        cJSON_AddItemToObject(result_obj, child->string, child);
        child = next;
    }
    cJSON_Delete(src);
    return true;
}

static bool action_emit(cJSON *result_obj, GameActionResult result, const char *message) {
    cJSON_AddStringToObject(result_obj, "action_result", result == GAME_ACTION_OK ? "ok" : "blocked");
    cJSON_AddStringToObject(result_obj, "message", message ? message : "");
    return state_emit(result_obj, state_json());
}

static bool ep_game_state(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    return state_emit(result_obj, state_json());
}

static bool ep_game_reset_playtest(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    game_actions_reset_ember_road();
    set_message("Reset to Old Gate.");
    return state_emit(result_obj, state_json());
}

static bool ep_action_primary(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    const bool ok = invoke_primary_action();
    return action_emit(result_obj, ok ? GAME_ACTION_OK : GAME_ACTION_BLOCKED, s_last_message);
}

static bool ep_action_accept(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_accept_quest(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_travel(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_travel_north_road(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_battle(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_auto_battle(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_equip(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_equip_ring(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_claim(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_claim_reward(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_enter_mine(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_enter_old_mine(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_scout_mine(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_scout_old_mine(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_resolve_mine_depth(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_resolve_old_mine_depth(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_delve_mine(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_delve_old_mine(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_return_gate(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_return_old_gate(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

static bool ep_action_forge_lantern(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    char message[128];
    const GameActionResult result = game_actions_forge_mine_lantern(message, (int)sizeof(message));
    set_message(message);
    return action_emit(result_obj, result, message);
}

#if !defined(NT_PLATFORM_WEB)
static bool ep_capture_framebuffer(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
    const cJSON *output = cJSON_GetObjectItemCaseSensitive(params, "output");
    if (!cJSON_IsString(output) || output->valuestring == NULL || output->valuestring[0] == '\0') {
        if (err) {
            err->code = "invalid_params";
            err->message = "game.capture.framebuffer requires string output";
        }
        return false;
    }
    (void)snprintf(s_capture_path, sizeof(s_capture_path), "%s", output->valuestring);
    s_capture_path[sizeof(s_capture_path) - 1] = '\0';
    s_capture_pending = true;
    cJSON_AddStringToObject(result_obj, "output", s_capture_path);
    cJSON_AddBoolToObject(result_obj, "scheduled", true);
    return true;
}
#endif

static void register_game_endpoints(void) {
    static const nt_devapi_command_desc descs[] = {
        {"game.state", "game", "Return Ember Road runtime state.", "", "{runtime,hero,quest,battle,primary_action}", "immediate", "none"},
        {"game.reset_playtest", "game", "Reset Ember Road first slice state for automation.", "", "{runtime,quest_stage}", "immediate", "resets state"},
        {"game.action.primary", "game", "Run the current primary RPG action.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.accept_quest", "game", "Accept Wolves at the North Road.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.travel_north_road", "game", "Travel from town to the first encounter node.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.auto_battle", "game", "Resolve the automated Road Wolf battle.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.equip_ring", "game", "Equip the first ring reward.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.claim_reward", "game", "Claim the completed town quest reward.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.enter_old_mine", "game", "Enter the unlocked Old Mine route choice surface.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.scout_old_mine", "game", "Scout the Old Mine entrance and reveal the first threat/resource result.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.resolve_old_mine_depth", "game", "Resolve the first Old Mine depth encounter.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.delve_old_mine", "game", "Recover the first visible Old Mine ember cache after Depth 1.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.return_old_gate", "game", "Return from the Old Mine entry choice to Old Gate.", "", "{state,message}", "next-frame", "mutates state"},
        {"game.action.forge_mine_lantern", "game", "Forge the Mine Lantern from the first cache shards in Old Gate.", "", "{state,message}", "next-frame", "mutates state"},
#if !defined(NT_PLATFORM_WEB)
        {"game.capture.framebuffer", "game", "Schedule a PPM framebuffer capture after the next render.", "{output:string}", "{output,scheduled}", "next-frame", "writes file"},
#endif
    };
    game_state_register_devapi();
    game_devapi_ui_register();
    (void)nt_devapi_register(&descs[0], ep_game_state, NULL);
    (void)nt_devapi_register(&descs[1], ep_game_reset_playtest, NULL);
    (void)nt_devapi_register(&descs[2], ep_action_primary, NULL);
    (void)nt_devapi_register(&descs[3], ep_action_accept, NULL);
    (void)nt_devapi_register(&descs[4], ep_action_travel, NULL);
    (void)nt_devapi_register(&descs[5], ep_action_battle, NULL);
    (void)nt_devapi_register(&descs[6], ep_action_equip, NULL);
    (void)nt_devapi_register(&descs[7], ep_action_claim, NULL);
    (void)nt_devapi_register(&descs[8], ep_action_enter_mine, NULL);
    (void)nt_devapi_register(&descs[9], ep_action_scout_mine, NULL);
    (void)nt_devapi_register(&descs[10], ep_action_resolve_mine_depth, NULL);
    (void)nt_devapi_register(&descs[11], ep_action_delve_mine, NULL);
    (void)nt_devapi_register(&descs[12], ep_action_return_gate, NULL);
    (void)nt_devapi_register(&descs[13], ep_action_forge_lantern, NULL);
#if !defined(NT_PLATFORM_WEB)
    (void)nt_devapi_register(&descs[14], ep_capture_framebuffer, NULL);
#endif
}

static void register_box_node(const char *id, const char *parent, const char *role, const char *label, const char *text, UiBox box, bool enabled) {
    /* DevAPI UI rectangles use screen coordinates; keep UiBox itself Y-up. */
    const float y_down = s_view_h - box.y - box.h;
    (void)game_devapi_ui_register_node(id, parent, role, label, text, box.x, y_down, box.w, box.h, true, enabled);
}

static void register_ui_devapi(float w, float h) {
    (void)w;
    (void)h;
    game_devapi_ui_clear();
    (void)game_devapi_ui_register_node("root", "", "screen", "Ember Road", game_actions_objective_text(), 0.0F, 0.0F, w, h, true, true);
    if (!g_game_state.reward_item_ready || g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED) {
        register_box_node("ember.primary", "root", "button", "Primary Action", game_actions_primary_action_label(), s_primary_box, strcmp(game_actions_primary_action_id(), "ember.completed") != 0);
        register_box_node(game_actions_primary_action_id(), "root", "button", game_actions_primary_action_label(), game_actions_objective_text(), s_primary_box, strcmp(game_actions_primary_action_id(), "ember.completed") != 0);
    }
    register_box_node("ember.map.old_gate", "root", "map_node", "Old Gate", "Town hub", s_map_town_box, true);
    register_box_node("ember.map.north_road", "root", "map_node", "North Road", "First encounter node", s_map_road_box, g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_NOT_STARTED);
    register_box_node("ember.map.old_mine", "root", "map_node", "Old Mine", "Locked until level 2 / quest complete", s_map_mine_box, g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED);
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED &&
        g_game_state.location_index == GAME_STATE_LOCATION_OLD_GATE &&
        g_game_state.old_mine_cache_claimed) {
        register_box_node("ember.scene.forge_workbench", "root", "upgrade", g_game_state.gear_mine_lantern ? "Mine Lantern Equipped" : "Forge Table",
                          g_game_state.gear_mine_lantern ? "Depth 2 route is lit from the forge" : "Click the forge table to craft the Mine Lantern", s_forge_workbench_box, true);
        register_box_node("ember.town.lantern_upgrade", "root", "upgrade", g_game_state.gear_mine_lantern ? "Mine Lantern Ready" : "Forge Mine Lantern",
                          g_game_state.gear_mine_lantern ? "Depth 2 route is lit" : "Spend 6 ember shards to unlock Depth 2", s_primary_box, !g_game_state.gear_mine_lantern);
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_COMPLETED && g_game_state.location_index == GAME_STATE_LOCATION_OLD_MINE) {
        const bool scouted = g_game_state.old_mine_scouted;
        const bool depth_done = g_game_state.old_mine_depth_resolved;
        register_box_node("ember.mine.choice", "root", "modal", depth_done ? "Depth 1 Clear" : (scouted ? "Scout Report" : "Old Mine Entrance"),
                          g_game_state.old_mine_cache_claimed ? "First cache recovered; return with proof" : (depth_done ? "Cave Bat defeated; delve the visible cache or return" : (scouted ? "Depth 1, Cave Bat signs, ember shards found" : "Scout or return")), s_mine_choice_box, true);
        register_box_node("ember.mine.scout", "ember.mine.choice", "button", g_game_state.old_mine_cache_claimed ? "Cache Claimed" : (depth_done ? "Delve Cache" : (scouted ? "Clear Depth 1" : "Scout Entrance")),
                          g_game_state.old_mine_cache_claimed ? "First cache recovered" : (depth_done ? "Recover +1 shard, +2 gold, +3 XP" : (scouted ? "Resolve Cave Bat encounter" : "Reveal threat and resource signs")), s_mine_scout_box, !g_game_state.old_mine_cache_claimed);
        if (depth_done && !g_game_state.old_mine_cache_claimed) {
            register_box_node("ember.mine.next_delve", "ember.mine.choice", "button", "Delve Cache", "Next Old Mine choice after Depth 1 clear", s_mine_scout_box, true);
        }
        if (g_game_state.old_mine_scouted) {
            register_box_node("ember.mine.scout_result", "ember.mine.choice", "status", "Scout Result", "Cave Bat signs; +3 ember shards; depth 1 mapped", s_mine_choice_box, true);
        }
        if (g_game_state.old_mine_depth_resolved) {
            register_box_node("ember.mine.depth_encounter", "ember.mine.choice", "status", "Depth 1 Encounter", "Cave Bat defeated; +2 shards; +4 gold", s_mine_choice_box, true);
        }
        if (g_game_state.old_mine_cache_claimed) {
            register_box_node("ember.mine.delve_reward", "ember.mine.choice", "status", "Delve Reward", "First cache recovered; +1 shard; +2 gold; +3 XP", s_mine_choice_box, true);
        }
        register_box_node("ember.mine.back", "ember.mine.choice", "button", "Back to Old Gate", "Return to route hub", s_mine_back_box, true);
    }
    if (g_game_state.reward_item_ready && g_game_state.quest_stage_index != GAME_STATE_QUEST_STAGE_COMPLETED) {
        register_box_node("ember.equip_ring", "root", "button", "Equip Ring", "Rusty Iron Ring +1 attack", s_equip_box, !g_game_state.gear_ring_equipped);
    }
    if (g_game_state.quest_stage_index == GAME_STATE_QUEST_STAGE_WOLF_DEFEATED) {
        register_box_node("ember.claim_reward", "root", "button", "Claim Reward", "Return to town quest reward", s_claim_box, true);
    }
}
#endif

#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
static bool write_framebuffer_ppm(const char *path, int width, int height) {
    if (!path || path[0] == '\0' || width <= 0 || height <= 0) {
        return false;
    }
    const size_t pixel_count = (size_t)width * (size_t)height;
    uint8_t *pixels = (uint8_t *)malloc(pixel_count * 3U);
    if (!pixels) {
        return false;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    FILE *file = fopen(path, "wb");
    if (!file) {
        free(pixels);
        return false;
    }
    (void)fprintf(file, "P6\n%d %d\n255\n", width, height);
    const size_t row_size = (size_t)width * 3U;
    for (int y = height - 1; y >= 0; --y) {
        (void)fwrite(pixels + (size_t)y * row_size, 1U, row_size, file);
    }
    (void)fclose(file);
    free(pixels);
    return true;
}
#endif

static void frame(void) {
    nt_window_poll();
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_net_poll();
    }
#endif
    nt_input_poll();
    step_text_assets();
    const float w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const float h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    s_view_h = h;
    layout(w, h);
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
        nt_resource_invalidate(NT_ASSET_SHADER_CODE);
        nt_resource_invalidate(NT_ASSET_TEXTURE);
        nt_resource_invalidate(NT_ASSET_FONT);
        s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
            .type = NT_BUFFER_UNIFORM,
            .usage = NT_USAGE_DYNAMIC,
            .size = sizeof(nt_frame_uniforms_t),
            .label = "ember_road_globals",
        });
        nt_shape_renderer_restore_gpu();
        nt_sprite_renderer_restore_gpu();
        nt_text_renderer_restore_gpu();
    }
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.08F, 0.07F, 0.08F, 1.0F}, .clear_depth = 1.0F});
    update_text_globals(w, h);
    draw_scene(w, h);
    nt_shape_renderer_flush();
    nt_sprite_renderer_flush();
    nt_text_renderer_flush();
#if NT_DEVAPI_ENABLED && !defined(NT_PLATFORM_WEB)
    if (s_capture_pending) {
        (void)write_framebuffer_ppm(s_capture_path, (int)w, (int)h);
        s_capture_pending = false;
    }
#endif
    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Ember Road";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    parse_args(argc, argv);
    game_actions_reset_ember_road();

    g_nt_window.title = "Ember Road";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    nt_gfx_init(&gfx_desc);
    nt_shape_renderer_init();
    init_text_assets();

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
    shutdown_text_assets();
    nt_shape_renderer_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif

    return 0;
}
