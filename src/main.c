#include "app/nt_app.h"
#include "core/nt_core.h"
#include "core/nt_platform.h"
#include "devapi/nt_devapi.h"
#include "game_storage.h"
#include "game_state_actions.h"
#include "atlas/nt_atlas.h"
#include "fs/nt_fs.h"
#include "generated/game_state.h"
#include "generated/game_67_idle_assets.h"
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
#include "resource/nt_resource.h"
#include "window/nt_window.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NT_PLATFORM_WEB
#include <emscripten/emscripten.h>
#include "platform/web/nt_platform_web.h"
#else
#include <glad/gl.h>
#endif

#define GAME_STORAGE_NAMESPACE "game_67_idle"
#define GAME_STORAGE_NATIVE_ROOT "build/saves"
#define GAME_AUTOSAVE_KEY "autosave"

#define FIRST_UPGRADE_COST 5
#define SECOND_UPGRADE_COST 8
#define THIRD_UPGRADE_COST 20
#define FOURTH_UPGRADE_COST 35
#define FIFTH_UPGRADE_COST 60
#define FIRST_JOB_DURATION_MS 6000
#define SECOND_JOB_DURATION_MS 8000
#define SECOND_JOB_REWARD 30
#define SECOND_JOB_ID "sticker_run"
#define THIRD_JOB_DURATION_MS 10000
#define THIRD_JOB_REWARD 90
#define THIRD_JOB_ID "meme_stand_owner"
#define UI_TOP_SAFE_MIN 24.0F
#define UI_BOTTOM_SAFE_MIN 28.0F

#define SETTINGS_BUTTON_W 48.0F
#define SETTINGS_BUTTON_H 48.0F
#define SETTINGS_BUTTON_MARGIN 16.0F
#define SETTINGS_MODAL_W 340.0F
#define SETTINGS_MODAL_H 230.0F
#define SETTINGS_CLOSE_W 40.0F
#define SETTINGS_CLOSE_H 34.0F
#define SETTINGS_SLIDER_W 220.0F
#define SETTINGS_SLIDER_H 34.0F

typedef struct UiRect {
    float x;
    float y;
    float w;
    float h;
} UiRect;

typedef struct CardSlots {
    UiRect icon;
    float label_x;
    float label_y;
    float value_x;
    float value_y;
    float progress_x;
    float progress_y;
    float progress_w;
} CardSlots;

static bool s_settings_open;
static int s_active_settings_slider;
static bool s_fresh_state;
static bool s_autosave_disabled;
static bool s_autosave_ready;
static bool s_autosave_load_done;
static bool s_autosave_sync_pending;
static bool s_autosave_sync_failed;
#ifdef NT_PLATFORM_WEB
static bool s_web_page_paused;
static bool s_web_flush_requested;
#endif
#ifndef NT_PLATFORM_WEB
static bool s_devapi_started;
static bool s_frame_capture_pending;
static char s_frame_capture_path[512];
#endif

static UiRect s_do67_rect;
static UiRect s_upgrade_rect;
static UiRect s_job_rect;
static UiRect s_claim_rect;
static UiRect s_reset_rect;
static UiRect s_coin_pill_rect;
static UiRect s_status_pill_rect;
static UiRect s_goal_rect;

static nt_buffer_t s_sprite_frame_ubo;
static nt_hash32_t s_art_pack_id;
static nt_resource_t s_art_atlas;
static nt_resource_t s_sprite_vs;
static nt_resource_t s_sprite_fs;
static nt_resource_t s_art_atlas_texture;
static nt_material_t s_sprite_material;
static bool s_art_resolved;
static uint32_t s_bg_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_hero_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_badge_power_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_button_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_card_job_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_card_upgrade_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_icon_coin_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_icon_next_goal_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_icon_tap_hand_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_pill_coin_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_pill_tap_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_tab_city_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_tab_home_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_first_action_plate_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_first_status_badge_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_runtime_goal_card_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_runtime_progress_bar_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_runtime_resource_pill_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_runtime_tab_locked_region = NT_ATLAS_INVALID_REGION;
static uint32_t s_runtime_tab_selected_region = NT_ATLAS_INVALID_REGION;

static int status_value(void) { return g_game_state.status; }
static bool first_upgrade_owned(void) { return g_game_state.first_upgrade_owned; }
static bool second_upgrade_owned(void) { return g_game_state.second_upgrade_owned; }
static bool third_upgrade_owned(void) { return g_game_state.third_upgrade_owned; }
static bool fourth_upgrade_owned(void) { return g_game_state.fourth_upgrade_owned; }
static bool fifth_upgrade_owned(void) { return g_game_state.fifth_upgrade_owned; }
static bool job_active(void) { return g_game_state.active_job_id[0] != '\0'; }
static bool job_ready(void) { return job_active() && g_game_state.active_job_elapsed_ms >= g_game_state.active_job_duration_ms; }
static bool second_job_unlocked(void) { return third_upgrade_owned() || status_value() >= 6; }
static bool second_job_active(void) { return strcmp(g_game_state.active_job_id, SECOND_JOB_ID) == 0; }
static bool third_job_unlocked(void) { return fifth_upgrade_owned() || status_value() >= 10; }
static bool third_job_active(void) { return strcmp(g_game_state.active_job_id, THIRD_JOB_ID) == 0; }
static bool runtime_art_ready(void);
static float settings_button_x(void);
static float settings_button_y(void);
static float ui_bottom_safe(void);
#ifdef NT_PLATFORM_WEB
static void game67_web_install_page_lifecycle(void);
#endif

#ifdef NT_PLATFORM_WEB
EM_JS(void, game67_web_qa_snapshot_js, (int meme_coins, int status, int click_power, int first_upgrade_owned, int active_job_elapsed_ms, int active_job_duration_ms, int income_per_second, int comfort, int visual_stage, int feedback_code), {
    var state = {
        meme_coins: meme_coins,
        status: status,
        click_power: click_power,
        first_upgrade_owned: !!first_upgrade_owned,
        active_job_elapsed_ms: active_job_elapsed_ms,
        active_job_duration_ms: active_job_duration_ms,
        income_per_second: income_per_second,
        comfort: comfort,
        visual_stage: visual_stage,
        feedback_code: feedback_code
    };
    state.first_job_active = active_job_duration_ms > 0;
    state.first_job_ready = state.first_job_active && active_job_elapsed_ms >= active_job_duration_ms;
    var snapshot = {
        schema: 'game_67_idle.qa_snapshot',
        gameSchema: 'game_67_idle.state',
        document: 'game',
        source: 'live-wasm',
        state: state
    };
    window.Game67QA = window.Game67QA || {};
    window.Game67QA._snapshot = snapshot;
    window.Game67QA.snapshot = function snapshotGame67QA() {
        return JSON.parse(JSON.stringify(window.Game67QA._snapshot));
    };
    window.__game67QaState = state;
    try {
        localStorage.setItem('game_67_idle.qa_state', JSON.stringify({
            schema: 'game_67_idle.qa_state',
            state: state
        }));
    } catch (e) {}
})
#endif

static void update_web_qa_snapshot(void) {
#ifdef NT_PLATFORM_WEB
    game67_web_qa_snapshot_js(
        g_game_state.meme_coins,
        g_game_state.status,
        g_game_state.click_power,
        g_game_state.first_upgrade_owned ? 1 : 0,
        g_game_state.active_job_elapsed_ms,
        g_game_state.active_job_duration_ms,
        g_game_state.income_per_second,
        g_game_state.comfort,
        g_game_state.visual_stage,
        g_game_state.feedback_code);
#endif
}

static bool has_arg(int argc, char **argv, const char *name) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], name) == 0) {
            return true;
        }
    }
    return false;
}

static float clamp01(float value) {
    if (value < 0.0F) {
        return 0.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

static bool point_in_rect(float px, float py, UiRect r) { return px >= r.x && px <= r.x + r.w && py >= r.y && py <= r.y + r.h; }

static float ui_bottom_safe(void) {
    const float h = (float)g_nt_window.fb_height;
    return fmaxf(UI_BOTTOM_SAFE_MIN, h * 0.034F);
}

static CardSlots card_slots(UiRect r) {
    const float value_w = fminf(88.0F, fmaxf(70.0F, r.w * 0.22F));
    const float progress_x = r.x + 82.0F;
    const float progress_right = r.x + r.w - value_w - 14.0F;
    CardSlots slots = {
        .icon = {r.x + 18.0F, r.y + 13.0F, 42.0F, 42.0F},
        .label_x = r.x + 78.0F,
        .label_y = r.y + 15.0F,
        .value_x = r.x + r.w - value_w,
        .value_y = r.y + 21.0F,
        .progress_x = progress_x,
        .progress_y = r.y + 51.0F,
        .progress_w = fmaxf(72.0F, progress_right - progress_x),
    };
    return slots;
}

static float bitmap_text_width(const char *text, float scale) {
    float width = 0.0F;
    for (int i = 0; text[i] != '\0'; i++) {
        width += (text[i] == ' ') ? scale * 4.0F : scale * 6.0F;
    }
    return width;
}

static void autosave_init(bool fresh_state, bool disabled) {
    s_fresh_state = fresh_state;
    s_autosave_disabled = disabled;
    s_autosave_load_done = fresh_state;
    s_autosave_ready = !disabled;
    s_autosave_sync_pending = false;
    s_autosave_sync_failed = false;
}

static void autosave_try_load(void) {
    if (s_autosave_disabled || !s_autosave_ready || s_autosave_load_done) {
        return;
    }
    s_autosave_load_done = true;
    if (s_fresh_state || game_state_is_dirty()) {
        return;
    }
    char error[128] = {0};
    char *data = NULL;
    if (!game_storage_load_json(GAME_AUTOSAVE_KEY, GAME_STATE_DOCUMENT, &data, error, (int)sizeof(error))) {
        return;
    }
    if (game_state_load_json_string(&g_game_state, data, error, (int)sizeof(error))) {
        game_state_clear_dirty();
    }
    free(data);
}

static void autosave_flush_if_dirty(void) {
    if (s_autosave_disabled) {
        return;
    }
    autosave_try_load();
    if (!s_autosave_ready || s_autosave_sync_pending || !game_state_is_dirty()) {
        return;
    }
    char error[128] = {0};
    char *data = game_state_save_json_string(&g_game_state, error, (int)sizeof(error));
    if (!data) {
        return;
    }
    if (!game_storage_save_json(GAME_AUTOSAVE_KEY, GAME_STATE_DOCUMENT, data, error, (int)sizeof(error))) {
        cJSON_free(data);
        s_autosave_sync_failed = true;
        return;
    }
    cJSON_free(data);
    s_autosave_sync_failed = false;
    game_state_clear_dirty();
}

#ifdef NT_PLATFORM_WEB
EMSCRIPTEN_KEEPALIVE void game67_web_notify_page_state(int paused, int flush) {
    s_web_page_paused = paused != 0;
    if (flush != 0) {
        s_web_flush_requested = true;
        autosave_flush_if_dirty();
        update_web_qa_snapshot();
    }
}

EM_JS(void, game67_web_install_page_lifecycle_js, (void), {
    if (window.Game67Lifecycle && window.Game67Lifecycle.installed) {
        return;
    }
    var state = {
        installed: true,
        paused: false,
        flushes: 0,
        events: []
    };
    window.Game67Lifecycle = state;
    function notify(paused, flush, source) {
        state.paused = !!paused;
        if (flush) {
            state.flushes += 1;
        }
        state.events.push({
            source: source,
            paused: !!paused,
            flush: !!flush,
            time: Date.now()
        });
        if (state.events.length > 24) {
            state.events.shift();
        }
        try {
            if (typeof _game67_web_notify_page_state === 'function') {
                _game67_web_notify_page_state(paused ? 1 : 0, flush ? 1 : 0);
            }
        } catch (e) {}
    }
    document.addEventListener('visibilitychange', function() {
        notify(!!document.hidden, !!document.hidden, 'visibilitychange');
    });
    window.addEventListener('pagehide', function() {
        notify(true, true, 'pagehide');
    });
    window.addEventListener('blur', function() {
        notify(true, true, 'blur');
    });
    window.addEventListener('focus', function() {
        notify(false, false, 'focus');
    });
    notify(!!document.hidden, false, 'install');
})

static void game67_web_install_page_lifecycle(void) {
    game67_web_install_page_lifecycle_js();
}
#endif

#if NT_DEVAPI_ENABLED
static bool parse_devapi_port(int argc, char **argv, uint16_t *out_port) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--devapi") == 0) {
            long port = 9123;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                port = strtol(argv[i + 1], NULL, 10);
            }
            if (port <= 0 || port > 65535) {
                port = 9123;
            }
            *out_port = (uint16_t)port;
            return true;
        }
        if (strncmp(argv[i], "--devapi=", 9) == 0) {
            long port = strtol(argv[i] + 9, NULL, 10);
            if (port <= 0 || port > 65535) {
                port = 9123;
            }
            *out_port = (uint16_t)port;
            return true;
        }
    }
    return false;
}

static bool ep_game_state(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)error;
    (void)error_cap;
    (void)user;
    cJSON *obj = game_state_to_json(&g_game_state);
    cJSON_AddNumberToObject(obj, "time", (double)g_nt_app.time);
    cJSON_AddNumberToObject(obj, "frame", (double)g_nt_app.frame);
    cJSON_AddBoolToObject(obj, "state_dirty", game_state_is_dirty());
    cJSON_AddBoolToObject(obj, "autosave_ready", s_autosave_ready);
    cJSON_AddBoolToObject(obj, "autosave_sync_pending", s_autosave_sync_pending);
    cJSON_AddBoolToObject(obj, "autosave_sync_failed", s_autosave_sync_failed);
    cJSON_AddNumberToObject(obj, "meme_coins", (double)g_game_state.meme_coins);
    cJSON_AddNumberToObject(obj, "status", (double)g_game_state.status);
    cJSON_AddNumberToObject(obj, "click_power", (double)g_game_state.click_power);
    cJSON_AddBoolToObject(obj, "first_upgrade_owned", first_upgrade_owned());
    cJSON_AddBoolToObject(obj, "second_upgrade_owned", second_upgrade_owned());
    cJSON_AddBoolToObject(obj, "third_upgrade_owned", third_upgrade_owned());
    cJSON_AddBoolToObject(obj, "fourth_upgrade_owned", fourth_upgrade_owned());
    cJSON_AddBoolToObject(obj, "fifth_upgrade_owned", fifth_upgrade_owned());
    cJSON_AddBoolToObject(obj, "first_job_active", job_active());
    cJSON_AddBoolToObject(obj, "first_job_ready", job_ready());
    cJSON_AddNumberToObject(obj, "first_job_elapsed_ms", (double)g_game_state.active_job_elapsed_ms);
    cJSON_AddNumberToObject(obj, "art_pack_state", (double)nt_resource_pack_state(s_art_pack_id));
    cJSON_AddBoolToObject(obj, "runtime_art_ready", runtime_art_ready());
    *result = obj;
    return true;
}

static bool ep_reset_playtest(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)params;
    (void)user;
    bool ok = game_state_action_reset_playtest(&g_game_state, error, error_cap);
    *result = cJSON_CreateBool(ok);
    return ok;
}

#ifndef NT_PLATFORM_WEB
static bool ep_capture_framebuffer(const cJSON *params, cJSON **result, char *error, int error_cap, void *user) {
    (void)user;
    const cJSON *output = cJSON_GetObjectItemCaseSensitive(params, "output");
    if (!cJSON_IsString(output) || !output->valuestring || output->valuestring[0] == '\0') {
        (void)snprintf(error, (size_t)error_cap, "%s", "output string required");
        return false;
    }
    if (strlen(output->valuestring) >= sizeof(s_frame_capture_path)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "output path too long");
        return false;
    }
    (void)snprintf(s_frame_capture_path, sizeof(s_frame_capture_path), "%s", output->valuestring);
    s_frame_capture_pending = true;

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "output", s_frame_capture_path);
    cJSON_AddStringToObject(obj, "format", "ppm");
    cJSON_AddBoolToObject(obj, "pending", true);
    *result = obj;
    return true;
}
#endif

static bool on_game_state_changed(const char *path, void *user, char *error, int error_cap) {
    (void)path;
    (void)user;
    (void)error;
    (void)error_cap;
    return true;
}
#endif

static void setup_layout(void) {
    const float w = (float)g_nt_window.fb_width;
    const float h = (float)g_nt_window.fb_height;
    const float margin = fmaxf(18.0F, w * 0.048F);
    const float top_safe = fmaxf(UI_TOP_SAFE_MIN, h * 0.016F);
    const float bottom_safe = ui_bottom_safe();
    const float tab_h = fminf(60.0F, fmaxf(54.0F, h * 0.080F));
    const float card_h = fminf(74.0F, fmaxf(66.0F, h * 0.088F));
    const float gap = fmaxf(10.0F, h * 0.012F);
    const float button_size = fminf(w * 0.53F, h * 0.205F);
    const float card_w = w - (margin * 2.0F);
    const float card_y = h - bottom_safe - tab_h - margin - (card_h * 2.0F) - gap;
    const float top_pill_w = fminf(150.0F, w * 0.385F);
    s_coin_pill_rect = (UiRect){margin, top_safe, top_pill_w, 48.0F};
    s_status_pill_rect = (UiRect){w - margin - top_pill_w, top_safe, top_pill_w, 48.0F};
    s_upgrade_rect = (UiRect){margin, card_y, card_w, card_h};
    s_job_rect = (UiRect){margin, card_y + card_h + gap, card_w, card_h};
    float button_y = h * 0.39F;
    float max_button_y = s_upgrade_rect.y - gap - button_size;
    if (button_y > max_button_y) {
        button_y = max_button_y;
    }
    if (button_y < h * 0.35F) {
        button_y = h * 0.35F;
    }
    s_do67_rect = (UiRect){(w - button_size) * 0.5F, button_y, button_size, button_size};
    s_goal_rect = (UiRect){margin, s_do67_rect.y + s_do67_rect.h + gap, card_w, fminf(58.0F, h * 0.078F)};
    s_claim_rect = s_job_rect;
    s_reset_rect = (UiRect){(w * 0.5F) - SETTINGS_BUTTON_W - 3.0F, settings_button_y(), SETTINGS_BUTTON_W, SETTINGS_BUTTON_H};
}

static void init_runtime_art(void) {
    s_art_pack_id = nt_hash32_str("game_67_idle");
    (void)nt_resource_mount(s_art_pack_id, 100);
#ifdef NT_CDN_URL
    nt_resource_load_auto(s_art_pack_id, NT_CDN_URL "/game_67_idle/game_67_idle.ntpack");
#elif defined(NT_PLATFORM_WEB)
    nt_resource_load_auto(s_art_pack_id, "assets/game_67_idle.ntpack");
#else
    nt_resource_load_auto(s_art_pack_id, "build/game_67_idle/game_67_idle.ntpack");
#endif

    s_sprite_vs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_VERT, NT_ASSET_SHADER_CODE);
    s_sprite_fs = nt_resource_request(ASSET_SHADER_ASSETS_SHADERS_SPRITE_FRAG, NT_ASSET_SHADER_CODE);
    s_art_atlas = nt_resource_request(ASSET_ATLAS_GAME_67_IDLE_RUNTIME_ATLAS, NT_ASSET_ATLAS);
    s_art_atlas_texture = nt_resource_request(ASSET_TEXTURE_GAME_67_IDLE_RUNTIME_ATLAS_TEX0, NT_ASSET_TEXTURE);
    s_sprite_material = nt_material_create(&(nt_material_create_desc_t){
        .vs = s_sprite_vs,
        .fs = s_sprite_fs,
        .textures = {{.name = "u_texture", .resource = s_art_atlas_texture}},
        .texture_count = 1,
        .blend_mode = NT_BLEND_MODE_ALPHA,
        .depth_test = false,
        .depth_write = false,
        .cull_mode = NT_CULL_NONE,
        .label = "game_67_idle_sprite",
    });
    nt_resource_set_activate_time_budget(0.0F);
}

static void resolve_runtime_art(void) {
    if (s_art_resolved || !nt_resource_is_ready(s_art_atlas)) {
        return;
    }
    s_bg_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_BG_STARTER_ROOM_YARD.value);
    s_hero_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_HERO_1_67_BODY.value);
    s_badge_power_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_BADGE_POWER_1_67.value);
    s_button_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_BUTTON_67_GESTURE.value);
    s_card_job_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_CARD_JOB_KIOSK.value);
    s_card_upgrade_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_CARD_UPGRADE_TAP.value);
    s_icon_coin_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_ICON_MEME_COIN_67.value);
    s_icon_next_goal_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_ICON_NEXT_GOAL_ARROW_67.value);
    s_icon_tap_hand_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_ICON_TAP_HAND_67.value);
    s_pill_coin_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_PILL_COIN_PLUS.value);
    s_pill_tap_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_PILL_TAP_PLUS_1.value);
    s_tab_city_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_TAB_CITY.value);
    s_tab_home_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_TAB_HOME.value);
    s_first_action_plate_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_UI_FIRST_ACTION_PLATE_9S.value);
    s_first_status_badge_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_UI_FIRST_STATUS_BADGE_SHELL_9S.value);
    s_runtime_goal_card_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_UI_RUNTIME_GOAL_CARD_9S.value);
    s_runtime_progress_bar_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_UI_RUNTIME_PROGRESS_BAR_9S.value);
    s_runtime_resource_pill_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_UI_RUNTIME_RESOURCE_PILL_9S.value);
    s_runtime_tab_locked_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_UI_RUNTIME_TAB_LOCKED_9S.value);
    s_runtime_tab_selected_region = nt_atlas_find_region(s_art_atlas, ASSET_ATLAS_REGION_GAME_67_IDLE_RUNTIME_ATLAS_UI_RUNTIME_TAB_SELECTED_9S.value);
    s_art_resolved = s_bg_region != NT_ATLAS_INVALID_REGION && s_hero_region != NT_ATLAS_INVALID_REGION && s_button_region != NT_ATLAS_INVALID_REGION;
}

static bool runtime_art_ready(void) {
    const nt_material_info_t *info = nt_material_get_info(s_sprite_material);
    return s_art_resolved && info && info->ready && nt_resource_is_ready(s_art_atlas);
}

static void begin_runtime_art(void) {
    if (!runtime_art_ready()) {
        return;
    }
    float w = (float)g_nt_window.fb_width;
    float h = (float)g_nt_window.fb_height;
    mat4 view_m;
    mat4 proj_m;
    mat4 vp;
    glm_mat4_identity(view_m);
    glm_ortho(0.0F, w, 0.0F, h, -1.0F, 1.0F, proj_m);
    glm_mat4_mul(proj_m, view_m, vp);

    nt_frame_uniforms_t uniforms = {0};
    memcpy(uniforms.view_proj, vp, 64);
    memcpy(uniforms.view, view_m, 64);
    memcpy(uniforms.proj, proj_m, 64);
    uniforms.time[0] = (float)g_nt_app.time;
    uniforms.time[1] = g_nt_app.dt;
    uniforms.resolution[0] = w;
    uniforms.resolution[1] = h;
    uniforms.resolution[2] = w > 0.0F ? 1.0F / w : 0.0F;
    uniforms.resolution[3] = h > 0.0F ? 1.0F / h : 0.0F;
    uniforms.near_far[0] = -1.0F;
    uniforms.near_far[1] = 1.0F;

    nt_gfx_update_buffer(s_sprite_frame_ubo, &uniforms, sizeof(uniforms));
    nt_gfx_bind_uniform_buffer(s_sprite_frame_ubo, 0);
    nt_sprite_renderer_set_material(s_sprite_material);
}

static void draw_sprite_region_rect(uint32_t region_index, UiRect r) {
    if (!runtime_art_ready() || region_index == NT_ATLAS_INVALID_REGION || r.w <= 0.0F || r.h <= 0.0F) {
        return;
    }
    const nt_texture_region_t *region = nt_atlas_get_region(s_art_atlas, region_index);
    if (!region || region->source_w == 0 || region->source_h == 0) {
        return;
    }
    float scale_x = r.w / (float)region->source_w;
    float scale_y = r.h / (float)region->source_h;
    float center_x = r.x + (r.w * 0.5F);
    float center_y = (float)g_nt_window.fb_height - r.y - (r.h * 0.5F);
    mat4 model;
    glm_mat4_identity(model);
    glm_translate(model, (vec3){center_x, center_y, 0.0F});
    glm_scale(model, (vec3){scale_x, scale_y, 1.0F});
    nt_sprite_renderer_set_material(s_sprite_material);
    nt_sprite_renderer_emit_region(s_art_atlas, region_index, (const float *)model, 0.5F, 0.5F, 0xFFFFFFFFU, 0U);
}

static void draw_sprite_region_cover(uint32_t region_index, UiRect target) {
    if (!runtime_art_ready() || region_index == NT_ATLAS_INVALID_REGION) {
        return;
    }
    const nt_texture_region_t *region = nt_atlas_get_region(s_art_atlas, region_index);
    if (!region || region->source_w == 0 || region->source_h == 0) {
        return;
    }
    float sx = target.w / (float)region->source_w;
    float sy = target.h / (float)region->source_h;
    float s = fmaxf(sx, sy);
    float draw_w = (float)region->source_w * s;
    float draw_h = (float)region->source_h * s;
    draw_sprite_region_rect(region_index, (UiRect){target.x + ((target.w - draw_w) * 0.5F), target.y + ((target.h - draw_h) * 0.5F), draw_w, draw_h});
}

static void draw_sprite_region_contain_center(uint32_t region_index, UiRect target, float scale) {
    if (!runtime_art_ready() || region_index == NT_ATLAS_INVALID_REGION) {
        return;
    }
    const nt_texture_region_t *region = nt_atlas_get_region(s_art_atlas, region_index);
    if (!region || region->source_w == 0 || region->source_h == 0) {
        return;
    }
    float sx = target.w / (float)region->source_w;
    float sy = target.h / (float)region->source_h;
    float s = fminf(sx, sy) * scale;
    float draw_w = (float)region->source_w * s;
    float draw_h = (float)region->source_h * s;
    draw_sprite_region_rect(region_index, (UiRect){target.x + ((target.w - draw_w) * 0.5F), target.y + ((target.h - draw_h) * 0.5F), draw_w, draw_h});
}

static void register_game_ui(void) {
    setup_layout();
    nt_devapi_register_ui_element("scene.viewport", "Game 67 viewport", 0.0F, 0.0F, (float)g_nt_window.fb_width, (float)g_nt_window.fb_height);
    nt_devapi_register_ui_node("main.root", "", "screen", "Main", "", 0.0F, 0.0F, (float)g_nt_window.fb_width, (float)g_nt_window.fb_height, true, true);
    nt_devapi_register_ui_node("main.do67", "main.root", "button", "Do 67", "+meme coins", s_do67_rect.x, s_do67_rect.y, s_do67_rect.w, s_do67_rect.h, true, true);
    char text[64];
    (void)snprintf(text, sizeof(text), "%d coins", g_game_state.meme_coins);
    nt_devapi_register_ui_node("main.coins", "main.root", "label", "Meme coins", text, s_coin_pill_rect.x, s_coin_pill_rect.y, s_coin_pill_rect.w, s_coin_pill_rect.h, true, false);
    (void)snprintf(text, sizeof(text), "%d/67", status_value());
    nt_devapi_register_ui_node("main.status", "main.root", "label", "Status", text, s_status_pill_rect.x, s_status_pill_rect.y, s_status_pill_rect.w, s_status_pill_rect.h, true, false);
    if (!first_upgrade_owned()) {
        (void)snprintf(text, sizeof(text), "%s", g_game_state.meme_coins >= FIRST_UPGRADE_COST ? "ready buy" : "need 5 coins");
    } else if (!second_upgrade_owned()) {
        (void)snprintf(text, sizeof(text), "%s", g_game_state.meme_coins >= SECOND_UPGRADE_COST ? "ready cap" : "need 8 coins");
    } else if (!third_upgrade_owned()) {
        (void)snprintf(text, sizeof(text), "%s", g_game_state.meme_coins >= THIRD_UPGRADE_COST ? "ready pic" : "need 20 coins");
    } else if (!fourth_upgrade_owned() && status_value() >= 7) {
        (void)snprintf(text, sizeof(text), "%s", g_game_state.meme_coins >= FOURTH_UPGRADE_COST ? "ready bike" : "need 35 coins");
    } else if (!fifth_upgrade_owned() && fourth_upgrade_owned()) {
        (void)snprintf(text, sizeof(text), "%s", g_game_state.meme_coins >= FIFTH_UPGRADE_COST ? "ready stnd" : "need 60 coins");
    } else if (fifth_upgrade_owned()) {
        (void)snprintf(text, sizeof(text), "%s", "owned x10");
    } else {
        (void)snprintf(text, sizeof(text), "%s", "owned x5");
    }
    bool upgrade_enabled = !third_upgrade_owned() || (!fourth_upgrade_owned() && status_value() >= 7) || (!fifth_upgrade_owned() && fourth_upgrade_owned());
    nt_devapi_register_ui_node("main.upgrade.first", "main.root", "button", "Upgrade", text, s_upgrade_rect.x, s_upgrade_rect.y, s_upgrade_rect.w, s_upgrade_rect.h, true, upgrade_enabled);
    bool active = job_active();
    bool ready = job_ready();
    const bool third_job = third_job_unlocked() || third_job_active();
    const bool second_job = !third_job && (second_job_unlocked() || second_job_active());
    nt_devapi_register_ui_node(
        ready ? "main.claim" : "main.job.first",
        "main.root",
        "button",
        ready ? "Claim job" : (third_job ? "Start shop job" : (second_job ? "Start sticker job" : "Start first job")),
        active ? "timer" : (third_job ? "reward 90" : (second_job ? "reward 30" : "reward 8")),
        s_job_rect.x,
        s_job_rect.y,
        s_job_rect.w,
        s_job_rect.h,
        true,
        first_upgrade_owned() || ready);
    nt_devapi_register_ui_node("main.reset", "main.root", "button", "Reset", "reset", s_reset_rect.x, s_reset_rect.y, s_reset_rect.w, s_reset_rect.h, true, true);
    nt_devapi_register_ui_node("settings.open", "main.root", "button", "Settings", "audio", settings_button_x(), settings_button_y(), SETTINGS_BUTTON_W, SETTINGS_BUTTON_H, true, true);
    if (s_settings_open) {
        float modal_x = ((float)g_nt_window.fb_width - SETTINGS_MODAL_W) * 0.5F;
        float modal_y = ((float)g_nt_window.fb_height - SETTINGS_MODAL_H) * 0.5F;
        float slider_x = modal_x + 72.0F;
        nt_devapi_register_ui_node("settings.modal", "main.root", "dialog", "Settings", "", modal_x, modal_y, SETTINGS_MODAL_W, SETTINGS_MODAL_H, true, true);
        nt_devapi_register_ui_node("settings.close", "settings.modal", "button", "Close", "x", modal_x + SETTINGS_MODAL_W - SETTINGS_CLOSE_W - 18.0F, modal_y + 18.0F, SETTINGS_CLOSE_W, SETTINGS_CLOSE_H, true, true);
        nt_devapi_register_ui_node("settings.master_volume", "settings.modal", "slider", "Master volume", "master", slider_x, modal_y + 86.0F, SETTINGS_SLIDER_W, SETTINGS_SLIDER_H, true, true);
        nt_devapi_register_ui_node("settings.sfx_volume", "settings.modal", "slider", "SFX volume", "sfx", slider_x, modal_y + 154.0F, SETTINGS_SLIDER_W, SETTINGS_SLIDER_H, true, true);
    }
}

static void draw_screen_rect(float x, float y, float w, float h, const float color[4]) {
    float px = x + (w * 0.5F);
    float py = (float)g_nt_window.fb_height - y - (h * 0.5F);
    float pos[3] = {px, py, 0.0F};
    float size[2] = {w, h};
    nt_shape_renderer_rect(pos, size, color);
}

static void draw_screen_rect_wire(float x, float y, float w, float h, const float color[4]) {
    float px = x + (w * 0.5F);
    float py = (float)g_nt_window.fb_height - y - (h * 0.5F);
    float pos[3] = {px, py, 0.0F};
    float size[2] = {w, h};
    nt_shape_renderer_rect_wire(pos, size, color);
}

static void draw_screen_circle(float x, float y, float radius, const float color[4]) {
    float pos[3] = {x, (float)g_nt_window.fb_height - y, 0.0F};
    nt_shape_renderer_circle(pos, radius, color);
}

static void draw_segment(float x, float y, float w, float h, const float color[4]) { draw_screen_rect(x, y, w, h, color); }

static void draw_digit(int digit, float x, float y, float s, const float color[4]) {
    static const uint8_t mask[10] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
    };
    uint8_t m = mask[digit % 10];
    float t = s * 0.16F;
    float w = s * 0.62F;
    float h = s;
    if (m & 0x01) draw_segment(x + t, y, w, t, color);
    if (m & 0x02) draw_segment(x + w + t, y + t, t, (h * 0.5F) - t, color);
    if (m & 0x04) draw_segment(x + w + t, y + (h * 0.5F), t, (h * 0.5F) - t, color);
    if (m & 0x08) draw_segment(x + t, y + h - t, w, t, color);
    if (m & 0x10) draw_segment(x, y + (h * 0.5F), t, (h * 0.5F) - t, color);
    if (m & 0x20) draw_segment(x, y + t, t, (h * 0.5F) - t, color);
    if (m & 0x40) draw_segment(x + t, y + (h * 0.5F) - (t * 0.5F), w, t, color);
}

static float draw_number(int value, float x, float y, float s, const float color[4]) {
    char buf[16];
    (void)snprintf(buf, sizeof(buf), "%d", value);
    float cursor = x;
    for (int i = 0; buf[i] != '\0'; i++) {
        if (buf[i] >= '0' && buf[i] <= '9') {
            draw_digit(buf[i] - '0', cursor, y, s, color);
            cursor += s * 0.86F;
        }
    }
    return cursor - x;
}

static const uint8_t *glyph_rows(char ch) {
    static const uint8_t space[7] = {0, 0, 0, 0, 0, 0, 0};
    static const uint8_t plus[7] = {0, 4, 4, 31, 4, 4, 0};
    static const uint8_t dash[7] = {0, 0, 0, 31, 0, 0, 0};
    static const uint8_t slash[7] = {1, 2, 2, 4, 8, 8, 16};
    static const uint8_t d0[7] = {14, 17, 19, 21, 25, 17, 14};
    static const uint8_t d1[7] = {4, 12, 4, 4, 4, 4, 14};
    static const uint8_t d2[7] = {14, 17, 1, 2, 4, 8, 31};
    static const uint8_t d3[7] = {30, 1, 1, 14, 1, 1, 30};
    static const uint8_t d4[7] = {2, 6, 10, 18, 31, 2, 2};
    static const uint8_t d5[7] = {31, 16, 16, 30, 1, 1, 30};
    static const uint8_t d6[7] = {14, 16, 16, 30, 17, 17, 14};
    static const uint8_t d7[7] = {31, 1, 2, 4, 8, 8, 8};
    static const uint8_t d8[7] = {14, 17, 17, 14, 17, 17, 14};
    static const uint8_t d9[7] = {14, 17, 17, 15, 1, 1, 14};
    static const uint8_t a[7] = {14, 17, 17, 31, 17, 17, 17};
    static const uint8_t b[7] = {30, 17, 17, 30, 17, 17, 30};
    static const uint8_t c[7] = {14, 17, 16, 16, 16, 17, 14};
    static const uint8_t d[7] = {30, 17, 17, 17, 17, 17, 30};
    static const uint8_t e[7] = {31, 16, 16, 30, 16, 16, 31};
    static const uint8_t f[7] = {31, 16, 16, 30, 16, 16, 16};
    static const uint8_t g[7] = {14, 17, 16, 23, 17, 17, 14};
    static const uint8_t h_[7] = {17, 17, 17, 31, 17, 17, 17};
    static const uint8_t i[7] = {14, 4, 4, 4, 4, 4, 14};
    static const uint8_t j[7] = {1, 1, 1, 1, 17, 17, 14};
    static const uint8_t k[7] = {17, 18, 20, 24, 20, 18, 17};
    static const uint8_t l[7] = {16, 16, 16, 16, 16, 16, 31};
    static const uint8_t m[7] = {17, 27, 21, 21, 17, 17, 17};
    static const uint8_t n[7] = {17, 25, 21, 19, 17, 17, 17};
    static const uint8_t o[7] = {14, 17, 17, 17, 17, 17, 14};
    static const uint8_t p[7] = {30, 17, 17, 30, 16, 16, 16};
    static const uint8_t r[7] = {30, 17, 17, 30, 20, 18, 17};
    static const uint8_t s[7] = {15, 16, 16, 14, 1, 1, 30};
    static const uint8_t t[7] = {31, 4, 4, 4, 4, 4, 4};
    static const uint8_t u[7] = {17, 17, 17, 17, 17, 17, 14};
    static const uint8_t v[7] = {17, 17, 17, 17, 10, 10, 4};
    static const uint8_t w[7] = {17, 17, 17, 21, 21, 27, 17};
    static const uint8_t x[7] = {17, 17, 10, 4, 10, 17, 17};
    static const uint8_t y[7] = {17, 17, 10, 4, 4, 4, 4};
    switch (ch) {
    case ' ': return space;
    case '+': return plus;
    case '-': return dash;
    case '/': return slash;
    case '0': return d0;
    case '1': return d1;
    case '2': return d2;
    case '3': return d3;
    case '4': return d4;
    case '5': return d5;
    case '6': return d6;
    case '7': return d7;
    case '8': return d8;
    case '9': return d9;
    case 'A': return a;
    case 'B': return b;
    case 'C': return c;
    case 'D': return d;
    case 'E': return e;
    case 'F': return f;
    case 'G': return g;
    case 'H': return h_;
    case 'I': return i;
    case 'J': return j;
    case 'K': return k;
    case 'L': return l;
    case 'M': return m;
    case 'N': return n;
    case 'O': return o;
    case 'P': return p;
    case 'R': return r;
    case 'S': return s;
    case 'T': return t;
    case 'U': return u;
    case 'V': return v;
    case 'W': return w;
    case 'X': return x;
    case 'Y': return y;
    default: return space;
    }
}

static float draw_bitmap_text(const char *text, float x, float y, float scale, const float color[4]) {
    float cursor = x;
    for (int i = 0; text[i] != '\0'; i++) {
        const uint8_t *rows = glyph_rows(text[i]);
        for (int row = 0; row < 7; row++) {
            for (int col = 0; col < 5; col++) {
                if (rows[row] & (uint8_t)(1U << (4 - col))) {
                    draw_screen_rect(cursor + ((float)col * scale), y + ((float)row * scale), scale, scale, color);
                }
            }
        }
        cursor += (text[i] == ' ') ? scale * 4.0F : scale * 6.0F;
    }
    return cursor - x;
}

static void draw_progress_toy(float x, float y, float w, float h, float value, bool art_ready) {
    (void)art_ready;
    const float bg[4] = {0.17F, 0.20F, 0.24F, 1.0F};
    const float fill[4] = {0.22F, 0.86F, 0.40F, 1.0F};
    const float shine[4] = {1.0F, 0.96F, 0.64F, 0.70F};
    draw_screen_rect(x, y, w, h, bg);
    draw_screen_rect(x + 7.0F, y + 7.0F, (w - 14.0F) * clamp01(value), h - 14.0F, fill);
    draw_screen_rect(x + 10.0F, y + 9.0F, (w - 20.0F) * clamp01(value), fmaxf(3.0F, h * 0.16F), shine);
}

static void draw_card(UiRect r, const float fill[4], bool enabled) {
    const float shade[4] = {0.0F, 0.0F, 0.0F, 0.18F};
    const float edge[4] = {0.12F, 0.10F, 0.16F, 1.0F};
    const float disabled[4] = {0.45F, 0.48F, 0.52F, 1.0F};
    draw_screen_rect(r.x + 4.0F, r.y + 5.0F, r.w, r.h, shade);
    draw_screen_rect(r.x, r.y, r.w, r.h, enabled ? fill : disabled);
    draw_screen_rect_wire(r.x, r.y, r.w, r.h, edge);
}

static void draw_reward_badge(UiRect badge, const char *text, const float text_color[4], bool hot) {
    const float shade[4] = {0.0F, 0.0F, 0.0F, 0.18F};
    const float edge[4] = {0.11F, 0.10F, 0.14F, 1.0F};
    const float yellow[4] = {1.0F, 0.90F, 0.25F, 1.0F};
    const float green[4] = {0.22F, 0.86F, 0.40F, 1.0F};
    const float shine[4] = {1.0F, 1.0F, 0.74F, 0.70F};
    const float *fill = hot ? green : yellow;
    float radius = badge.h * 0.5F;
    draw_screen_rect(badge.x + 3.0F, badge.y + 4.0F, badge.w, badge.h, shade);
    draw_screen_rect(badge.x + radius, badge.y, badge.w - (radius * 2.0F), badge.h, fill);
    draw_screen_circle(badge.x + radius, badge.y + radius, radius, fill);
    draw_screen_circle(badge.x + badge.w - radius, badge.y + radius, radius, fill);
    draw_screen_rect(badge.x + radius, badge.y, badge.w - (radius * 2.0F), badge.h, fill);
    draw_screen_rect(badge.x + 13.0F, badge.y + 6.0F, badge.w - 26.0F, 5.0F, shine);
    draw_screen_rect_wire(badge.x + 4.0F, badge.y + 4.0F, badge.w - 8.0F, badge.h - 8.0F, edge);
    float scale = 2.2F;
    float text_w = bitmap_text_width(text, scale);
    draw_bitmap_text(text, badge.x + ((badge.w - text_w) * 0.5F), badge.y + 8.5F, scale, text_color);
}

static void draw_home_icon(float cx, float cy, float size, const float color[4]) {
    draw_screen_rect(cx - (size * 0.33F), cy, size * 0.66F, size * 0.42F, color);
    draw_screen_rect(cx - (size * 0.46F), cy - (size * 0.13F), size * 0.92F, size * 0.16F, color);
    draw_screen_rect(cx - (size * 0.11F), cy + (size * 0.18F), size * 0.22F, size * 0.24F, color);
}

static void draw_city_icon(float cx, float cy, float size, const float color[4]) {
    draw_screen_rect(cx - (size * 0.42F), cy - (size * 0.08F), size * 0.22F, size * 0.50F, color);
    draw_screen_rect(cx - (size * 0.10F), cy - (size * 0.28F), size * 0.24F, size * 0.70F, color);
    draw_screen_rect(cx + (size * 0.24F), cy + (size * 0.02F), size * 0.20F, size * 0.40F, color);
}

static void draw_audio_icon(float cx, float cy, float size, const float color[4]) {
    const float s = size * 0.5F;
    draw_screen_rect(cx - s * 0.76F, cy - s * 0.26F, s * 0.28F, s * 0.52F, color);
    draw_screen_rect(cx - s * 0.48F, cy - s * 0.44F, s * 0.30F, s * 0.88F, color);
    draw_screen_rect(cx - s * 0.18F, cy - s * 0.58F, s * 0.18F, s * 1.16F, color);
    draw_screen_rect(cx + s * 0.24F, cy - s * 0.44F, s * 0.12F, s * 0.88F, color);
    draw_screen_rect(cx + s * 0.50F, cy - s * 0.62F, s * 0.12F, s * 1.24F, color);
}

static void draw_reset_icon(float cx, float cy, float size, const float color[4]) {
    const float s = size * 0.5F;
    draw_screen_rect(cx - s * 0.60F, cy - s * 0.54F, s * 1.08F, s * 0.14F, color);
    draw_screen_rect(cx - s * 0.60F, cy - s * 0.54F, s * 0.14F, s * 0.54F, color);
    draw_screen_rect(cx + s * 0.34F, cy - s * 0.54F, s * 0.14F, s * 0.38F, color);
    draw_screen_rect(cx + s * 0.20F, cy - s * 0.28F, s * 0.42F, s * 0.14F, color);
    draw_screen_rect(cx + s * 0.48F, cy - s * 0.28F, s * 0.14F, s * 0.42F, color);
    draw_screen_rect(cx + s * 0.12F, cy + s * 0.28F, s * 0.36F, s * 0.14F, color);
}

static void draw_game_screen(void) {
    mat4 ortho;
    float cam_pos[3] = {0.0F, 0.0F, 1.0F};
    glm_ortho(0.0F, (float)g_nt_window.fb_width, 0.0F, (float)g_nt_window.fb_height, -1.0F, 1.0F, ortho);
    nt_shape_renderer_set_vp((float *)ortho);
    nt_shape_renderer_set_cam_pos(cam_pos);
    nt_shape_renderer_set_depth(false);

    const float sky[4] = {0.34F, 0.78F, 1.0F, 1.0F};
    const float grass[4] = {0.24F, 0.82F, 0.32F, 1.0F};
    const float hill[4] = {0.18F, 0.68F, 0.28F, 1.0F};
    const float white[4] = {1.0F, 1.0F, 0.94F, 1.0F};
    const float ink[4] = {0.08F, 0.08F, 0.12F, 1.0F};
    const float coin[4] = {1.0F, 0.80F, 0.12F, 1.0F};
    const float orange[4] = {1.0F, 0.42F, 0.12F, 1.0F};
    const float pink[4] = {1.0F, 0.34F, 0.58F, 1.0F};
    const float purple[4] = {0.42F, 0.24F, 0.86F, 1.0F};
    const float blue[4] = {0.12F, 0.48F, 0.96F, 1.0F};
    const float green[4] = {0.08F, 0.78F, 0.42F, 1.0F};
    const float yellow[4] = {1.0F, 0.92F, 0.18F, 1.0F};
    const float locked[4] = {0.50F, 0.54F, 0.58F, 0.78F};
    const float bottom_shadow[4] = {0.05F, 0.06F, 0.07F, 1.0F};

    float w = (float)g_nt_window.fb_width;
    float h = (float)g_nt_window.fb_height;
    setup_layout();
    bool art_ready = runtime_art_ready();
    const float bottom_safe = ui_bottom_safe();
    const float tab_h = fminf(60.0F, fmaxf(54.0F, h * 0.080F));
    const UiRect coin_pill = s_coin_pill_rect;
    const UiRect status_badge = s_status_pill_rect;
    const UiRect goal_rail = s_goal_rect;
    if (art_ready) {
        begin_runtime_art();
        draw_sprite_region_cover(s_bg_region, (UiRect){0.0F, 0.0F, w, h});
        float hero_h = fminf(h * 0.23F, 196.0F);
        float hero_w = hero_h * (193.0F / 361.0F);
        float hero_y = fminf(h * 0.17F, s_do67_rect.y - (hero_h * 0.76F));
        draw_sprite_region_rect(s_hero_region, (UiRect){(w - hero_w) * 0.5F, hero_y, hero_w, hero_h});
        draw_sprite_region_rect(s_runtime_resource_pill_region, coin_pill);
        draw_sprite_region_rect(s_runtime_resource_pill_region, status_badge);
        draw_sprite_region_rect(s_first_action_plate_region, goal_rail);
        draw_sprite_region_rect(s_runtime_goal_card_region, s_upgrade_rect);
        draw_sprite_region_rect(s_runtime_goal_card_region, s_job_rect);
        draw_sprite_region_rect(s_runtime_tab_selected_region, (UiRect){0.0F, h - bottom_safe - tab_h, w * 0.5F, tab_h});
        draw_sprite_region_rect(s_runtime_tab_locked_region, (UiRect){w * 0.5F, h - bottom_safe - tab_h, w * 0.5F, tab_h});
        draw_sprite_region_contain_center(s_button_region, s_do67_rect, 1.05F);
        draw_sprite_region_contain_center(s_icon_tap_hand_region, (UiRect){s_do67_rect.x + (s_do67_rect.w * 0.56F), s_do67_rect.y + (s_do67_rect.h * 0.46F), s_do67_rect.w * 0.34F, s_do67_rect.h * 0.34F}, 1.0F);
        draw_sprite_region_contain_center(s_icon_coin_region, (UiRect){coin_pill.x + 8.0F, coin_pill.y + 9.0F, 38.0F, 38.0F}, 1.0F);
        draw_sprite_region_contain_center(s_icon_coin_region, (UiRect){goal_rail.x + 12.0F, goal_rail.y + 10.0F, 38.0F, 38.0F}, 1.0F);
        nt_sprite_renderer_flush();
    } else {
        draw_screen_rect(0.0F, 0.0F, w, h, sky);
        draw_screen_circle(w * 0.18F, h * 0.18F, 44.0F, yellow);
        draw_screen_rect(0.0F, h * 0.53F, w, h * 0.47F, grass);
        draw_screen_circle(w * 0.10F, h * 0.58F, w * 0.55F, hill);
        draw_screen_circle(w * 0.84F, h * 0.55F, w * 0.60F, hill);
    }

    if (!art_ready || s_runtime_resource_pill_region == NT_ATLAS_INVALID_REGION) {
        draw_card(coin_pill, coin, true);
        draw_screen_circle(coin_pill.x + 28.0F, coin_pill.y + 28.0F, 13.0F, yellow);
    }
    draw_number(g_game_state.meme_coins, coin_pill.x + 58.0F, coin_pill.y + 14.0F, 20.0F, ink);

    if (!art_ready || s_runtime_resource_pill_region == NT_ATLAS_INVALID_REGION) {
        draw_card(status_badge, purple, true);
    }
    char status_text[16];
    (void)snprintf(status_text, sizeof(status_text), "%d/67", status_value());
    draw_bitmap_text(status_text, status_badge.x + 48.0F, status_badge.y + 15.0F, 3.25F, ink);

    if (!art_ready || s_first_action_plate_region == NT_ATLAS_INVALID_REGION) {
        draw_card(goal_rail, yellow, true);
    }
    int goal_current = g_game_state.meme_coins;
    int goal_target = FIRST_UPGRADE_COST;
    if (first_upgrade_owned() && !second_upgrade_owned()) {
        goal_target = SECOND_UPGRADE_COST;
    } else if (second_upgrade_owned() && !third_upgrade_owned()) {
        goal_target = THIRD_UPGRADE_COST;
    } else if (third_upgrade_owned() && !fourth_upgrade_owned() && status_value() >= 7) {
        goal_target = FOURTH_UPGRADE_COST;
    } else if (fourth_upgrade_owned() && !fifth_upgrade_owned()) {
        goal_target = FIFTH_UPGRADE_COST;
    } else if (third_upgrade_owned()) {
        goal_current = status_value();
        goal_target = 67;
    }
    float goal_value = (float)goal_current / (float)goal_target;
    draw_number(goal_current < goal_target ? goal_current : goal_target, goal_rail.x + 58.0F, goal_rail.y + 17.0F, 22.0F, ink);
    draw_progress_toy(goal_rail.x + 118.0F, goal_rail.y + 22.0F, goal_rail.w - 186.0F, 16.0F, goal_value, art_ready);
    draw_number(goal_target, goal_rail.x + goal_rail.w - 58.0F, goal_rail.y + 17.0F, 22.0F, ink);

    float hero_x = w * 0.50F;
    float hero_y = h * 0.30F;
    float bounce = sinf((float)g_nt_app.time * 4.0F) * 4.0F;
    if (!art_ready) {
        draw_screen_circle(hero_x, hero_y - 40.0F + bounce, 34.0F, pink);
        draw_screen_rect(hero_x - 34.0F, hero_y - 10.0F + bounce, 68.0F, 86.0F, orange);
        draw_screen_rect(hero_x - 58.0F, hero_y + 10.0F + bounce, 24.0F, 62.0F, purple);
        draw_screen_rect(hero_x + 34.0F, hero_y + 10.0F + bounce, 24.0F, 62.0F, purple);
    } else {
        (void)white;
    }
    if (status_value() >= 2) {
        draw_screen_circle(hero_x + 48.0F, hero_y - 72.0F + bounce, 15.0F, green);
        draw_screen_circle(hero_x + 48.0F, hero_y - 72.0F + bounce, 8.5F, yellow);
    }
    if (status_value() >= 3) {
        draw_screen_circle(hero_x + 70.0F, hero_y + 20.0F, 22.0F, green);
    }
    if (status_value() >= 6) {
        draw_screen_rect(w * 0.68F, h * 0.19F, 66.0F, 48.0F, yellow);
        draw_screen_rect_wire(w * 0.68F, h * 0.19F, 66.0F, 48.0F, purple);
        draw_bitmap_text("67", w * 0.70F, h * 0.205F, 4.2F, purple);
        if (status_value() >= 7) {
            draw_screen_rect(w * 0.705F, h * 0.185F, 14.0F, 14.0F, pink);
            draw_screen_rect(w * 0.770F, h * 0.225F, 12.0F, 12.0F, green);
            draw_screen_rect(w * 0.735F, h * 0.245F, 10.0F, 10.0F, orange);
        }
        if (status_value() >= 9) {
            const float bike_y = h * 0.52F;
            const float bike_x = w * 0.72F;
            draw_screen_circle(bike_x, bike_y, 13.0F, ink);
            draw_screen_circle(bike_x, bike_y, 7.0F, blue);
            draw_screen_circle(bike_x + 48.0F, bike_y, 13.0F, ink);
            draw_screen_circle(bike_x + 48.0F, bike_y, 7.0F, blue);
            draw_screen_rect(bike_x + 3.0F, bike_y - 31.0F, 42.0F, 8.0F, orange);
            draw_screen_rect(bike_x + 19.0F, bike_y - 47.0F, 28.0F, 8.0F, green);
            draw_screen_rect(bike_x + 36.0F, bike_y - 36.0F, 18.0F, 7.0F, pink);
        }
        if (status_value() >= 10) {
            const float stand_x = w * 0.17F;
            const float stand_y = h * 0.53F;
            draw_screen_rect(stand_x - 36.0F, stand_y - 28.0F, 72.0F, 18.0F, orange);
            draw_screen_rect(stand_x - 30.0F, stand_y - 10.0F, 60.0F, 42.0F, yellow);
            draw_screen_rect_wire(stand_x - 30.0F, stand_y - 10.0F, 60.0F, 42.0F, ink);
            draw_screen_rect(stand_x - 24.0F, stand_y - 54.0F, 48.0F, 24.0F, green);
            draw_screen_rect_wire(stand_x - 24.0F, stand_y - 54.0F, 48.0F, 24.0F, ink);
            draw_bitmap_text("67", stand_x - 15.0F, stand_y - 50.0F, 3.0F, white);
            draw_screen_circle(stand_x - 16.0F, stand_y + 13.0F, 7.0F, coin);
            draw_screen_circle(stand_x + 16.0F, stand_y + 13.0F, 7.0F, coin);
            if (status_value() >= 11) {
                draw_screen_rect(stand_x - 38.0F, stand_y - 84.0F, 76.0F, 22.0F, orange);
                draw_screen_rect_wire(stand_x - 38.0F, stand_y - 84.0F, 76.0F, 22.0F, ink);
                draw_bitmap_text("OPEN", stand_x - 26.0F, stand_y - 78.0F, 2.15F, white);
                draw_screen_circle(stand_x + 50.0F, stand_y - 18.0F, 10.0F, pink);
                draw_screen_rect(stand_x + 42.0F, stand_y - 7.0F, 16.0F, 24.0F, blue);
                draw_screen_circle(stand_x - 26.0F, stand_y - 70.0F, 5.0F, coin);
                draw_screen_circle(stand_x + 4.0F, stand_y - 76.0F, 5.0F, coin);
                draw_screen_circle(stand_x + 28.0F, stand_y - 66.0F, 5.0F, coin);
                draw_screen_circle(stand_x + 58.0F, stand_y - 42.0F, 5.5F, coin);
                draw_screen_circle(stand_x + 70.0F, stand_y - 34.0F, 4.0F, yellow);
            }
        }
    }
    if (!art_ready) {
        draw_screen_circle(w * 0.20F, h * 0.43F, 23.0F, yellow);
        draw_screen_rect(w * 0.20F - 10.0F, h * 0.43F + 18.0F, 20.0F, 48.0F, yellow);
    }

    float pulse = 1.0F + ((float)g_game_state.feedback_pulse_ms / 900.0F) * 0.08F;
    float cx = s_do67_rect.x + s_do67_rect.w * 0.5F;
    float cy = s_do67_rect.y + s_do67_rect.h * 0.5F;
    if (!art_ready) {
        draw_screen_circle(cx, cy + 8.0F, s_do67_rect.w * 0.48F * pulse, orange);
        draw_screen_circle(cx, cy + 8.0F, s_do67_rect.w * 0.36F * pulse, yellow);
    } else if (g_game_state.feedback_pulse_ms > 0) {
        const float pulse_ring[4] = {1.0F, 1.0F, 1.0F, 0.45F};
        draw_screen_circle(cx, cy + 8.0F, s_do67_rect.w * 0.48F * pulse, pulse_ring);
    }
    if (g_game_state.feedback_pulse_ms > 0) {
        float t = 1.0F - ((float)g_game_state.feedback_pulse_ms / 900.0F);
        if (t < 0.0F) t = 0.0F;
        if (t > 1.0F) t = 1.0F;
        for (int i = 0; i < 4; i++) {
            float lane = (float)i - 1.5F;
            float fx = cx + ((coin_pill.x + 28.0F - cx) * t) + (sinf((t * 5.0F) + lane) * 14.0F);
            float fy = cy + ((coin_pill.y + 26.0F - cy) * t) + (lane * 8.0F);
            draw_screen_circle(fx, fy, 5.5F, yellow);
            draw_screen_circle(fx, fy, 3.0F, coin);
        }
        const char *feedback_text = "+1";
        if (g_game_state.feedback_code == 2) {
            feedback_text = "BUY";
        } else if (g_game_state.feedback_code == 4) {
            feedback_text = "+8";
        } else if (g_game_state.feedback_code == 6) {
            feedback_text = "+30";
        } else if (g_game_state.feedback_code == 8) {
            feedback_text = "+90";
        }
        const float *feedback_color = (g_game_state.feedback_code == 1) ? yellow : green;
        draw_bitmap_text(feedback_text, cx - 24.0F, cy - (s_do67_rect.h * 0.62F), 4.0F, feedback_color);
    }

    bool can_upgrade = !first_upgrade_owned() && g_game_state.meme_coins >= FIRST_UPGRADE_COST;
    bool can_second_upgrade = first_upgrade_owned() && !second_upgrade_owned() && g_game_state.meme_coins >= SECOND_UPGRADE_COST;
    bool can_third_upgrade = second_upgrade_owned() && !third_upgrade_owned() && g_game_state.meme_coins >= THIRD_UPGRADE_COST;
    bool can_fourth_upgrade = third_upgrade_owned() && !fourth_upgrade_owned() && status_value() >= 7 && g_game_state.meme_coins >= FOURTH_UPGRADE_COST;
    bool can_fifth_upgrade = fourth_upgrade_owned() && !fifth_upgrade_owned() && g_game_state.meme_coins >= FIFTH_UPGRADE_COST;
    CardSlots upgrade_slots = card_slots(s_upgrade_rect);
    if (!art_ready || s_runtime_goal_card_region == NT_ATLAS_INVALID_REGION) {
        draw_card(
            s_upgrade_rect,
            (can_upgrade || can_second_upgrade || can_third_upgrade || can_fourth_upgrade || can_fifth_upgrade) ? green : blue,
            !third_upgrade_owned() || (status_value() >= 7 && !fourth_upgrade_owned()) || (fourth_upgrade_owned() && !fifth_upgrade_owned()));
        draw_screen_circle(s_upgrade_rect.x + 34.0F, s_upgrade_rect.y + 40.0F, 20.0F, yellow);
    }
    if (art_ready && s_icon_tap_hand_region != NT_ATLAS_INVALID_REGION) {
        draw_sprite_region_contain_center(s_icon_tap_hand_region, upgrade_slots.icon, 1.0F);
        if (!third_upgrade_owned() || (status_value() >= 7 && !fourth_upgrade_owned()) || (fourth_upgrade_owned() && !fifth_upgrade_owned())) {
            draw_sprite_region_contain_center(s_icon_coin_region, (UiRect){upgrade_slots.value_x - 36.0F, s_upgrade_rect.y + 18.0F, 34.0F, 34.0F}, 1.0F);
        }
        nt_sprite_renderer_flush();
    }
    const float *upgrade_strip = (fifth_upgrade_owned() || can_upgrade || can_second_upgrade || can_third_upgrade || can_fourth_upgrade || can_fifth_upgrade) ? green : blue;
    draw_screen_rect(s_upgrade_rect.x + 8.0F, s_upgrade_rect.y + 8.0F, 8.0F, s_upgrade_rect.h - 16.0F, upgrade_strip);
    if (fifth_upgrade_owned()) {
        draw_bitmap_text("X10", upgrade_slots.label_x, s_upgrade_rect.y + 18.0F, 4.8F, green);
    } else if (fourth_upgrade_owned()) {
        draw_bitmap_text("STND", upgrade_slots.label_x, upgrade_slots.label_y, 3.0F, ink);
        draw_progress_toy(upgrade_slots.progress_x, upgrade_slots.progress_y, upgrade_slots.progress_w, 12.0F, (float)g_game_state.meme_coins / (float)FIFTH_UPGRADE_COST, art_ready);
        draw_number(60, upgrade_slots.value_x + 8.0F, upgrade_slots.value_y, 25.0F, coin);
    } else if (third_upgrade_owned() && status_value() >= 7) {
        draw_bitmap_text("BIK", upgrade_slots.label_x, upgrade_slots.label_y, 3.5F, ink);
        draw_progress_toy(upgrade_slots.progress_x, upgrade_slots.progress_y, upgrade_slots.progress_w, 12.0F, (float)g_game_state.meme_coins / (float)FOURTH_UPGRADE_COST, art_ready);
        draw_number(35, upgrade_slots.value_x + 8.0F, upgrade_slots.value_y, 25.0F, coin);
    } else if (third_upgrade_owned()) {
        draw_bitmap_text("X5", upgrade_slots.label_x, s_upgrade_rect.y + 18.0F, 5.5F, green);
    } else if (second_upgrade_owned()) {
        draw_bitmap_text("PIC", upgrade_slots.label_x, upgrade_slots.label_y, 3.5F, ink);
        draw_progress_toy(upgrade_slots.progress_x, upgrade_slots.progress_y, upgrade_slots.progress_w, 12.0F, (float)g_game_state.meme_coins / (float)THIRD_UPGRADE_COST, art_ready);
        draw_number(20, upgrade_slots.value_x + 8.0F, upgrade_slots.value_y, 25.0F, coin);
    } else if (first_upgrade_owned()) {
        draw_bitmap_text("CAP", upgrade_slots.label_x, upgrade_slots.label_y, 3.5F, ink);
        draw_progress_toy(upgrade_slots.progress_x, upgrade_slots.progress_y, upgrade_slots.progress_w, 12.0F, (float)g_game_state.meme_coins / (float)SECOND_UPGRADE_COST, art_ready);
        draw_number(8, upgrade_slots.value_x + 18.0F, upgrade_slots.value_y, 25.0F, coin);
    } else {
        draw_bitmap_text("TAP", upgrade_slots.label_x, upgrade_slots.label_y, 3.5F, ink);
        draw_progress_toy(upgrade_slots.progress_x, upgrade_slots.progress_y, upgrade_slots.progress_w, 12.0F, (float)g_game_state.meme_coins / (float)FIRST_UPGRADE_COST, art_ready);
        draw_number(5, upgrade_slots.value_x + 18.0F, upgrade_slots.value_y, 25.0F, coin);
    }

    bool active = job_active();
    bool ready = job_ready();
    bool third_job = third_job_unlocked() || third_job_active();
    bool second_job = !third_job && (second_job_unlocked() || second_job_active());
    CardSlots job_slots = card_slots(s_job_rect);
    if (!art_ready || s_runtime_goal_card_region == NT_ATLAS_INVALID_REGION) {
        draw_card(s_job_rect, ready ? green : pink, first_upgrade_owned());
        draw_screen_circle(s_job_rect.x + 34.0F, s_job_rect.y + 40.0F, 20.0F, coin);
    }
    const float *job_strip = ready ? green : (first_upgrade_owned() ? blue : locked);
    draw_screen_rect(s_job_rect.x + 8.0F, s_job_rect.y + 8.0F, 8.0F, s_job_rect.h - 16.0F, job_strip);
    const bool show_third_job = active ? third_job_active() : third_job;
    UiRect job_reward_badge = {s_job_rect.x + s_job_rect.w - 88.0F, s_job_rect.y + 16.0F, 68.0F, 34.0F};
    if (active) {
        if (art_ready && s_icon_coin_region != NT_ATLAS_INVALID_REGION) {
            draw_sprite_region_contain_center(s_icon_coin_region, job_slots.icon, 1.0F);
            nt_sprite_renderer_flush();
        }
        const bool active_third_job = third_job_active();
        const bool active_second_job = second_job_active();
        draw_bitmap_text(active_third_job ? "SHOP" : (active_second_job ? "STK" : "JOB"), job_slots.label_x, job_slots.label_y, active_third_job ? 2.35F : 3.5F, ink);
        if (show_third_job) {
            draw_reward_badge(job_reward_badge, "+90", ready ? green : ink, ready);
        } else {
            draw_bitmap_text(active_second_job ? "+30" : "+8", active_second_job ? job_slots.value_x - 2.0F : job_slots.value_x + 8.0F, job_slots.value_y, active_second_job ? 2.45F : 3.2F, ready ? green : coin);
        }
        float duration = g_game_state.active_job_duration_ms > 0 ? (float)g_game_state.active_job_duration_ms : (float)FIRST_JOB_DURATION_MS;
        draw_progress_toy(job_slots.progress_x, job_slots.progress_y, job_slots.progress_w, 12.0F, (float)g_game_state.active_job_elapsed_ms / duration, art_ready);
    } else {
        if (art_ready && s_icon_coin_region != NT_ATLAS_INVALID_REGION) {
            draw_sprite_region_contain_center(s_icon_coin_region, job_slots.icon, 1.0F);
            nt_sprite_renderer_flush();
        }
        const float locked_ink[4] = {0.24F, 0.28F, 0.32F, 1.0F};
        draw_bitmap_text(third_job ? "SHOP" : (second_job ? "STK" : "JOB"), job_slots.label_x, job_slots.label_y, third_job ? 2.35F : 3.5F, first_upgrade_owned() ? ink : locked_ink);
        if (third_job) {
            draw_reward_badge(job_reward_badge, "+90", first_upgrade_owned() ? ink : locked_ink, first_upgrade_owned());
        } else {
            draw_bitmap_text(second_job ? "+30" : "+8", second_job ? job_slots.value_x - 2.0F : job_slots.value_x + 8.0F, job_slots.value_y, second_job ? 2.45F : 3.2F, first_upgrade_owned() ? coin : locked_ink);
        }
        if (first_upgrade_owned()) {
            draw_progress_toy(job_slots.progress_x, job_slots.progress_y, job_slots.progress_w, 12.0F, 1.0F, art_ready);
        } else {
            draw_bitmap_text("LOCK", job_slots.progress_x + 2.0F, s_job_rect.y + 42.0F, 1.9F, locked_ink);
        }
    }

    const float tab_a[4] = {0.10F, 0.38F, 0.88F, 1.0F};
    const float tab_b[4] = {0.88F, 0.18F, 0.56F, 1.0F};
    float tab_y = h - bottom_safe - tab_h;
    float tab_w = w * 0.5F;
    draw_screen_rect(0.0F, tab_y + tab_h, w, bottom_safe, bottom_shadow);
    if (!art_ready || s_runtime_tab_selected_region == NT_ATLAS_INVALID_REGION) {
        draw_screen_rect(0.0F, tab_y, tab_w - 1.0F, tab_h, tab_a);
        draw_screen_circle(tab_w * 0.5F, tab_y + (tab_h * 0.45F), 13.0F, white);
    }
    if (!art_ready || s_runtime_tab_locked_region == NT_ATLAS_INVALID_REGION) {
        draw_screen_rect(tab_w, tab_y, tab_w - 1.0F, tab_h, tab_b);
        draw_screen_circle(tab_w * 1.5F, tab_y + (tab_h * 0.45F), 13.0F, white);
    }
    draw_home_icon(tab_w * 0.5F, tab_y + (tab_h * 0.46F), 34.0F, ink);
    draw_city_icon(tab_w * 1.5F, tab_y + (tab_h * 0.46F), 34.0F, ink);

}

static float settings_button_x(void) { return ((float)g_nt_window.fb_width * 0.5F) + 3.0F; }
static float settings_button_y(void) {
    const float h = (float)g_nt_window.fb_height;
    return fmaxf(78.0F, fmaxf(UI_TOP_SAFE_MIN, h * 0.016F) + 54.0F);
}
static float settings_modal_x(void) { return ((float)g_nt_window.fb_width - SETTINGS_MODAL_W) * 0.5F; }
static float settings_modal_y(void) { return ((float)g_nt_window.fb_height - SETTINGS_MODAL_H) * 0.5F; }
static float settings_slider_x(void) { return settings_modal_x() + 72.0F; }
static float settings_master_slider_y(void) { return settings_modal_y() + 86.0F; }
static float settings_sfx_slider_y(void) { return settings_modal_y() + 154.0F; }

static void set_master_volume_from_x(float x) {
    char error[128] = {0};
    float volume = clamp01((x - settings_slider_x()) / SETTINGS_SLIDER_W);
    (void)game_state_action_set_master_volume(&g_game_state, volume, error, (int)sizeof(error));
}

static void set_sfx_volume_from_x(float x) {
    char error[128] = {0};
    float volume = clamp01((x - settings_slider_x()) / SETTINGS_SLIDER_W);
    (void)game_state_action_set_sfx_volume(&g_game_state, volume, error, (int)sizeof(error));
}

static void draw_slider(float x, float y, float w, float value, const float accent[4]) {
    const float track_col[4] = {0.18F, 0.20F, 0.24F, 1.0F};
    const float knob_col[4] = {0.94F, 0.96F, 0.90F, 1.0F};
    draw_screen_rect(x, y + 14.0F, w, 6.0F, track_col);
    draw_screen_rect(x, y + 14.0F, w * clamp01(value), 6.0F, accent);
    draw_screen_rect(x + (w * clamp01(value)) - 7.0F, y + 5.0F, 14.0F, 24.0F, knob_col);
}

static void draw_settings_ui(void) {
    const float button_col[4] = {0.14F, 0.32F, 0.42F, 1.0F};
    const float button_wire[4] = {0.55F, 0.88F, 0.92F, 1.0F};
    const float reset_col[4] = {0.90F, 0.48F, 0.18F, 1.0F};
    const float reset_wire[4] = {1.0F, 0.88F, 0.34F, 1.0F};
    const float icon_col[4] = {0.04F, 0.07F, 0.10F, 1.0F};
    const float inset = 4.0F;
    draw_screen_rect(s_reset_rect.x + inset, s_reset_rect.y + inset, s_reset_rect.w - (inset * 2.0F), s_reset_rect.h - (inset * 2.0F), reset_col);
    draw_screen_rect_wire(s_reset_rect.x + inset, s_reset_rect.y + inset, s_reset_rect.w - (inset * 2.0F), s_reset_rect.h - (inset * 2.0F), reset_wire);
    draw_reset_icon(s_reset_rect.x + (s_reset_rect.w * 0.5F), s_reset_rect.y + (s_reset_rect.h * 0.5F), 24.0F, icon_col);
    draw_screen_rect(settings_button_x() + inset, settings_button_y() + inset, SETTINGS_BUTTON_W - (inset * 2.0F), SETTINGS_BUTTON_H - (inset * 2.0F), button_col);
    draw_screen_rect_wire(settings_button_x() + inset, settings_button_y() + inset, SETTINGS_BUTTON_W - (inset * 2.0F), SETTINGS_BUTTON_H - (inset * 2.0F), button_wire);
    draw_audio_icon(settings_button_x() + (SETTINGS_BUTTON_W * 0.5F), settings_button_y() + (SETTINGS_BUTTON_H * 0.5F), 24.0F, icon_col);
    if (!s_settings_open) {
        return;
    }
    const float overlay_col[4] = {0.02F, 0.02F, 0.025F, 0.72F};
    const float modal_col[4] = {0.08F, 0.09F, 0.11F, 1.0F};
    const float modal_wire[4] = {0.76F, 0.80F, 0.68F, 1.0F};
    const float close_col[4] = {0.88F, 0.18F, 0.20F, 1.0F};
    const float accent_master[4] = {0.20F, 0.78F, 0.66F, 1.0F};
    const float accent_sfx[4] = {0.92F, 0.58F, 0.22F, 1.0F};
    float modal_x = settings_modal_x();
    float modal_y = settings_modal_y();
    draw_screen_rect(0.0F, 0.0F, (float)g_nt_window.fb_width, (float)g_nt_window.fb_height, overlay_col);
    draw_screen_rect(modal_x, modal_y, SETTINGS_MODAL_W, SETTINGS_MODAL_H, modal_col);
    draw_screen_rect_wire(modal_x, modal_y, SETTINGS_MODAL_W, SETTINGS_MODAL_H, modal_wire);
    draw_screen_rect(modal_x + SETTINGS_MODAL_W - SETTINGS_CLOSE_W - 18.0F, modal_y + 18.0F, SETTINGS_CLOSE_W, SETTINGS_CLOSE_H, close_col);
    draw_slider(settings_slider_x(), settings_master_slider_y(), SETTINGS_SLIDER_W, g_game_state.settings_master_volume, accent_master);
    draw_slider(settings_slider_x(), settings_sfx_slider_y(), SETTINGS_SLIDER_W, g_game_state.settings_sfx_volume, accent_sfx);
}

#ifndef NT_PLATFORM_WEB
static void write_pending_frame_capture(void) {
    if (!s_frame_capture_pending) {
        return;
    }
    s_frame_capture_pending = false;

    const int w = (int)g_nt_window.fb_width;
    const int h = (int)g_nt_window.fb_height;
    if (w <= 0 || h <= 0 || s_frame_capture_path[0] == '\0') {
        return;
    }

    uint8_t *rgba = (uint8_t *)malloc((size_t)w * (size_t)h * 4U);
    if (!rgba) {
        return;
    }

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    FILE *file = NULL;
#ifdef _WIN32
    (void)fopen_s(&file, s_frame_capture_path, "wb");
#else
    file = fopen(s_frame_capture_path, "wb");
#endif
    if (file) {
        (void)fprintf(file, "P6\n%d %d\n255\n", w, h);
        for (int y = h - 1; y >= 0; --y) {
            const uint8_t *row = rgba + ((size_t)y * (size_t)w * 4U);
            for (int x = 0; x < w; ++x) {
                (void)fwrite(row + ((size_t)x * 4U), 1, 3, file);
            }
        }
        (void)fclose(file);
    }
    free(rgba);
}
#endif

static bool update_settings_ui_interaction(void) {
    const float px = g_nt_input.pointers[0].x;
    const float py = g_nt_input.pointers[0].y;
    UiRect open = {settings_button_x(), settings_button_y(), SETTINGS_BUTTON_W, SETTINGS_BUTTON_H};
    UiRect close = {settings_modal_x() + SETTINGS_MODAL_W - SETTINGS_CLOSE_W - 18.0F, settings_modal_y() + 18.0F, SETTINGS_CLOSE_W, SETTINGS_CLOSE_H};
    bool blocks_scene = s_settings_open || point_in_rect(px, py, open);
    if (nt_input_key_is_pressed(NT_KEY_S)) {
        s_settings_open = !s_settings_open;
        s_active_settings_slider = 0;
        return true;
    }
    if (nt_input_mouse_is_released(NT_BUTTON_LEFT)) {
        s_active_settings_slider = 0;
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT) && point_in_rect(px, py, open)) {
        s_settings_open = true;
        return true;
    }
    if (!s_settings_open) {
        return blocks_scene;
    }
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT) && point_in_rect(px, py, close)) {
        s_settings_open = false;
        s_active_settings_slider = 0;
        return true;
    }
    UiRect master = {settings_slider_x(), settings_master_slider_y(), SETTINGS_SLIDER_W, SETTINGS_SLIDER_H};
    UiRect sfx = {settings_slider_x(), settings_sfx_slider_y(), SETTINGS_SLIDER_W, SETTINGS_SLIDER_H};
    if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT) && point_in_rect(px, py, master)) {
        s_active_settings_slider = 1;
    } else if (nt_input_mouse_is_pressed(NT_BUTTON_LEFT) && point_in_rect(px, py, sfx)) {
        s_active_settings_slider = 2;
    }
    if (nt_input_mouse_is_down(NT_BUTTON_LEFT)) {
        if (s_active_settings_slider == 1) {
            set_master_volume_from_x(px);
        } else if (s_active_settings_slider == 2) {
            set_sfx_volume_from_x(px);
        }
    }
    return blocks_scene;
}

static void update_game_input(void) {
    if (!nt_input_mouse_is_pressed(NT_BUTTON_LEFT)) {
        return;
    }
    const float px = g_nt_input.pointers[0].x;
    const float py = g_nt_input.pointers[0].y;
    char error[128] = {0};
    if (point_in_rect(px, py, s_reset_rect)) {
        (void)game_state_action_reset_playtest(&g_game_state, error, (int)sizeof(error));
    } else if (point_in_rect(px, py, s_do67_rect)) {
        (void)game_state_action_do67(&g_game_state, error, (int)sizeof(error));
    } else if (point_in_rect(px, py, s_upgrade_rect)) {
        if (!first_upgrade_owned()) {
            (void)game_state_action_buy_first_upgrade(&g_game_state, error, (int)sizeof(error));
        } else if (!second_upgrade_owned()) {
            (void)game_state_action_buy_second_upgrade(&g_game_state, error, (int)sizeof(error));
        } else if (!third_upgrade_owned()) {
            (void)game_state_action_buy_third_upgrade(&g_game_state, error, (int)sizeof(error));
        } else if (!fourth_upgrade_owned()) {
            (void)game_state_action_buy_fourth_upgrade(&g_game_state, error, (int)sizeof(error));
        } else if (!fifth_upgrade_owned()) {
            (void)game_state_action_buy_fifth_upgrade(&g_game_state, error, (int)sizeof(error));
        }
    } else if (point_in_rect(px, py, s_job_rect)) {
        if (job_ready()) {
            (void)game_state_action_claim_first_job(&g_game_state, error, (int)sizeof(error));
        } else {
            (void)game_state_action_start_first_job(&g_game_state, error, (int)sizeof(error));
        }
    }
}

static void frame(void) {
    nt_window_poll();
    nt_input_poll();
    nt_devapi_set_frame(g_nt_app.frame);
    nt_devapi_set_view((float)g_nt_window.fb_width, (float)g_nt_window.fb_height, (float)g_nt_window.width, (float)g_nt_window.height);
    nt_devapi_clear_ui_elements();
    register_game_ui();
    nt_devapi_net_poll();
    nt_devapi_apply_pending();
    nt_resource_step();
    nt_material_step();
    resolve_runtime_art();
    autosave_try_load();

    bool page_paused = false;
#ifdef NT_PLATFORM_WEB
    page_paused = s_web_page_paused;
    if (s_web_flush_requested) {
        s_web_flush_requested = false;
        autosave_flush_if_dirty();
    }
#endif
    char error[128] = {0};
    int delta_ms = (int)(g_nt_app.dt * 1000.0F);
    if (!page_paused) {
        (void)game_state_action_tick(&g_game_state, delta_ms, error, (int)sizeof(error));
    }

    bool ui_blocks_pointer = page_paused || update_settings_ui_interaction();
    if (!ui_blocks_pointer && !page_paused) {
        update_game_input();
    } else if (page_paused) {
        s_active_settings_slider = 0;
    }
    update_web_qa_snapshot();
#ifndef NT_PLATFORM_WEB
    if (nt_input_key_is_pressed(NT_KEY_ESCAPE)) {
        if (s_settings_open) {
            s_settings_open = false;
            s_active_settings_slider = 0;
        } else {
            nt_app_quit();
        }
    }
#endif

    nt_gfx_begin_frame();
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.34F, 0.78F, 1.0F, 1.0F}, .clear_depth = 1.0F});
    draw_game_screen();
    draw_settings_ui();
    nt_shape_renderer_flush();
    nt_gfx_end_pass();
#ifndef NT_PLATFORM_WEB
    write_pending_frame_capture();
#endif
    nt_gfx_end_frame();
    nt_window_swap_buffers();
    autosave_flush_if_dirty();
#ifdef NT_PLATFORM_WEB
    update_web_qa_snapshot();
#endif
}

int main(int argc, char **argv) {
    nt_engine_config_t config = {0};
    config.app_name = "game_67_idle";
    config.version = 1;
    if (nt_engine_init(&config) != NT_OK) {
        return 1;
    }

    g_nt_window.width = 390;
    g_nt_window.height = 844;
    nt_window_init();
    nt_input_init();
    nt_gfx_init(&(nt_gfx_desc_t){.max_shaders = 32, .max_pipelines = 16, .max_buffers = 128, .max_textures = 64, .max_meshes = 64, .depth = true});
    nt_gfx_register_global_block("Globals", 0);
    nt_http_init();
    nt_fs_init();
    nt_hash_init(&(nt_hash_desc_t){0});
    nt_resource_init(&(nt_resource_desc_t){0});
    nt_resource_set_activator(NT_ASSET_TEXTURE, nt_gfx_activate_texture, nt_gfx_deactivate_texture);
    nt_resource_set_activator(NT_ASSET_SHADER_CODE, nt_gfx_activate_shader, nt_gfx_deactivate_shader);
    nt_atlas_init();
    nt_material_init(&(nt_material_desc_t){.max_materials = 8});
    nt_sprite_renderer_init(NULL);
    nt_shape_renderer_init();
    s_sprite_frame_ubo = nt_gfx_make_buffer(&(nt_buffer_desc_t){
        .type = NT_BUFFER_UNIFORM,
        .usage = NT_USAGE_DYNAMIC,
        .size = sizeof(nt_frame_uniforms_t),
        .label = "game_67_idle_sprite_frame_uniforms",
    });
    game_state_init();
    game_storage_init(&(GameStorageConfig){
        .namespace_name = GAME_STORAGE_NAMESPACE,
        .native_root = GAME_STORAGE_NATIVE_ROOT,
    });
    autosave_init(has_arg(argc, argv, "--fresh-state"), has_arg(argc, argv, "--disable-autosave"));
    init_runtime_art();

#ifdef NT_PLATFORM_WEB
    nt_platform_web_loading_complete();
    game67_web_install_page_lifecycle();
#endif

#if NT_DEVAPI_ENABLED
    game_state_set_changed_callback(on_game_state_changed, NULL);
    uint16_t devapi_port = 0;
    if (parse_devapi_port(argc, argv, &devapi_port)) {
        nt_devapi_init();
        nt_devapi_register_builtins();
        nt_devapi_register("game.state", ep_game_state, NULL);
        nt_devapi_register("game.reset_playtest", ep_reset_playtest, NULL);
#ifndef NT_PLATFORM_WEB
        nt_devapi_register("game.capture.framebuffer", ep_capture_framebuffer, NULL);
#endif
        game_state_register_devapi();
#ifndef NT_PLATFORM_WEB
        s_devapi_started = nt_devapi_net_start(devapi_port);
#endif
    }
#else
    (void)argc;
    (void)argv;
#endif

    nt_app_run(frame);

#ifndef NT_PLATFORM_WEB
#if NT_DEVAPI_ENABLED
    if (s_devapi_started) {
        nt_devapi_net_stop();
    }
    nt_devapi_shutdown();
#endif
    nt_sprite_renderer_shutdown();
    nt_shape_renderer_shutdown();
    nt_material_destroy(s_sprite_material);
    nt_material_shutdown();
    nt_resource_shutdown();
    nt_fs_shutdown();
    nt_http_shutdown();
    nt_hash_shutdown();
    nt_gfx_destroy_buffer(s_sprite_frame_ubo);
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
#endif
    return 0;
}
