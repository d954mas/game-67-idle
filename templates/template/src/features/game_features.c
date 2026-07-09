#include "features/game_features.h"
#include "systems/sys_move.h"
#include "features/settings/settings.h"
#if defined(__EMSCRIPTEN__)
#include "features/platform_sdk/platform_sdk_web.h"
#endif
#include "features/progression/progression.h" /* И3a: progression_update() tick (auto/threshold level-ups) */
#include "ui/demo_hud.h" /* И3b: demo composition (resource_panel over items/progression) */
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
    (void)w; /* TODO(feature-migration): per-feature <id>_init(w) здесь */
}

void game_features_update(World *w, float dt) {
    sys_move(w, dt); /* мировая симуляция шаблона; здесь же фичи эмитят события */
    demo_hud_update(dt); /* И3b: demo idle-доход ПЕРЕД авто-покупкой того же кадра */
    progression_update(); /* И3a: auto/threshold tick (T5 HARD-капы внутри) */
    /* TODO(feature-migration): <id>_update(w, dt) по строке на фичу */
}

/* КАНОН-ИДИОМ потребителя (образец для будущих реакторов, E2+; §E1.4). Один
   обязательный инвариант: сброс курсора по СМЕНЕ tick, НЕ по числу событий
   (HIGH-2). Указатели стабильны весь кадр (фиксированная арена) -> `log`
   фетчится ОДИН раз, удержание e/payload через emit ЗАКОННО:

   static int      s_pos;            // курсор потребителя
   static uint32_t s_last_tick;      // для детекта нового кадра

   uint32_t tick = game_events_tick();
   if (tick != s_last_tick) { s_last_tick = tick; s_pos = 0; }  // новый кадр -> сброс

   int n; const game_event_t *log = game_event_log(&n);   // стабилен весь кадр
   for (; s_pos < n; ++s_pos) {
       const game_event_t *e = &log[s_pos];
       if (e->type.value != WANTED.value) continue;
       ... react ...   // может эмитить каскад: следующее react-поколение увидит его
   }

   Почему сброс по tick, а НЕ по числу событий: старый способ (if (s_pos > n)
   s_pos = 0) не срабатывает, когда новый кадр имеет >= событий, чем осталось
   курсору (тихая потеря, event §7). Сброс по смене tick надёжен всегда. */
void game_features_react(World *w) {
    (void)w; /* TODO(E2+): реакторы читают game_event_log(&n), канон-идиом выше (§E1.4) */
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

void game_features_draw_world(World *w) { (void)w; /* TODO: см. §E1.5 */ }
void game_features_draw_ui(World *w) {
    /* UI-слой фич: агрегатор владеет ui_runtime-кадром; каждая фича получает
       ctx и рисует свой слой ОДНОЙ строкой, порядок вызовов = z-order. */
    if (ui_runtime_begin(g_nt_app.dt)) {
        demo_hud_draw_ui(ui_runtime_ctx());    /* И3b: resource_panel (HUD, снизу) */
        settings_draw_ui(ui_runtime_ctx(), w); /* settings overlay (сверху) */
        platform_sdk_debug_draw_ui(ui_runtime_ctx());
        ui_runtime_end();
    }
}
void game_features_shutdown(World *w) {
    (void)w; /* TODO: per-feature shutdown */
    platform_sdk_debug_shutdown();
}
