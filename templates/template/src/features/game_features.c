#include "features/game_features.h"
#include "systems/sys_move.h"

/* Что стало «фичей» в скелете (E1 решение): НИЧТО формально (папок
   src/features/<id>/ E1 не создаёт — отдельный трек, feature_architecture §4
   п.1). Единственный существующий game-системный вызов, продетый через
   агрегатор, — sys_move в фазе update (мировая симуляция = естественный дом
   эмиттеров). Рендер-системы (render_mesh_draw/hud_draw/sys_settings_ui)
   ОСТАЮТСЯ прямыми вызовами шелла в frame() с TODO-якорем (§E1.4/§E1.5) —
   поэтому draw_world/draw_ui объявлены и определены пустыми здесь. */

void game_features_init(World *w) {
    (void)w; /* TODO(feature-migration): per-feature <id>_init(w) здесь */
}

void game_features_update(World *w, float dt) {
    sys_move(w, dt); /* мировая симуляция шаблона; здесь же фичи эмитят события */
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
    (void)w; /* TODO(E3/E4): рекордеры (DevAPI tail E3, аналитика E4) */
}

void game_features_draw_world(World *w) { (void)w; /* TODO: см. §E1.5 */ }
void game_features_draw_ui(World *w) { (void)w; /* TODO: см. §E1.5 */ }
void game_features_shutdown(World *w) { (void)w; /* TODO: per-feature shutdown */ }
