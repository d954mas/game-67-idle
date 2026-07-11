#include "features/game_features.h"
#include "features/dress_room/dress_room.h"
#include "features/settings/settings.h"
#include "ui/nt_ui.h"
#if defined(__EMSCRIPTEN__)
#include "features/platform_sdk/platform_sdk_web.h"
#endif
#include "features/progression/progression.h" /* И3a: progression_update() tick (auto/threshold level-ups) */
#include "ui/platform_sdk_debug.h"
#include "ui/ui_runtime.h"
#include "app/nt_app.h" /* g_nt_app.dt */
#if GAME_EVENTS_LOG_MIRROR
#include "game_events_log_mirror.h"
#endif
#if NT_DEVAPI_ENABLED
#include "game_events_devapi.h" /* E3: DevAPI tail recorder */
#endif
#if FEATURE_GAME_ANALYTICS
#include "game_analytics.h" /* E4: local analytics NDJSON writer */
#endif

/* settings — первая мигрированная фича (И1): draw_ui теперь владеет
   ui_runtime-кадром и рисует settings по одной строке (z-order). Рендер-
   системы (render_mesh_draw/hud_draw) ПОКА прямые вызовы шелла в main.c,
   поэтому draw_world остаётся заглушкой. */

void game_features_init(World *w) {
#if defined(__EMSCRIPTEN__)
    platform_sdk_install_web_backend();
#endif
    platform_sdk_debug_init();
    dress_room_init();
    (void)w;
}

void game_features_update(World *w, float dt) {
    (void)w;
    dress_room_show_tick(dt);
    progression_update();
}

/* Event consumers reset cursors when game_events_tick changes. The fixed
   frame arena keeps event and payload pointers stable until reset. */
void game_features_react(World *w) {
    (void)w; /* TODO: add event reactors using game_event_log(). */
}

void game_features_record(World *w) {
    (void)w;
#if GAME_EVENTS_LOG_MIRROR
    game_events_log_mirror_record();
#endif
#if NT_DEVAPI_ENABLED
    game_events_devapi_record(); /* E3: DevAPI tail — render-at-copy into the ring */
#endif
#if FEATURE_GAME_ANALYTICS
    game_analytics_record(); /* E4: local analytics NDJSON writer (arena alive; reads only) */
#endif
}

void game_features_draw_world(World *w) { (void)w; /* TODO: add feature render systems. */ }
void game_features_draw_ui(World *w) {
    /* UI-слой фич: агрегатор владеет ui_runtime-кадром; z-order = call order. */
    if (ui_runtime_begin(g_nt_app.dt)) {
        /* When settings is open, freeze dress-room input (review HIGH: click-through). */
        dress_room_draw_ui(ui_runtime_ctx(), !settings_is_open());
        settings_draw_ui(ui_runtime_ctx(), w);
        /* platform_sdk_debug_draw_ui disabled for player-facing playtest captures */
        ui_runtime_end();
    }
}
void game_features_shutdown(World *w) {
    (void)w; /* TODO: per-feature shutdown */
    platform_sdk_debug_shutdown();
}
