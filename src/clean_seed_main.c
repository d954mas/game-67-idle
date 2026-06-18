#include "app/nt_app.h"
#include "atlas/nt_atlas.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "font/nt_font.h"
#include "fs/nt_fs.h"
#include "graphics/nt_gfx.h"
#include "hash/nt_hash.h"
#include "http/nt_http.h"
#include "input/nt_input.h"
#include "material/nt_material.h"
#include "math/nt_math.h"
#include "mine_cards_model_proof.h"
#include "nt_pack_format.h"
#include "renderers/nt_shape_renderer.h"
#include "renderers/nt_sprite_renderer.h"
#include "renderers/nt_text_renderer.h"
#include "resource/nt_resource.h"
#include "time/nt_time.h"
#include "ui/nt_ui_scale.h"
#include "window/nt_window.h"

#if SKELETAL_ANIMATION_EXTENSION_ENABLED
#include "skeletal_animation/nt_skeletal_animation.h"
#endif

#ifdef NT_PLATFORM_WEB
#include "platform/web/nt_platform_web.h"
#endif

#include "cJSON.h"

#include <glad/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MINE_CARDS_DEVAPI_PORT_DEFAULT 9123
#define MINE_CARDS_SKELETAL_MAX_MATRICES 64
#define MINE_CARDS_REWARD_LOG_ROWS 4
#define MINE_CARDS_HIT_FEEDBACK_SECONDS 0.55F
#define MINE_CARDS_UI_REF_W 960.0F
#define MINE_CARDS_UI_REF_H 540.0F

typedef struct UiBox {
    float x;
    float y;
    float w;
    float h;
} UiBox;

typedef struct MiningNodeDef {
    const char *id;
    const char *label;
    const char *yield_label;
    float base_interval_seconds;
    int unlock_level;
    int stone;
    int copper_ore;
    int coins;
    int mining_xp;
    int mastery_xp;
} MiningNodeDef;

typedef struct MineCardsRuntime {
    int selected_node;
    float progress_seconds;
    int stone;
    int copper_ore;
    int coins;
    int mining_xp;
    int mining_level;
    int mastery_xp[2];
    int tick_count;
    bool copper_pickaxe;
    bool copper_unlocked_seen;
    float callout_timer;
    float hit_feedback_timer;
    char callout_text[96];
    char hit_feedback_text[64];
    char reward_log[MINE_CARDS_REWARD_LOG_ROWS][96];
} MineCardsRuntime;

static const MiningNodeDef k_nodes[] = {
    {
        .id = "surface_stone",
        .label = "Surface Stone",
        .yield_label = "+1 Stone, +2 XP",
        .base_interval_seconds = 3.0F,
        .unlock_level = 1,
        .stone = 1,
        .mining_xp = 2,
        .mastery_xp = 1,
    },
    {
        .id = "copper_vein",
        .label = "Copper Vein",
        .yield_label = "+1 Copper, +1 Coin, +4 XP",
        .base_interval_seconds = 5.0F,
        .unlock_level = 2,
        .copper_ore = 1,
        .coins = 1,
        .mining_xp = 4,
        .mastery_xp = 1,
    },
};

static MineCardsRuntime s_game;
static bool s_devapi_enabled;
static uint16_t s_devapi_port = MINE_CARDS_DEVAPI_PORT_DEFAULT;
static int s_window_width = 1280;
static int s_window_height = 720;
static UiBox s_node_boxes[2];
static UiBox s_upgrade_box;
static UiBox s_primary_box;
static UiBox s_stage_box;
static UiBox s_player_box;
static UiBox s_content_box;
static UiBox s_activity_box;
static nt_hash32_t s_text_pack_id;
static nt_hash32_t s_ui_pack_id;
static nt_font_t s_ui_font;
static nt_resource_t s_ui_atlas;
static nt_material_t s_ui_sprite_material;
static nt_material_t s_text_material;
static nt_buffer_t s_frame_ubo;
static float s_surface_h;
static bool s_rich_render_initialized;

typedef enum MineCardsUiRegion {
    MINE_CARDS_UI_SCREEN_BG,
    MINE_CARDS_UI_TOP_BAR,
    MINE_CARDS_UI_BOTTOM_BAR,
    MINE_CARDS_UI_PANEL_DARK,
    MINE_CARDS_UI_PANEL_STAGE,
    MINE_CARDS_UI_PANEL_CONTENT,
    MINE_CARDS_UI_BOARD_PANEL_GENERATED,
    MINE_CARDS_UI_STAGE_ACTION_CARD_GENERATED,
    MINE_CARDS_UI_TAB_IDLE,
    MINE_CARDS_UI_TAB_ACTIVE,
    MINE_CARDS_UI_CARD_NODE,
    MINE_CARDS_UI_CARD_SELECTED,
    MINE_CARDS_UI_BUTTON_DARK,
    MINE_CARDS_UI_BUTTON_ACTIVE,
    MINE_CARDS_UI_NAV_IDLE,
    MINE_CARDS_UI_NAV_ACTIVE,
    MINE_CARDS_UI_PROGRESS_TRACK,
    MINE_CARDS_UI_PROGRESS_FILL,
    MINE_CARDS_UI_ROCK_STONE,
    MINE_CARDS_UI_ROCK_COPPER,
    MINE_CARDS_UI_LOCK_BADGE,
    MINE_CARDS_UI_CALLOUT,
    MINE_CARDS_UI_ICON_ACTIVITY_MINING,
    MINE_CARDS_UI_ICON_ACTIVITY_WOODCUTTING,
    MINE_CARDS_UI_ICON_ACTIVITY_FISHING,
    MINE_CARDS_UI_ICON_ACTIVITY_SMITHING,
    MINE_CARDS_UI_ICON_ACTIVITY_COMBAT,
    MINE_CARDS_UI_ICON_ACTIVITY_FARMING,
    MINE_CARDS_UI_ICON_ACTIVITY_BANK,
    MINE_CARDS_UI_ICON_ACTIVITY_SHOP,
    MINE_CARDS_UI_ICON_RESOURCE_STONE,
    MINE_CARDS_UI_ICON_RESOURCE_COPPER_ORE,
    MINE_CARDS_UI_ICON_RESOURCE_COIN,
    MINE_CARDS_UI_ICON_UPGRADE_PICKAXE,
    MINE_CARDS_UI_ICON_STATE_LOCKED,
    MINE_CARDS_UI_ICON_STATE_EQUIPPED,
    MINE_CARDS_UI_ICON_STATE_READY,
    MINE_CARDS_UI_STAGE_MINE_WALL_TILE,
    MINE_CARDS_UI_STAGE_MINE_FLOOR_SHADOW,
    MINE_CARDS_UI_STAGE_STONE_DEBRIS_CLUSTER,
    MINE_CARDS_UI_STAGE_COPPER_ORE_SEAM_STAMP,
    MINE_CARDS_UI_STAGE_WARM_LANTERN_LIGHT_OVERLAY,
    MINE_CARDS_UI_FX_STONE_HIT_CHIP,
    MINE_CARDS_UI_FX_COPPER_HIT_CHIP,
    MINE_CARDS_UI_FX_GEODE_REWARD_POP,
    MINE_CARDS_UI_FX_XP_SPARK,
    MINE_CARDS_UI_FX_COIN_SPARK,
    MINE_CARDS_UI_REGION_COUNT,
} MineCardsUiRegion;

static const char *const k_ui_region_names[MINE_CARDS_UI_REGION_COUNT] = {
    "mine-cards/ui_atlas/mine-cards/ui/screen_bg",
    "mine-cards/ui_atlas/mine-cards/ui/top_bar",
    "mine-cards/ui_atlas/mine-cards/ui/bottom_bar",
    "mine-cards/ui_atlas/mine-cards/ui/panel_dark",
    "mine-cards/ui_atlas/mine-cards/ui/panel_stage",
    "mine-cards/ui_atlas/mine-cards/ui/panel_content",
    "mine-cards/ui_atlas/mine-cards/ui/board_panel_generated",
    "mine-cards/ui_atlas/mine-cards/ui/stage_action_card_generated",
    "mine-cards/ui_atlas/mine-cards/ui/tab_idle",
    "mine-cards/ui_atlas/mine-cards/ui/tab_active",
    "mine-cards/ui_atlas/mine-cards/ui/card_node",
    "mine-cards/ui_atlas/mine-cards/ui/card_selected",
    "mine-cards/ui_atlas/mine-cards/ui/button_dark",
    "mine-cards/ui_atlas/mine-cards/ui/button_active",
    "mine-cards/ui_atlas/mine-cards/ui/nav_idle",
    "mine-cards/ui_atlas/mine-cards/ui/nav_active",
    "mine-cards/ui_atlas/mine-cards/ui/progress_track",
    "mine-cards/ui_atlas/mine-cards/ui/progress_fill",
    "mine-cards/ui_atlas/mine-cards/ui/rock_stone",
    "mine-cards/ui_atlas/mine-cards/ui/rock_copper",
    "mine-cards/ui_atlas/mine-cards/ui/lock_badge",
    "mine-cards/ui_atlas/mine-cards/ui/callout",
    "mine-cards/ui_atlas/mine-cards/ui/icon_activity_mining",
    "mine-cards/ui_atlas/mine-cards/ui/icon_activity_woodcutting",
    "mine-cards/ui_atlas/mine-cards/ui/icon_activity_fishing",
    "mine-cards/ui_atlas/mine-cards/ui/icon_activity_smithing",
    "mine-cards/ui_atlas/mine-cards/ui/icon_activity_combat",
    "mine-cards/ui_atlas/mine-cards/ui/icon_activity_farming",
    "mine-cards/ui_atlas/mine-cards/ui/icon_activity_bank",
    "mine-cards/ui_atlas/mine-cards/ui/icon_activity_shop",
    "mine-cards/ui_atlas/mine-cards/ui/icon_resource_stone",
    "mine-cards/ui_atlas/mine-cards/ui/icon_resource_copper_ore",
    "mine-cards/ui_atlas/mine-cards/ui/icon_resource_coin",
    "mine-cards/ui_atlas/mine-cards/ui/icon_upgrade_pickaxe",
    "mine-cards/ui_atlas/mine-cards/ui/icon_state_locked",
    "mine-cards/ui_atlas/mine-cards/ui/icon_state_equipped",
    "mine-cards/ui_atlas/mine-cards/ui/icon_state_ready",
    "mine-cards/ui_atlas/mine-cards/ui/stage_mine_wall_tile",
    "mine-cards/ui_atlas/mine-cards/ui/stage_mine_floor_shadow",
    "mine-cards/ui_atlas/mine-cards/ui/stage_stone_debris_cluster",
    "mine-cards/ui_atlas/mine-cards/ui/stage_copper_ore_seam_stamp",
    "mine-cards/ui_atlas/mine-cards/ui/stage_warm_lantern_light_overlay",
    "mine-cards/ui_atlas/mine-cards/ui/fx_stone_hit_chip_fx",
    "mine-cards/ui_atlas/mine-cards/ui/fx_copper_hit_chip_fx",
    "mine-cards/ui_atlas/mine-cards/ui/fx_geode_reward_pop_fx",
    "mine-cards/ui_atlas/mine-cards/ui/fx_xp_spark_fx",
    "mine-cards/ui_atlas/mine-cards/ui/fx_coin_spark_fx",
};

static const uint64_t k_ui_region_hashes[MINE_CARDS_UI_REGION_COUNT] = {
    0xDCC28E00F68949DDULL,
    0xC2D134DCABF931EFULL,
    0x4179867DD109E699ULL,
    0xCFA4483E270C06F9ULL,
    0xAB88CCFE034E7384ULL,
    0x4BC458602C3D2A61ULL,
    0xD4D188129B0AE53EULL,
    0x4BAC466FA864CF0AULL,
    0x71254BDB75C0B324ULL,
    0x31529A02DCE07F87ULL,
    0xF94475F760BAD2D0ULL,
    0x53A562003C6E4796ULL,
    0x24DE14ADBB62C1DDULL,
    0x1B89348C71FCDA72ULL,
    0x0AE6A985DA252D84ULL,
    0x19469F7215F4CDE7ULL,
    0x63FFAB099BCC41A1ULL,
    0xBF86B1874DF2AEACULL,
    0xF53C381D7F917E98ULL,
    0xC9C4B2EA75D595EFULL,
    0x2FB64131D31B44A4ULL,
    0xAF61F2A05C6523F4ULL,
    0xDEDEE71F1FCE4CC7ULL,
    0xD7DFE06FDD786118ULL,
    0x075576ED685CECDFULL,
    0x8E750A4371A4B348ULL,
    0xE6B02B480D61F7BEULL,
    0x07A12216F3CB37BBULL,
    0xB8C160F15E1DAAD8ULL,
    0x7D04F5A4ED272CF8ULL,
    0xA7ACA1C8F5D868B5ULL,
    0xDB2F96F8679F2AE7ULL,
    0xFB188522AEAD43CAULL,
    0x1657C6328CE3E132ULL,
    0x9416ED595D7AD515ULL,
    0x595146642B2065BDULL,
    0xC95AC6C068259253ULL,
    0x678A789837344A4FULL,
    0xDA4D4BE665F96EADULL,
    0x7D515085D928150CULL,
    0x5C8327ED6939414BULL,
    0x92415BAFDC025F05ULL,
    0x15079EE0F8D79691ULL,
    0x28D73C11F2FF5A34ULL,
    0xDF728C472E9BA11CULL,
};

static uint32_t s_ui_regions[MINE_CARDS_UI_REGION_COUNT];
static bool s_ui_regions_ready;
static int s_ui_missing_region = -1;

#if SKELETAL_ANIMATION_EXTENSION_ENABLED
static nt_skeletal_anim_clip_t *s_skeletal_clip;
static bool s_skeletal_ready;
static bool s_skeletal_reported;
static nt_skeletal_anim_attachment_sample_t s_skeletal_head;
static nt_skeletal_anim_attachment_sample_t s_skeletal_hand_l;
static nt_skeletal_anim_attachment_sample_t s_skeletal_hand_r;
static float s_skeletal_model_matrices[MINE_CARDS_SKELETAL_MAX_MATRICES * 16];
static int s_skeletal_model_matrix_count;
#endif

static void rect(float x, float y, float w, float h, const float color[4]);
static void rect_wire(float x, float y, float w, float h, const float color[4]);
static void capsule(float x, float y, float w, float h, const float color[4]);
static void circle(float x, float y, float radius, const float color[4]);
static void draw_text(float x, float y, float size, const float color[4], const char *text);
static float clamp01(float v);
static bool ui_assets_ready(void);
static bool is_portrait_layout(float w, float h);

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

static bool contains(UiBox box, float x, float y) {
    return x >= box.x && x <= box.x + box.w && y >= box.y && y <= box.y + box.h;
}

static UiBox ui_box_to_physical(const nt_ui_scale_t *scale, UiBox box) {
    return (UiBox){
        scale->offset_x + box.x * scale->scale_x,
        scale->offset_y + box.y * scale->scale_y,
        box.w * scale->scale_x,
        box.h * scale->scale_y,
    };
}

static float active_interval_seconds(void) {
    float interval = k_nodes[s_game.selected_node].base_interval_seconds;
    if (s_game.copper_pickaxe) {
        interval *= 0.85F;
    }
    return interval;
}

static bool node_unlocked(int node_index) {
    if (node_index < 0 || node_index >= (int)(sizeof(k_nodes) / sizeof(k_nodes[0]))) {
        return false;
    }
    return s_game.mining_level >= k_nodes[node_index].unlock_level;
}

static bool upgrade_affordable(void) {
    return !s_game.copper_pickaxe && s_game.stone >= 6 && s_game.copper_ore >= 32 && s_game.coins >= 32;
}

static void set_callout(const char *text) {
    (void)snprintf(s_game.callout_text, sizeof(s_game.callout_text), "%s", text);
    s_game.callout_timer = 1.8F;
}

static void set_hit_feedback(const char *text) {
    (void)snprintf(s_game.hit_feedback_text, sizeof(s_game.hit_feedback_text), "%s", text);
    s_game.hit_feedback_timer = MINE_CARDS_HIT_FEEDBACK_SECONDS;
}

static void push_log(const char *text) {
    for (int i = MINE_CARDS_REWARD_LOG_ROWS - 1; i > 0; --i) {
        memcpy(s_game.reward_log[i], s_game.reward_log[i - 1], sizeof(s_game.reward_log[i]));
    }
    (void)snprintf(s_game.reward_log[0], sizeof(s_game.reward_log[0]), "%s", text);
}

static void reset_mine_cards(void) {
    memset(&s_game, 0, sizeof(s_game));
    s_game.selected_node = 0;
    s_game.mining_level = 1;
    push_log("Mining started: Surface Stone");
}

static void apply_level_unlocks(void) {
    const int previous_level = s_game.mining_level;
    s_game.mining_level = s_game.mining_xp >= 12 ? 2 : 1;
    if (s_game.mining_level > previous_level) {
        push_log("Mining Lv2: Copper Vein unlocked");
        set_callout("Mining Lv2: Copper unlocked");
        s_game.copper_unlocked_seen = true;
    }
}

static void resolve_mining_tick(void) {
    const MiningNodeDef *node = &k_nodes[s_game.selected_node];
    s_game.stone += node->stone;
    s_game.copper_ore += node->copper_ore;
    s_game.coins += node->coins;
    s_game.mining_xp += node->mining_xp;
    s_game.mastery_xp[s_game.selected_node] += node->mastery_xp;
    s_game.tick_count += 1;

    char log_line[96];
    if (s_game.selected_node == 0) {
        (void)snprintf(log_line, sizeof(log_line), "+%d Stone, +%d Mining XP", node->stone, node->mining_xp);
        set_hit_feedback("+1 STONE");
    } else {
        (void)snprintf(log_line, sizeof(log_line), "+%d Copper Ore, +%d Coin, +%d XP", node->copper_ore, node->coins, node->mining_xp);
        set_hit_feedback("+1 COPPER");
    }
    push_log(log_line);
    set_callout(log_line);
    apply_level_unlocks();

    if ((s_game.tick_count % 17) == 0) {
        s_game.coins += 8;
        s_game.mining_xp += 5;
        push_log("Geode found: +8 Coins, +5 XP");
        set_callout("GEODE FOUND: +8 Coins, +5 XP");
        set_hit_feedback("GEODE +8 COINS");
        apply_level_unlocks();
    }
}

static void update_mining(float dt) {
    const float interval = active_interval_seconds();
    s_game.progress_seconds += dt;
    while (s_game.progress_seconds >= interval) {
        s_game.progress_seconds -= interval;
        resolve_mining_tick();
    }
}

static void select_node(int node_index) {
    if (!node_unlocked(node_index)) {
        push_log("Copper Vein needs Mining Lv2");
        return;
    }
    if (s_game.selected_node != node_index) {
        s_game.selected_node = node_index;
        s_game.progress_seconds = 0.0F;
        push_log(node_index == 0 ? "Mining Surface Stone" : "Mining Copper Vein");
    }
}

static void buy_upgrade(void) {
    if (s_game.copper_pickaxe) {
        push_log("Copper Pickaxe already equipped");
        return;
    }
    if (!upgrade_affordable()) {
        char line[96];
        const int missing_stone = s_game.stone < 6 ? 6 - s_game.stone : 0;
        const int missing_copper = s_game.copper_ore < 32 ? 32 - s_game.copper_ore : 0;
        const int missing_coins = s_game.coins < 32 ? 32 - s_game.coins : 0;
        (void)snprintf(line, sizeof(line), "Need: %d Stone, %d Copper, %d Coins", missing_stone, missing_copper, missing_coins);
        push_log(line);
        set_callout(line);
        return;
    }
    s_game.stone -= 6;
    s_game.copper_ore -= 32;
    s_game.coins -= 32;
    s_game.copper_pickaxe = true;
    push_log("Copper Pickaxe equipped: 5.0s -> 4.25s");
    set_callout("Copper Pickaxe equipped");
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
    s_surface_h = h;

    const bool portrait = is_portrait_layout(w, h);
    if (portrait) {
        const float stage_h = h < 880.0F ? h * 0.30F : 290.0F;
        s_player_box = (UiBox){18.0F, h - 82.0F - stage_h, w - 36.0F, stage_h};
        s_content_box = (UiBox){18.0F, 84.0F, w - 36.0F, s_player_box.y - 96.0F};

        const float node_w = s_content_box.w - 36.0F;
        const float node_h = 50.0F;
        const float node_gap = 12.0F;
        const float node_top = s_content_box.y + s_content_box.h - 136.0F;
        s_activity_box = (UiBox){s_content_box.x + 18.0F, s_content_box.y + s_content_box.h - 58.0F, node_w, 42.0F};
        s_node_boxes[0] = (UiBox){s_content_box.x + 18.0F, node_top - node_h, node_w, node_h};
        s_node_boxes[1] = (UiBox){s_content_box.x + 18.0F, node_top - node_h * 2.0F - node_gap, node_w, node_h};
        s_upgrade_box = (UiBox){s_content_box.x + 18.0F, s_content_box.y + 18.0F, node_w, 146.0F};
        s_primary_box = (UiBox){s_upgrade_box.x + s_upgrade_box.w - 166.0F, s_upgrade_box.y + 14.0F, 148.0F, 30.0F};
        s_stage_box = (UiBox){s_player_box.x + 18.0F, s_player_box.y + 10.0F, s_player_box.w - 36.0F, 52.0F};
        return;
    }

    const float hero_y = h * 0.505F;
    s_player_box = (UiBox){18.0F, hero_y, w - 36.0F, h - hero_y - 62.0F};
    s_content_box = (UiBox){18.0F, 84.0F, w - 36.0F, hero_y - 96.0F};

    const float upgrade_w = s_content_box.w < 720.0F ? 210.0F : 252.0F;
    const float list_w = s_content_box.w - upgrade_w - 42.0F;
    const float node_w = list_w > 210.0F ? list_w : 210.0F;
    const bool compact = s_content_box.h < 190.0F;
    const float node_h = compact ? 34.0F : 46.0F;
    const float node_gap = compact ? 6.0F : 8.0F;
    const float node_top = s_content_box.y + s_content_box.h - (compact ? 52.0F : 112.0F);
    s_activity_box = (UiBox){s_content_box.x + 18.0F, s_content_box.y + s_content_box.h - (compact ? 42.0F : 54.0F), compact ? 178.0F : 204.0F, compact ? 30.0F : 38.0F};
    s_node_boxes[0] = (UiBox){s_content_box.x + 18.0F, node_top - node_h, node_w, node_h};
    s_node_boxes[1] = (UiBox){s_content_box.x + 18.0F, node_top - node_h * 2.0F - node_gap, node_w, node_h};
    const float upgrade_h = s_content_box.h < 194.0F ? s_content_box.h - 36.0F : 158.0F;
    s_upgrade_box = (UiBox){s_content_box.x + s_content_box.w - upgrade_w - 18.0F, s_content_box.y + 18.0F, upgrade_w, upgrade_h};
    s_primary_box = (UiBox){s_upgrade_box.x + 14.0F, s_upgrade_box.y + 10.0F, s_upgrade_box.w - 28.0F, compact ? 34.0F : 34.0F};
    s_stage_box = (UiBox){s_player_box.x + 18.0F, s_player_box.y + 10.0F, s_player_box.w - 36.0F, 52.0F};
}

static void rect(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void rect_wire(float x, float y, float w, float h, const float color[4]) {
    nt_shape_renderer_rect_wire((float[3]){x + w * 0.5F, y + h * 0.5F, 0.0F}, (float[2]){w, h}, color);
}

static void circle(float x, float y, float radius, const float color[4]) {
    nt_shape_renderer_circle((float[3]){x, y, 0.0F}, radius, color);
}

static void capsule(float x, float y, float w, float h, const float color[4]) {
    const float r = h * 0.5F;
    rect(x + r, y, w - r * 2.0F, h, color);
    circle(x + r, y + r, r, color);
    circle(x + w - r, y + r, r, color);
}

static float clamp01(float v) {
    if (v < 0.0F) {
        return 0.0F;
    }
    if (v > 1.0F) {
        return 1.0F;
    }
    return v;
}

static bool is_portrait_layout(float w, float h) {
    return h > w * 1.10F;
}

static UiBox stage_action_box(float w, float h) {
    const bool portrait = is_portrait_layout(w, h);
    const float top_reserved = portrait ? 46.0F : 34.0F;
    const float bottom_reserved = portrait ? 54.0F : 50.0F;
    return (UiBox){
        s_player_box.x + 18.0F,
        s_player_box.y + bottom_reserved,
        s_player_box.w - 36.0F,
        s_player_box.h - top_reserved - bottom_reserved,
    };
}

static UiBox stage_actor_box(float w, float h) {
    const bool portrait = is_portrait_layout(w, h);
    const UiBox action = stage_action_box(w, h);
    const float actor_w = portrait ? 236.0F : 292.0F;
    const float actor_h = action.h + (portrait ? 22.0F : 34.0F);
    const float actor_center_x = action.x + action.w * (portrait ? 0.59F : 0.41F);
    const float actor_x = actor_center_x - actor_w * 0.5F;
    const float actor_y = action.y + (portrait ? -24.0F : -30.0F);
    return (UiBox){actor_x, actor_y, actor_w, actor_h};
}

static UiBox stage_target_box(float w, float h, float scale) {
    const bool portrait = is_portrait_layout(w, h);
    const UiBox action = stage_action_box(w, h);
    const float target_w = (portrait ? 116.0F : 106.0F) * scale;
    const float target_h = (portrait ? 92.0F : 82.0F) * scale;
    const float target_x = action.x + action.w * (portrait ? 0.33F : 0.27F);
    const float target_y = action.y + action.h * (portrait ? 0.31F : 0.34F);
    return (UiBox){target_x - target_w * 0.5F, target_y - target_h * 0.5F, target_w, target_h};
}

static UiBox hit_feedback_box(float w, float h) {
    const bool portrait = is_portrait_layout(w, h);
    const UiBox action = stage_action_box(w, h);
    const UiBox target = stage_target_box(w, h, 1.0F);
    const float box_w = portrait ? 148.0F : 166.0F;
    const float box_h = 28.0F;
    float box_x = target.x + target.w + (portrait ? 8.0F : 14.0F);
    float box_y = target.y + target.h * (portrait ? 0.28F : 0.42F);
    const float min_x = s_player_box.x + 14.0F;
    const float max_x = s_player_box.x + s_player_box.w - box_w - 14.0F;
    if (box_x < min_x) {
        box_x = min_x;
    }
    if (box_x > max_x) {
        box_x = max_x;
    }
    const float min_y = action.y + 6.0F;
    const float max_y = action.y + action.h - box_h - 6.0F;
    if (box_y < min_y) {
        box_y = min_y;
    }
    if (box_y > max_y) {
        box_y = max_y;
    }
    return (UiBox){box_x, box_y, box_w, box_h};
}

static UiBox stage_callout_box(float w, float h) {
    const bool portrait = is_portrait_layout(w, h);
    const float box_w = s_stage_box.w > 360.0F ? 360.0F : s_stage_box.w - 8.0F;
    const float box_h = 34.0F;
    if (portrait) {
        return (UiBox){
            s_stage_box.x + (s_stage_box.w - box_w) * 0.5F,
            s_stage_box.y + s_stage_box.h + 8.0F,
            box_w,
            box_h,
        };
    }
    return (UiBox){
        s_player_box.x + s_player_box.w - box_w - 18.0F,
        s_player_box.y + s_player_box.h - 90.0F,
        box_w,
        box_h,
    };
}

static int missing_stone(void) {
    return s_game.stone < 6 ? 6 - s_game.stone : 0;
}

static int missing_copper(void) {
    return s_game.copper_ore < 32 ? 32 - s_game.copper_ore : 0;
}

static int missing_coins(void) {
    return s_game.coins < 32 ? 32 - s_game.coins : 0;
}

static void draw_text(float x, float y, float size, const float color[4], const char *text) {
    if (text == NULL || text[0] == '\0') {
        return;
    }
    const float offsets[][2] = {
        {0.0F, 0.0F},
        {1.0F, 0.0F},
        {0.0F, 1.0F},
        {1.0F, 1.0F},
    };
    for (int i = 0; i < (int)(sizeof(offsets) / sizeof(offsets[0])); ++i) {
        mat4 model;
        glm_mat4_identity(model);
        glm_translate(model, (vec3){x + offsets[i][0], y + offsets[i][1], 0.0F});
        nt_text_renderer_draw(text, (const float *)model, size, color, 0.0F, 0.0F);
    }
}

static bool text_ready(void) {
    const nt_material_info_t *info = nt_material_get_info(s_text_material);
    return info != NULL && info->ready && nt_font_get_metrics(s_ui_font).units_per_em > 0;
}

static bool resolve_ui_regions(void) {
    if (s_ui_regions_ready) {
        return true;
    }
    (void)k_ui_region_hashes;
    const char *const atlas_prefix = "mine-cards/ui_atlas/";
    const size_t atlas_prefix_len = strlen(atlas_prefix);
    if (!nt_resource_is_ready(s_ui_atlas)) {
        s_ui_missing_region = -2;
        return false;
    }
    for (int i = 0; i < (int)MINE_CARDS_UI_REGION_COUNT; ++i) {
        const char *region_name = k_ui_region_names[i];
        if (strncmp(region_name, atlas_prefix, atlas_prefix_len) == 0) {
            region_name += atlas_prefix_len;
        }
        const uint32_t region = nt_atlas_find_region(s_ui_atlas, nt_hash64_str(region_name).value);
        if (region == NT_ATLAS_INVALID_REGION) {
            s_ui_missing_region = i;
            return false;
        }
        s_ui_regions[i] = region;
    }
    s_ui_missing_region = -1;
    s_ui_regions_ready = true;
    return true;
}

static bool ui_assets_ready(void) {
    const nt_material_info_t *info = nt_material_get_info(s_ui_sprite_material);
    return info != NULL && info->ready && resolve_ui_regions();
}

static void draw_ui_slice(MineCardsUiRegion region, float x, float y, float w, float h) {
    if (w <= 0.0F || h <= 0.0F) {
        return;
    }
    nt_sprite_renderer_emit_slice9(s_ui_atlas, s_ui_regions[region], x, y, w, h, NULL, 1.0F, 0xFFFFFFFFU, 0, NT_MATH_MAT4_IDENTITY);
}

static void draw_ui_slice_box(MineCardsUiRegion region, UiBox box) {
    draw_ui_slice(region, box.x, box.y, box.w, box.h);
}

static void draw_ui_icon(MineCardsUiRegion region, float x, float y, float w, float h) {
    if (w <= 0.0F || h <= 0.0F) {
        return;
    }
    const nt_texture_region_t *r = nt_atlas_get_region(s_ui_atlas, s_ui_regions[region]);
    const float ipu = nt_atlas_get_inverse_pixels_per_unit(s_ui_atlas);
    const float src_w = (float)r->source_w * ipu;
    const float src_h = (float)r->source_h * ipu;
    if (src_w <= 0.0F || src_h <= 0.0F) {
        return;
    }
    float m[16] = {0};
    m[0] = w / src_w;
    m[5] = -(h / src_h);
    m[10] = 1.0F;
    m[12] = x + w * 0.5F;
    m[13] = y + h * 0.5F;
    m[15] = 1.0F;
    nt_sprite_renderer_emit_region(s_ui_atlas, s_ui_regions[region], m, r->origin_x, r->origin_y, 0xFFFFFFFFU, 0);
}

static void draw_stage_environment_sprites(float w, float h) {
    const bool portrait = is_portrait_layout(w, h);
    const UiBox action = stage_action_box(w, h);
    const UiBox actor = stage_actor_box(w, h);
    const UiBox target = stage_target_box(w, h, 1.0F);
    const float floor_w = portrait ? 198.0F : 220.0F;
    const float floor_h = portrait ? 78.0F : 76.0F;
    const float floor_x = (actor.x + actor.w * 0.44F + target.x + target.w * 0.50F) * 0.5F - floor_w * 0.5F;
    const float floor_y = action.y + (portrait ? 20.0F : 14.0F);
    const float wall_w = portrait ? 66.0F : 76.0F;
    const float wall_h = portrait ? 76.0F : 76.0F;
    const float wall_x = action.x + action.w - wall_w - (portrait ? 38.0F : 80.0F);
    const float wall_y = action.y + action.h * (portrait ? 0.38F : 0.24F);
    const float debris_size = portrait ? 32.0F : 36.0F;
    const float ore_size = portrait ? 28.0F : 30.0F;
    const float light_w = portrait ? 54.0F : 50.0F;
    const float light_h = portrait ? 68.0F : 62.0F;

    draw_ui_icon(MINE_CARDS_UI_STAGE_MINE_WALL_TILE, wall_x, wall_y, wall_w, wall_h);
    draw_ui_icon(MINE_CARDS_UI_STAGE_WARM_LANTERN_LIGHT_OVERLAY, actor.x + actor.w * 0.48F - light_w * 0.5F, action.y + action.h * (portrait ? 0.38F : 0.24F), light_w, light_h);
    draw_ui_icon(MINE_CARDS_UI_STAGE_MINE_FLOOR_SHADOW, floor_x, floor_y, floor_w, floor_h);
    draw_ui_icon(MINE_CARDS_UI_STAGE_STONE_DEBRIS_CLUSTER, target.x - debris_size * 0.85F, target.y - debris_size * 0.10F, debris_size, debris_size);
    draw_ui_icon(MINE_CARDS_UI_STAGE_COPPER_ORE_SEAM_STAMP, wall_x + wall_w * 0.42F, wall_y + wall_h * 0.04F, ore_size, ore_size);
}

static void draw_panel(UiBox box, const float tint[4]) {
    rect(box.x, box.y - 5.0F, box.w, box.h, (float[4]){0.04F, 0.06F, 0.08F, 0.18F});
    rect(box.x, box.y, box.w, box.h, tint);
    rect_wire(box.x, box.y, box.w, box.h, (float[4]){0.42F, 0.32F, 0.30F, 0.78F});
}

static void draw_node_panel(int index) {
    const bool selected = s_game.selected_node == index;
    const bool unlocked = node_unlocked(index);
    const UiBox box = s_node_boxes[index];
    draw_panel(box, selected ? (float[4]){0.24F, 0.18F, 0.38F, 1.0F} : (float[4]){0.14F, 0.11F, 0.18F, 1.0F});
    if (!unlocked) {
        rect(box.x + box.w - 48.0F, box.y + 18.0F, 28.0F, 20.0F, (float[4]){0.20F, 0.16F, 0.18F, 1.0F});
    }
}

static void draw_mine_backdrop(float w, float h) {
    rect(0.0F, 0.0F, w, h, (float[4]){0.06F, 0.04F, 0.09F, 1.0F});
    rect(0.0F, h - 78.0F, w, 78.0F, (float[4]){0.10F, 0.07F, 0.14F, 1.0F});
    rect(0.0F, 0.0F, w, 76.0F, (float[4]){0.10F, 0.08F, 0.12F, 1.0F});
    rect_wire(0.0F, h - 78.0F, w, 78.0F, (float[4]){0.42F, 0.30F, 0.34F, 0.82F});
    rect_wire(0.0F, 0.0F, w, 76.0F, (float[4]){0.42F, 0.30F, 0.34F, 0.82F});

    const float mine_x = s_stage_box.x;
    const float mine_y = s_stage_box.y;
    const float mine_w = s_stage_box.w;
    const float mine_h = s_stage_box.h;

    const float rock_x = mine_x + mine_w - 56.0F;
    const float rock_y = mine_y + mine_h * 0.48F;
    rect(rock_x - 26.0F, rock_y - 18.0F, 52.0F, 36.0F, (float[4]){0.36F, 0.39F, 0.40F, 1.0F});
    rect(rock_x - 26.0F, rock_y + 8.0F, 52.0F, 10.0F, (float[4]){0.54F, 0.58F, 0.58F, 1.0F});
    if (s_game.selected_node == 1) {
        rect(rock_x - 25.0F, rock_y - 4.0F, 24.0F, 18.0F, (float[4]){0.78F, 0.43F, 0.20F, 1.0F});
        rect(rock_x + 12.0F, rock_y + 12.0F, 28.0F, 16.0F, (float[4]){0.70F, 0.35F, 0.18F, 1.0F});
    }
}

static void draw_callout(float w, float h) {
    (void)w;
    if (s_game.callout_timer <= 0.0F || s_game.callout_text[0] == '\0') {
        return;
    }
    const float box_w = s_stage_box.w > 360.0F ? 360.0F : s_stage_box.w - 8.0F;
    const float box_h = 42.0F;
    const float box_x = s_stage_box.x + (s_stage_box.w - box_w) * 0.5F;
    const float box_y = s_stage_box.y + s_stage_box.h - box_h - 8.0F;
    capsule(box_x, box_y, box_w, box_h, (float[4]){0.06F, 0.20F, 0.18F, 0.90F});
    rect(box_x + 22.0F, box_y + box_h - 13.0F, box_w - 44.0F, 5.0F, (float[4]){1.0F, 1.0F, 1.0F, 0.14F});
}

static void draw_progress_bar(float w, float h) {
    const float interval = active_interval_seconds();
    const float progress = interval > 0.0F ? clamp01(s_game.progress_seconds / interval) : 0.0F;
    const float bar_w = s_stage_box.w > 360.0F ? 360.0F : s_stage_box.w;
    const float bar_x = s_stage_box.x + (s_stage_box.w - bar_w) * 0.5F;
    const float bar_y = s_stage_box.y + 6.0F;
    capsule(bar_x, bar_y, bar_w, 18.0F, (float[4]){0.08F, 0.13F, 0.15F, 0.70F});
    capsule(bar_x + 3.0F, bar_y + 3.0F, (bar_w - 6.0F) * progress, 12.0F, (float[4]){0.15F, 0.76F, 0.42F, 1.0F});
}

static void draw_button_panel(UiBox box, bool active) {
    rect(box.x, box.y - 5.0F, box.w, box.h, (float[4]){0.04F, 0.07F, 0.08F, 0.28F});
    capsule(box.x, box.y, box.w, box.h, active ? (float[4]){0.42F, 0.24F, 0.78F, 1.0F} : (float[4]){0.22F, 0.24F, 0.30F, 1.0F});
    rect(box.x + 22.0F, box.y + box.h - 21.0F, box.w - 44.0F, 9.0F, (float[4]){1.0F, 1.0F, 1.0F, 0.16F});
}

static void draw_ui_shapes(float w, float h) {
    if (ui_assets_ready()) {
        return;
    }
    draw_mine_backdrop(w, h);
    for (int i = 0; i < 3; ++i) {
        const float tab_x = 42.0F + (float)i * 112.0F;
        rect(tab_x, 14.0F, 96.0F, 48.0F, i == 1 ? (float[4]){0.20F, 0.16F, 0.34F, 1.0F} : (float[4]){0.12F, 0.10F, 0.14F, 1.0F});
        rect_wire(tab_x, 14.0F, 96.0F, 48.0F, i == 1 ? (float[4]){0.50F, 0.95F, 0.12F, 1.0F} : (float[4]){0.42F, 0.32F, 0.30F, 0.82F});
    }
    draw_panel(s_player_box, (float[4]){0.11F, 0.08F, 0.14F, 1.0F});
    draw_panel(s_content_box, (float[4]){0.10F, 0.07F, 0.13F, 1.0F});
    draw_node_panel(0);
    draw_node_panel(1);
    draw_panel(s_upgrade_box, (float[4]){0.12F, 0.09F, 0.16F, 1.0F});
    draw_button_panel(s_primary_box, upgrade_affordable());
    draw_progress_bar(w, h);
    draw_callout(w, h);
}

static void draw_ui_sprites(float w, float h) {
    if (!ui_assets_ready()) {
        return;
    }
    const bool compact = w < 800.0F || s_content_box.h < 190.0F;
    nt_sprite_renderer_set_material(s_ui_sprite_material);

    draw_ui_slice(MINE_CARDS_UI_SCREEN_BG, 0.0F, 0.0F, w, h);
    draw_ui_slice(MINE_CARDS_UI_TOP_BAR, 0.0F, h - 78.0F, w, 78.0F);
    draw_ui_slice(MINE_CARDS_UI_BOTTOM_BAR, 0.0F, 0.0F, w, 76.0F);
    draw_ui_slice_box(MINE_CARDS_UI_PANEL_STAGE, s_player_box);
    draw_ui_slice_box(MINE_CARDS_UI_BOARD_PANEL_GENERATED, s_content_box);
    draw_ui_slice_box(MINE_CARDS_UI_PANEL_DARK, s_stage_box);
    draw_ui_slice_box(MINE_CARDS_UI_STAGE_ACTION_CARD_GENERATED, stage_action_box(w, h));
    draw_stage_environment_sprites(w, h);

    draw_ui_slice_box(MINE_CARDS_UI_TAB_ACTIVE, s_activity_box);
    draw_ui_icon(MINE_CARDS_UI_ICON_ACTIVITY_MINING, s_activity_box.x + s_activity_box.w - 34.0F, s_activity_box.y + 5.0F, 26.0F, 26.0F);

    for (int i = 0; i < 2; ++i) {
        if (s_game.selected_node == i) {
            draw_ui_slice_box(MINE_CARDS_UI_CARD_SELECTED, s_node_boxes[i]);
        } else if (node_unlocked(i)) {
            draw_ui_slice_box(MINE_CARDS_UI_CARD_NODE, s_node_boxes[i]);
        }
        const UiBox box = s_node_boxes[i];
        draw_ui_icon(i == 0 ? MINE_CARDS_UI_ICON_RESOURCE_STONE : MINE_CARDS_UI_ICON_RESOURCE_COPPER_ORE, box.x + 12.0F, box.y + box.h * 0.5F - 13.0F, 26.0F, 26.0F);
        if (!node_unlocked(i)) {
            draw_ui_icon(MINE_CARDS_UI_ICON_STATE_LOCKED, box.x + box.w - 48.0F, box.y + box.h * 0.5F - 13.0F, 26.0F, 26.0F);
        }
    }
    draw_ui_slice_box(MINE_CARDS_UI_PANEL_CONTENT, s_upgrade_box);
    if (upgrade_affordable() || s_game.copper_pickaxe) {
        draw_ui_slice_box(upgrade_affordable() ? MINE_CARDS_UI_BUTTON_ACTIVE : MINE_CARDS_UI_BUTTON_DARK, s_primary_box);
    }
    if (!s_game.copper_pickaxe) {
        const float icon_y0 = s_upgrade_box.y + s_upgrade_box.h - (compact ? 74.0F : 80.0F);
        draw_ui_icon(MINE_CARDS_UI_ICON_RESOURCE_STONE, s_upgrade_box.x + 14.0F, icon_y0 - 2.0F, 16.0F, 16.0F);
        draw_ui_icon(MINE_CARDS_UI_ICON_RESOURCE_COPPER_ORE, s_upgrade_box.x + 14.0F, icon_y0 - 15.0F, 16.0F, 16.0F);
        draw_ui_icon(MINE_CARDS_UI_ICON_RESOURCE_COIN, s_upgrade_box.x + 14.0F, icon_y0 - 28.0F, 16.0F, 16.0F);
    } else {
        draw_ui_icon(MINE_CARDS_UI_ICON_STATE_EQUIPPED, s_upgrade_box.x + 14.0F, s_upgrade_box.y + s_upgrade_box.h - 86.0F, 28.0F, 28.0F);
    }

    const float interval = active_interval_seconds();
    const float progress = interval > 0.0F ? clamp01(s_game.progress_seconds / interval) : 0.0F;
    const float bar_w = s_stage_box.w > 360.0F ? 360.0F : s_stage_box.w;
    const float bar_x = s_stage_box.x + (s_stage_box.w - bar_w) * 0.5F;
    const float bar_y = s_stage_box.y + 6.0F;
    draw_ui_slice(MINE_CARDS_UI_PROGRESS_TRACK, bar_x, bar_y, bar_w, 18.0F);
    if (progress > 0.02F) {
        draw_ui_slice(MINE_CARDS_UI_PROGRESS_FILL, bar_x + 3.0F, bar_y + 3.0F, (bar_w - 6.0F) * progress, 12.0F);
    }

    if (upgrade_affordable() || s_game.copper_pickaxe) {
        draw_ui_icon(s_game.copper_pickaxe ? MINE_CARDS_UI_ICON_STATE_READY : MINE_CARDS_UI_ICON_UPGRADE_PICKAXE, s_primary_box.x + s_primary_box.w - 32.0F, s_primary_box.y + s_primary_box.h * 0.5F - 12.0F, 24.0F, 24.0F);
    }

    nt_sprite_renderer_flush();
}

static void draw_stage_target_sprite(float w, float h) {
    const MineCardsUiRegion stage_target = s_game.selected_node == 1 ? MINE_CARDS_UI_ROCK_COPPER : MINE_CARDS_UI_ROCK_STONE;
    const float hit_t = clamp01(s_game.hit_feedback_timer / MINE_CARDS_HIT_FEEDBACK_SECONDS);
    UiBox target_box = stage_target_box(w, h, 1.0F + hit_t * 0.10F);
    target_box.x -= hit_t * 4.0F;
    target_box.y += hit_t * 3.0F;
    draw_ui_slice_box(stage_target, target_box);
}

static void draw_stage_fx_sprites(float w, float h) {
    if (s_game.hit_feedback_timer <= 0.0F) {
        return;
    }
    const bool portrait = is_portrait_layout(w, h);
    const bool geode = strstr(s_game.hit_feedback_text, "GEODE") != NULL;
    const UiBox target = stage_target_box(w, h, 1.0F);
    const UiBox reward = hit_feedback_box(w, h);
    const float hit_t = clamp01(s_game.hit_feedback_timer / MINE_CARDS_HIT_FEEDBACK_SECONDS);
    const float hit_size = (portrait ? 74.0F : 70.0F) * (0.82F + hit_t * 0.45F);
    const float hit_x = target.x + target.w * 0.24F - hit_size * 0.5F;
    const float hit_y = target.y + target.h * 0.42F - hit_size * 0.5F;
    const MineCardsUiRegion hit_fx = s_game.selected_node == 1 ? MINE_CARDS_UI_FX_COPPER_HIT_CHIP : MINE_CARDS_UI_FX_STONE_HIT_CHIP;
    draw_ui_icon(hit_fx, hit_x, hit_y, hit_size, hit_size);
    if (geode) {
        const float geode_size = portrait ? 62.0F : 58.0F;
        const float coin_size = portrait ? 42.0F : 38.0F;
        draw_ui_icon(MINE_CARDS_UI_FX_GEODE_REWARD_POP, reward.x - geode_size * 0.10F, reward.y + reward.h * 0.5F - geode_size * 0.5F, geode_size, geode_size);
        draw_ui_icon(MINE_CARDS_UI_FX_COIN_SPARK, reward.x + reward.w - coin_size * 0.52F, reward.y + reward.h * 0.5F - coin_size * 0.5F, coin_size, coin_size);
    } else {
        const float xp_size = portrait ? 40.0F : 38.0F;
        draw_ui_icon(MINE_CARDS_UI_FX_XP_SPARK, target.x + target.w * 0.72F, target.y + target.h * 0.50F, xp_size, xp_size);
    }
}

static void draw_stage_overlay_sprites(float w, float h) {
    if (!ui_assets_ready()) {
        return;
    }
    nt_sprite_renderer_set_material(s_ui_sprite_material);

    draw_stage_target_sprite(w, h);
    draw_stage_fx_sprites(w, h);

    const bool show_stage_callout = s_game.callout_timer > 0.0F && s_game.callout_text[0] != '\0' && s_game.hit_feedback_timer <= 0.0F;
    if (show_stage_callout) {
        draw_ui_slice_box(MINE_CARDS_UI_CALLOUT, stage_callout_box(w, h));
    }
    if (s_game.hit_feedback_timer > 0.0F && s_game.hit_feedback_text[0] != '\0') {
        draw_ui_slice_box(MINE_CARDS_UI_CALLOUT, hit_feedback_box(w, h));
    }
    nt_sprite_renderer_flush();
}

static void draw_ui_text(float w, float h) {
    if (!text_ready()) {
        return;
    }
    nt_text_renderer_set_material(s_text_material);
    nt_text_renderer_set_font(s_ui_font);

    const bool compact = w < 800.0F || s_content_box.h < 190.0F;
    const bool portrait = is_portrait_layout(w, h);

    draw_text(22.0F, h - 34.0F, compact ? 22.0F : 24.0F, (float[4]){0.86F, 0.92F, 1.0F, 1.0F}, "MINE CARDS");
    draw_text(compact ? 220.0F : 250.0F, h - 28.0F, compact ? 13.0F : 16.0F, (float[4]){0.88F, 1.0F, 0.62F, 1.0F}, "MINING / SURFACE");
    draw_text(compact ? 374.0F : 430.0F, h - 28.0F, compact ? 12.0F : 14.0F, (float[4]){0.64F, 0.90F, 1.0F, 1.0F}, "LV 1  EXP 2/12");
    if (!compact) {
        draw_text(562.0F, h - 28.0F, 14.0F, (float[4]){0.72F, 0.74F, 0.86F, 1.0F}, "AUTO RUNNING");
    }

    char line[128];
    (void)snprintf(line, sizeof(line), "Stone %d    Copper %d    Coins %d", s_game.stone, s_game.copper_ore, s_game.coins);
    draw_text(compact ? 22.0F : w - 360.0F, compact ? h - 58.0F : h - 28.0F, compact ? 12.0F : 16.0F, (float[4]){1.0F, 0.92F, 0.62F, 1.0F}, line);

    draw_text(s_player_box.x + 16.0F, s_player_box.y + s_player_box.h - 28.0F, 18.0F, (float[4]){0.88F, 1.0F, 0.62F, 1.0F}, "NOW MINING");
    char stage_label[64];
    (void)snprintf(stage_label, sizeof(stage_label), "%s runs automatically", k_nodes[s_game.selected_node].label);
    const UiBox action_box = stage_action_box(w, h);
    draw_text(portrait ? s_player_box.x + 16.0F : action_box.x + action_box.w - 360.0F,
              portrait ? s_player_box.y + s_player_box.h - 52.0F : action_box.y + action_box.h - 40.0F,
              portrait ? 11.0F : 13.0F,
              (float[4]){0.64F, 0.72F, 0.82F, 1.0F},
              stage_label);

    draw_text(s_activity_box.x + 12.0F,
              s_activity_box.y + (s_activity_box.h < 34.0F ? 10.0F : 13.0F),
              s_activity_box.h < 34.0F ? 10.0F : 12.0F,
              (float[4]){0.88F, 1.0F, 0.62F, 1.0F},
              "1. MINING NOW");
    const float locked_x = portrait ? s_activity_box.x + 10.0F : s_activity_box.x + s_activity_box.w + 18.0F;
    const float locked_y = portrait ? s_activity_box.y - 24.0F : s_activity_box.y + 12.0F;
    draw_text(locked_x,
              locked_y,
              compact ? 9.0F : 10.0F,
              (float[4]){0.46F, 0.46F, 0.54F, 1.0F},
              "LATER: WOODCUTTING / FISHING / SMITHING");

    for (int i = 0; i < 2; ++i) {
        const UiBox box = s_node_boxes[i];
        draw_text(box.x + 48.0F, box.y + box.h - (compact ? 21.0F : 23.0F), compact ? 13.0F : 15.0F, (float[4]){0.94F, 0.90F, 1.0F, 1.0F}, k_nodes[i].label);
        if (s_game.selected_node == i) {
            draw_text(box.x + box.w - (compact ? 110.0F : 136.0F), box.y + box.h - (compact ? 21.0F : 23.0F), compact ? 10.0F : 11.0F, (float[4]){0.88F, 1.0F, 0.62F, 1.0F}, "RUNNING");
        } else if (!node_unlocked(i)) {
            draw_text(box.x + box.w - 100.0F, box.y + box.h - (compact ? 21.0F : 23.0F), compact ? 9.0F : 10.0F, (float[4]){0.70F, 0.62F, 0.58F, 1.0F}, "LV2 LOCK");
        }
        if (!compact || is_portrait_layout(w, h)) {
            draw_text(box.x + 48.0F, box.y + 10.0F, 9.0F, (float[4]){0.70F, 0.72F, 0.82F, 1.0F}, k_nodes[i].yield_label);
        }
    }

    const bool roomy_upgrade = s_upgrade_box.w >= 238.0F && s_upgrade_box.h >= 138.0F;
    draw_text(s_upgrade_box.x + 16.0F, s_upgrade_box.y + s_upgrade_box.h - (compact ? 22.0F : 26.0F), roomy_upgrade ? 11.0F : (compact ? 10.0F : 11.0F), (float[4]){0.88F, 1.0F, 0.62F, 1.0F}, upgrade_affordable() ? "NEXT ACTION" : "NEXT ACTION LOCKED");
    draw_text(s_upgrade_box.x + 16.0F, s_upgrade_box.y + s_upgrade_box.h - (compact ? 42.0F : 50.0F), roomy_upgrade ? 16.0F : (compact ? 14.0F : 17.0F), (float[4]){0.94F, 0.90F, 1.0F, 1.0F}, "Copper Pickaxe");
    if (s_game.copper_pickaxe) {
        draw_text(s_upgrade_box.x + 16.0F, s_upgrade_box.y + s_upgrade_box.h - (compact ? 64.0F : 74.0F), roomy_upgrade ? 11.0F : (compact ? 10.0F : 11.0F), (float[4]){0.72F, 0.90F, 0.78F, 1.0F}, "Mining speed +15%");
    } else if (upgrade_affordable()) {
        draw_text(s_upgrade_box.x + 16.0F, s_upgrade_box.y + s_upgrade_box.h - (compact ? 64.0F : 74.0F), roomy_upgrade ? 11.0F : (compact ? 10.0F : 11.0F), (float[4]){0.88F, 1.0F, 0.62F, 1.0F}, "Press Upgrade Pickaxe");
    } else {
        draw_text(s_upgrade_box.x + 16.0F, s_upgrade_box.y + s_upgrade_box.h - (compact ? 64.0F : 70.0F), roomy_upgrade ? 10.0F : (compact ? 9.0F : 10.0F), (float[4]){0.72F, 0.74F, 0.86F, 1.0F}, "Keep mining to unlock");
    }
    if (!s_game.copper_pickaxe) {
        char row[64];
        const float row_x = s_upgrade_box.x + 36.0F;
        const float row_y = s_upgrade_box.y + s_upgrade_box.h - (compact ? 77.0F : 80.0F);
        const float row_gap = roomy_upgrade ? 14.0F : (compact ? 12.0F : 13.0F);
        const float row_size = roomy_upgrade ? 10.0F : (compact ? 9.0F : 10.0F);
        (void)snprintf(row, sizeof(row), missing_stone() == 0 ? "Stone ready" : "Need %d Stone", missing_stone());
        draw_text(row_x, row_y, row_size, missing_stone() == 0 ? (float[4]){0.76F, 1.0F, 0.68F, 1.0F} : (float[4]){0.98F, 0.86F, 0.68F, 1.0F}, row);
        (void)snprintf(row, sizeof(row), missing_copper() == 0 ? "Copper ready" : "Need %d Copper", missing_copper());
        draw_text(row_x, row_y - row_gap, row_size, missing_copper() == 0 ? (float[4]){0.76F, 1.0F, 0.68F, 1.0F} : (float[4]){0.98F, 0.80F, 0.60F, 1.0F}, row);
        (void)snprintf(row, sizeof(row), missing_coins() == 0 ? "Coins ready" : "Need %d Coins", missing_coins());
        draw_text(row_x, row_y - row_gap * 2.0F, row_size, missing_coins() == 0 ? (float[4]){0.76F, 1.0F, 0.68F, 1.0F} : (float[4]){1.0F, 0.92F, 0.62F, 1.0F}, row);
    }

    if (upgrade_affordable() || s_game.copper_pickaxe) {
        const char *button_label = s_game.copper_pickaxe ? "EQUIPPED" : "UPGRADE PICKAXE";
        draw_text(s_primary_box.x + 18.0F, s_primary_box.y + (compact ? 9.0F : 17.0F), roomy_upgrade ? 11.0F : (compact ? 10.0F : 13.0F), (float[4]){1.0F, 1.0F, 0.96F, 1.0F}, button_label);
    }

    char progress[96];
    (void)snprintf(progress, sizeof(progress), "%s: %.1fs / %.1fs", k_nodes[s_game.selected_node].label, (double)s_game.progress_seconds, (double)active_interval_seconds());
    const float progress_x = s_stage_box.x + s_stage_box.w - (compact ? 220.0F : 260.0F);
    draw_text(progress_x, s_stage_box.y + 36.0F, compact ? 11.0F : 12.0F, (float[4]){0.86F, 0.92F, 1.0F, 1.0F}, progress);
    const bool show_stage_callout = s_game.callout_timer > 0.0F && s_game.callout_text[0] != '\0' && s_game.hit_feedback_timer <= 0.0F;
    if (show_stage_callout) {
        const UiBox callout = stage_callout_box(w, h);
        draw_text(callout.x + 14.0F, callout.y + 12.0F, 12.0F, (float[4]){0.96F, 1.0F, 0.94F, 1.0F}, s_game.callout_text);
    }
    if (s_game.hit_feedback_timer > 0.0F && s_game.hit_feedback_text[0] != '\0') {
        const UiBox hit_box = hit_feedback_box(w, h);
        draw_text(hit_box.x + 12.0F, hit_box.y + 9.0F, 11.0F, (float[4]){1.0F, 0.96F, 0.54F, 1.0F}, s_game.hit_feedback_text);
    }

    nt_text_renderer_flush();
}

static void handle_input(const nt_ui_scale_t *ui_scale) {
    if (nt_input_key_is_pressed(NT_KEY_1)) {
        select_node(0);
    }
    if (nt_input_key_is_pressed(NT_KEY_2)) {
        select_node(1);
    }
    if ((nt_input_key_is_pressed(NT_KEY_SPACE) || nt_input_key_is_pressed(NT_KEY_ENTER)) && upgrade_affordable()) {
        buy_upgrade();
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        for (int i = 0; i < NT_INPUT_MAX_POINTERS; ++i) {
            const nt_pointer_t pointer = nt_ui_scale_apply_pointer(ui_scale, g_nt_input.pointers[i]);
            if (!pointer.active) {
                continue;
            }
            const float pointer_x = pointer.x;
            const float pointer_y = ui_scale->logical_h - pointer.y;
            if (contains(s_node_boxes[0], pointer_x, pointer_y)) {
                select_node(0);
                return;
            }
            if (contains(s_node_boxes[1], pointer_x, pointer_y)) {
                select_node(1);
                return;
            }
            if (upgrade_affordable() && contains(s_primary_box, pointer_x, pointer_y)) {
                buy_upgrade();
                return;
            }
        }
    }
}

#if SKELETAL_ANIMATION_EXTENSION_ENABLED
static void init_skeletal_extension(void) {
    char error[512];
    if (nt_skeletal_anim_load_ozz(
            "gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_runtime/rig_medium_skeleton.ozz",
            "gamedesign/projects/mine-cards/visual/skeletal_spike/ozz_runtime/rig_medium_pickaxing.ozz",
            &s_skeletal_clip,
            error,
            sizeof(error))) {
        s_skeletal_ready = true;
        (void)fprintf(stderr,
                      "skeletal extension: loaded joints=%d tracks=%d duration=%.3f\n",
                      nt_skeletal_anim_joint_count(s_skeletal_clip),
                      nt_skeletal_anim_track_count(s_skeletal_clip),
                      (double)nt_skeletal_anim_duration(s_skeletal_clip));
        mine_cards_model_proof_bind_skeleton(s_skeletal_clip);
    } else {
        (void)fprintf(stderr, "skeletal extension: %s\n", error);
    }
}

static float mining_animation_sample_time(void) {
    const float duration = nt_skeletal_anim_duration(s_skeletal_clip);
    if (duration <= 0.0F) {
        return 0.0F;
    }

    if (s_game.hit_feedback_timer > 0.0F) {
        const float hit_age = MINE_CARDS_HIT_FEEDBACK_SECONDS - s_game.hit_feedback_timer;
        return fmodf(0.45F + hit_age * 0.65F, duration);
    }

    const float interval = active_interval_seconds();
    const float progress = interval > 0.0F ? clamp01(s_game.progress_seconds / interval) : 0.0F;
    const float cycle = fmodf(progress * 2.2F, 1.0F);
    return cycle * 1.75F;
}

static void update_skeletal_extension(void) {
    if (!s_skeletal_ready) {
        return;
    }

    char error[512];
    const float t = mining_animation_sample_time();
    if (!nt_skeletal_anim_sample_attachment(s_skeletal_clip, "head", t, &s_skeletal_head, error, sizeof(error)) ||
        !nt_skeletal_anim_sample_attachment(s_skeletal_clip, "handslot.l", t, &s_skeletal_hand_l, error, sizeof(error)) ||
        !nt_skeletal_anim_sample_attachment(s_skeletal_clip, "handslot.r", t, &s_skeletal_hand_r, error, sizeof(error))) {
        if (!s_skeletal_reported) {
            (void)fprintf(stderr, "skeletal extension sample failed: %s\n", error);
            s_skeletal_reported = true;
        }
        return;
    }

    s_skeletal_model_matrix_count = nt_skeletal_anim_copy_model_matrices(
        s_skeletal_clip,
        s_skeletal_model_matrices,
        MINE_CARDS_SKELETAL_MAX_MATRICES,
        error,
        sizeof(error));
    if (s_skeletal_model_matrix_count <= 0 && !s_skeletal_reported) {
        (void)fprintf(stderr, "skeletal extension matrix copy failed: %s\n", error);
        s_skeletal_reported = true;
        return;
    }

    if (!s_skeletal_reported) {
        (void)fprintf(stderr,
                      "skeletal extension: first sample head=[%.3f %.3f %.3f] hand.l=[%.3f %.3f %.3f]\n",
                      (double)s_skeletal_head.model_position.x,
                      (double)s_skeletal_head.model_position.y,
                      (double)s_skeletal_head.model_position.z,
                      (double)s_skeletal_hand_l.model_position.x,
                      (double)s_skeletal_hand_l.model_position.y,
                      (double)s_skeletal_hand_l.model_position.z);
        s_skeletal_reported = true;
    }
}
#endif

static void init_text_runtime(void) {
    nt_gfx_register_global_block("Globals", 0);
    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture);
    nt_atlas_init();

    nt_material_init(&(nt_material_desc_t){.max_materials = 8});
    nt_font_init(&(nt_font_desc_t){.max_fonts = 2});
    nt_sprite_renderer_init(&(nt_sprite_renderer_desc_t){.max_pipelines = 8});
    nt_text_renderer_init();
    s_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "mine_cards_ui_globals",
    });

    s_text_pack_id = nt_hash32_str("mine_cards_text");
    nt_resource_mount(s_text_pack_id, 10);
    nt_resource_load_auto(s_text_pack_id, "assets/mine_cards_text.ntpack");
    s_ui_pack_id = nt_hash32_str("mine_cards_ui");
    nt_resource_mount(s_ui_pack_id, 11);
    nt_resource_load_auto(s_ui_pack_id, "assets/mine_cards_ui.ntpack");
    nt_resource_set_activate_time_budget(0.0F);

    nt_resource_t vs = nt_resource_request(nt_hash64_str("external/neotolis-engine/assets/shaders/slug_text.vert"), NT_ASSET_SHADER_CODE);
    nt_resource_t fs = nt_resource_request(nt_hash64_str("external/neotolis-engine/assets/shaders/slug_text.frag"), NT_ASSET_SHADER_CODE);
    s_text_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = vs,
        .fs = fs,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = 0,
        .params[0] = {.name = "u_alpha_cutoff", .value = {NT_TEXT_ALPHA_CUTOFF_DEFAULT}},
        .param_count = 1,
        .label = "mine_cards_ui_text",
    });

    nt_resource_t sprite_vs = nt_resource_request(nt_hash64_str("external/neotolis-engine/assets/shaders/sprite.vert"), NT_ASSET_SHADER_CODE);
    nt_resource_t sprite_fs = nt_resource_request(nt_hash64_str("external/neotolis-engine/assets/shaders/sprite.frag"), NT_ASSET_SHADER_CODE);
    nt_resource_t atlas_tex0 = nt_resource_request(nt_hash64_str("mine-cards/ui_atlas/tex0"), NT_ASSET_TEXTURE);
    s_ui_atlas = nt_resource_request(nt_hash64_str("mine-cards/ui_atlas"), NT_ASSET_ATLAS);
    s_ui_sprite_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = sprite_vs,
        .fs = sprite_fs,
        .textures = {{.name = "u_texture", .resource = atlas_tex0}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = 0,
        .label = "mine_cards_ui_sprites",
    });

    s_ui_font = nt_font_create(&(nt_font_create_desc_t){
        .curve_texture_width = 1024,
        .curve_texture_height = 512,
        .band_texture_height = 256,
        .band_count = 8,
        .measure_cache_size = 256,
    });
    nt_font_add(s_ui_font, nt_resource_request(nt_hash64_str("mine-cards/font_ui"), NT_ASSET_FONT));
}

static void update_text_globals(float logical_w, float logical_h, float fb_w, float fb_h) {
    mat4 vp;
    mat4 ident;
    ortho(0.0F, logical_w, 0.0F, logical_h, -1.0F, 1.0F, (float *)vp);
    nt_frame_uniforms_t uniforms;
    memset(&uniforms, 0, sizeof(uniforms));
    memcpy(uniforms.view_proj, vp, sizeof(uniforms.view_proj));
    memcpy(uniforms.view, vp, sizeof(uniforms.view));
    glm_mat4_identity(ident);
    memcpy(uniforms.proj, ident, sizeof(uniforms.proj));
    uniforms.time[0] = (float)nt_time_now();
    uniforms.time[1] = g_nt_app.dt;
    uniforms.resolution[0] = fb_w;
    uniforms.resolution[1] = fb_h;
    uniforms.resolution[2] = fb_w > 0.0F ? 1.0F / fb_w : 0.0F;
    uniforms.resolution[3] = fb_h > 0.0F ? 1.0F / fb_h : 0.0F;
    uniforms.near_far[0] = -1.0F;
    uniforms.near_far[1] = 1.0F;
    nt_gfx_update_buffer(s_frame_ubo, &uniforms, sizeof(uniforms));
    nt_gfx_bind_uniform_buffer(s_frame_ubo, 0);
}

#if NT_DEVAPI_ENABLED
static bool write_framebuffer_ppm(const char *path, int w, int h, char *error, int error_cap) {
    if (path == NULL || path[0] == '\0') {
        (void)snprintf(error, (size_t)error_cap, "missing output path");
        return false;
    }
    if (w <= 0 || h <= 0) {
        (void)snprintf(error, (size_t)error_cap, "invalid framebuffer size %dx%d", w, h);
        return false;
    }

    const size_t row_bytes = (size_t)w * 3U;
    const size_t pixel_bytes = row_bytes * (size_t)h;
    uint8_t *pixels = (uint8_t *)malloc(pixel_bytes);
    uint8_t *flipped = (uint8_t *)malloc(pixel_bytes);
    if (pixels == NULL || flipped == NULL) {
        free(pixels);
        free(flipped);
        (void)snprintf(error, (size_t)error_cap, "out of memory for framebuffer capture");
        return false;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    for (int y = 0; y < h; ++y) {
        memcpy(flipped + (size_t)y * row_bytes, pixels + (size_t)(h - 1 - y) * row_bytes, row_bytes);
    }

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        free(pixels);
        free(flipped);
        (void)snprintf(error, (size_t)error_cap, "failed to open output path");
        return false;
    }
    (void)fprintf(f, "P6\n%d %d\n255\n", w, h);
    const size_t written = fwrite(flipped, 1U, pixel_bytes, f);
    (void)fclose(f);
    free(pixels);
    free(flipped);
    if (written != pixel_bytes) {
        (void)snprintf(error, (size_t)error_cap, "short framebuffer write");
        return false;
    }
    return true;
}

static int json_int_or_default(const cJSON *params, const char *key, int fallback) {
    const cJSON *item = params != NULL ? cJSON_GetObjectItemCaseSensitive(params, key) : NULL;
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static cJSON *state_json(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "runtime", "mine_cards_mining_v001");
    cJSON_AddStringToObject(root, "selected_node", k_nodes[s_game.selected_node].id);
    cJSON_AddNumberToObject(root, "progress_seconds", (double)s_game.progress_seconds);
    cJSON_AddNumberToObject(root, "interval_seconds", (double)active_interval_seconds());
    cJSON_AddNumberToObject(root, "stone", s_game.stone);
    cJSON_AddNumberToObject(root, "copper_ore", s_game.copper_ore);
    cJSON_AddNumberToObject(root, "coins", s_game.coins);
    cJSON_AddNumberToObject(root, "mining_xp", s_game.mining_xp);
    cJSON_AddNumberToObject(root, "mining_level", s_game.mining_level);
    cJSON_AddBoolToObject(root, "copper_pickaxe", s_game.copper_pickaxe);
    cJSON_AddBoolToObject(root, "upgrade_affordable", upgrade_affordable());
    const bool ui_ready_now = ui_assets_ready();
    const nt_material_info_t *ui_info = nt_material_get_info(s_ui_sprite_material);
    cJSON_AddBoolToObject(root, "ui_sprite_material_ready", ui_info != NULL && ui_info->ready);
    cJSON_AddBoolToObject(root, "ui_atlas_ready", nt_resource_is_ready(s_ui_atlas));
    cJSON_AddNumberToObject(root, "ui_missing_region", s_ui_missing_region);
    if (s_ui_missing_region >= 0 && s_ui_missing_region < (int)MINE_CARDS_UI_REGION_COUNT) {
        cJSON_AddStringToObject(root, "ui_missing_region_name", k_ui_region_names[s_ui_missing_region]);
    }
    cJSON_AddBoolToObject(root, "ui_assets_ready", ui_ready_now);
    cJSON_AddBoolToObject(root, "ui_regions_ready", s_ui_regions_ready);
    cJSON_AddNumberToObject(root, "missing_stone", s_game.stone < 6 ? 6 - s_game.stone : 0);
    cJSON_AddNumberToObject(root, "missing_copper_ore", s_game.copper_ore < 32 ? 32 - s_game.copper_ore : 0);
    cJSON_AddNumberToObject(root, "missing_coins", s_game.coins < 32 ? 32 - s_game.coins : 0);
    cJSON_AddStringToObject(root, "callout", s_game.callout_timer > 0.0F ? s_game.callout_text : "");
    cJSON_AddStringToObject(root, "hit_feedback", s_game.hit_feedback_timer > 0.0F ? s_game.hit_feedback_text : "");
    cJSON_AddNumberToObject(root, "hit_feedback_timer", (double)s_game.hit_feedback_timer);
    cJSON *log = cJSON_AddArrayToObject(root, "reward_log");
    if (log != NULL) {
        for (int i = 0; i < MINE_CARDS_REWARD_LOG_ROWS; ++i) {
            cJSON_AddItemToArray(log, cJSON_CreateString(s_game.reward_log[i]));
        }
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
    reset_mine_cards();
    *result = state_json();
    return true;
}

static bool ep_game_action_select_surface(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    select_node(0);
    *result = state_json();
    return true;
}

static bool ep_game_action_select_copper(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    select_node(1);
    *result = state_json();
    return true;
}

static bool ep_game_action_upgrade_pickaxe(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    buy_upgrade();
    *result = state_json();
    return true;
}

static bool ep_game_debug_seed_upgrade_resources(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    s_game.stone = json_int_or_default(params, "stone", 6);
    s_game.copper_ore = json_int_or_default(params, "copper_ore", 32);
    s_game.coins = json_int_or_default(params, "coins", 32);
    if (s_game.mining_xp < 12) {
        s_game.mining_xp = 12;
    }
    s_game.mining_level = 2;
    if (upgrade_affordable()) {
        push_log("Debug: upgrade resources ready");
        set_callout("Copper Pickaxe is affordable");
    } else {
        push_log("Debug: Mining Lv2 unlocked");
        set_callout("Mining Lv2: Copper unlocked");
    }
    *result = state_json();
    return true;
}

static bool ep_game_debug_prepare_geode_tick(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)error;
    (void)error_cap;
    (void)user;
    const int node_index = json_int_or_default(params, "node", 0);
    if (node_index >= 0 && node_index < (int)(sizeof(k_nodes) / sizeof(k_nodes[0])) && node_unlocked(node_index)) {
        s_game.selected_node = node_index;
    }
    s_game.tick_count = 16;
    s_game.progress_seconds = active_interval_seconds();
    *result = state_json();
    return true;
}

static bool ep_game_capture_framebuffer(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    const cJSON *output = params != NULL ? cJSON_GetObjectItemCaseSensitive(params, "output") : NULL;
    if (!cJSON_IsString(output)) {
        (void)snprintf(error, (size_t)error_cap, "missing output");
        return false;
    }
    const int w = (int)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const int h = (int)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    if (!write_framebuffer_ppm(output->valuestring, w, h, error, error_cap)) {
        return false;
    }
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddBoolToObject(obj, "ok", true);
    cJSON_AddStringToObject(obj, "output", output->valuestring);
    cJSON_AddNumberToObject(obj, "width", w);
    cJSON_AddNumberToObject(obj, "height", h);
    *result = obj;
    return true;
}

static void register_game_endpoints(void) {
    nt_devapi_register_builtins();
    nt_devapi_register("game.state", ep_game_state, NULL);
    nt_devapi_register("game.reset_playtest", ep_game_reset_playtest, NULL);
    nt_devapi_register("game.action.select_surface", ep_game_action_select_surface, NULL);
    nt_devapi_register("game.action.select_copper", ep_game_action_select_copper, NULL);
    nt_devapi_register("game.action.upgrade_pickaxe", ep_game_action_upgrade_pickaxe, NULL);
    nt_devapi_register("game.debug.seed_upgrade_resources", ep_game_debug_seed_upgrade_resources, NULL);
    nt_devapi_register("game.debug.prepare_geode_tick", ep_game_debug_prepare_geode_tick, NULL);
    nt_devapi_register("game.capture.framebuffer", ep_game_capture_framebuffer, NULL);
}

static UiBox devapi_box_from_y_up(UiBox box, float h) {
    return (UiBox){box.x, h - box.y - box.h, box.w, box.h};
}

static void register_ui_devapi(float w, float h) {
    nt_devapi_set_frame(g_nt_app.frame);
    nt_devapi_set_view((float)g_nt_window.fb_width, (float)g_nt_window.fb_height, w, h);
    nt_devapi_clear_ui_elements();
    const UiBox surface_box = devapi_box_from_y_up(s_node_boxes[0], h);
    const UiBox copper_box = devapi_box_from_y_up(s_node_boxes[1], h);
    const UiBox primary_box = devapi_box_from_y_up(s_primary_box, h);
    const bool copper_enabled = node_unlocked(1);
    const bool upgrade_enabled = upgrade_affordable() && !s_game.copper_pickaxe;
    (void)nt_devapi_register_ui_node("root", "", "screen", "Mining", "Mine Cards Mining screen", 0.0F, 0.0F, w, h, true, true);
    (void)nt_devapi_register_ui_node("mining.node.surface", "root", "button", "Surface Stone", "Current mining node", surface_box.x, surface_box.y, surface_box.w, surface_box.h, true, true);
    (void)nt_devapi_register_ui_node("mining.node.copper", "root", "button", "Copper Vein", node_unlocked(1) ? "Mine Copper Vein" : "Locked: Mining Lv2", copper_box.x, copper_box.y, copper_box.w,
                                     copper_box.h, true, copper_enabled);
    (void)nt_devapi_register_ui_node("mining.upgrade.pickaxe", "root", "button", "Copper Pickaxe", upgrade_enabled ? "Upgrade Pickaxe" : "Locked until resources are ready", primary_box.x, primary_box.y, primary_box.w, primary_box.h,
                                     true, upgrade_enabled);
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
#if NT_DEVAPI_ENABLED
    if (s_devapi_enabled) {
        nt_devapi_apply_pending();
    }
#endif

    const float fb_w = (float)(g_nt_window.fb_width ? g_nt_window.fb_width : g_nt_window.width);
    const float fb_h = (float)(g_nt_window.fb_height ? g_nt_window.fb_height : g_nt_window.height);
    const nt_ui_scale_desc_t ui_scale_desc = {
        .ref_w = MINE_CARDS_UI_REF_W,
        .ref_h = MINE_CARDS_UI_REF_H,
        .mode = NT_UI_SCALE_EXPAND,
    };
    const nt_ui_scale_t ui_scale = nt_ui_compute_scale(&ui_scale_desc, fb_w, fb_h);
    const float w = ui_scale.logical_w;
    const float h = ui_scale.logical_h;
    layout(w, h);
    handle_input(&ui_scale);
    update_mining(g_nt_app.dt);
    if (s_game.callout_timer > 0.0F) {
        s_game.callout_timer -= g_nt_app.dt;
        if (s_game.callout_timer < 0.0F) {
            s_game.callout_timer = 0.0F;
        }
    }
    if (s_game.hit_feedback_timer > 0.0F) {
        s_game.hit_feedback_timer -= g_nt_app.dt;
        if (s_game.hit_feedback_timer < 0.0F) {
            s_game.hit_feedback_timer = 0.0F;
        }
    }

    if (s_rich_render_initialized) {
#if SKELETAL_ANIMATION_EXTENSION_ENABLED
        update_skeletal_extension();
        mine_cards_model_proof_step_ozz(s_skeletal_model_matrices, s_skeletal_model_matrix_count);
#else
        mine_cards_model_proof_step(0.5F, 0.5F);
#endif

        nt_resource_step();
        nt_material_step();
        nt_font_step();
    }

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
        nt_sprite_renderer_restore_gpu();
        nt_text_renderer_restore_gpu();
        mine_cards_model_proof_restore_gpu();
        s_ui_regions_ready = false;
    }
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.82F, 0.89F, 0.84F, 1.0F}, .clear_depth = 1.0F});

    float shape_vp[16];
    ortho(0.0F, w, 0.0F, h, -1.0F, 1.0F, shape_vp);
    nt_shape_renderer_set_vp(shape_vp);
    nt_shape_renderer_set_cam_pos((float[3]){0.0F, 0.0F, 1.0F});
    nt_shape_renderer_set_depth(false);
    nt_shape_renderer_set_line_width(2.0F);
    draw_ui_shapes(w, h);
    nt_shape_renderer_flush();

    update_text_globals(w, h, fb_w, fb_h);
    draw_ui_sprites(w, h);

    const UiBox actor_box = stage_actor_box(w, h);
    const UiBox actor_physical_box = ui_box_to_physical(&ui_scale, actor_box);
    mine_cards_model_proof_draw_in_box(fb_w, fb_h, actor_physical_box.x, actor_physical_box.y, actor_physical_box.w, actor_physical_box.h);

    update_text_globals(w, h, fb_w, fb_h);
    draw_stage_overlay_sprites(w, h);

    update_text_globals(w, h, fb_w, fb_h);
    draw_ui_text(w, h);

    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "Mine Cards";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    parse_args(argc, argv);
    reset_mine_cards();

    g_nt_window.title = "Mine Cards";
    g_nt_window.width = (uint32_t)s_window_width;
    g_nt_window.height = (uint32_t)s_window_height;
    nt_window_init();
    nt_input_init();

    nt_gfx_desc_t gfx_desc = nt_gfx_desc_defaults();
    gfx_desc.depth = true;
    nt_gfx_init(&gfx_desc);
    nt_shape_renderer_init();
    init_text_runtime();
    mine_cards_model_proof_init();

#if SKELETAL_ANIMATION_EXTENSION_ENABLED
    init_skeletal_extension();
#endif
    s_rich_render_initialized = true;

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
    if (s_rich_render_initialized) {
        nt_text_renderer_shutdown();
        nt_sprite_renderer_shutdown();
        nt_font_destroy(s_ui_font);
        nt_font_shutdown();
        nt_material_destroy(s_ui_sprite_material);
        nt_material_destroy(s_text_material);
        nt_material_shutdown();
        nt_resource_shutdown();
        nt_fs_shutdown();
        nt_http_shutdown();
        nt_hash_shutdown();
        nt_gfx_destroy_buffer(s_frame_ubo);
        mine_cards_model_proof_shutdown();
#if SKELETAL_ANIMATION_EXTENSION_ENABLED
        nt_skeletal_anim_destroy(s_skeletal_clip);
#endif
    }
    nt_shape_renderer_shutdown();
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif

    return 0;
}
