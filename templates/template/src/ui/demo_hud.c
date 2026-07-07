/* demo_hud.c -- КОМПОЗИЦИЯ ИГРЫ (game-owned), не фича (§6.5 OQ5). Единственный
   TU, которому легально включать СРАЗУ resource_panel.h + items.h +
   progression.h: он связывает две L2-фичи (панель, прогрессия) с L1 (items)
   через геттеры -- фичи так делать не могут (греп-гейты G10/G12 бы упали). */
#include "ui/demo_hud.h"

#include "features/resource_panel/resource_panel.h"
#include "features/items/items.h"
#include "features/progression/progression.h"

#include "app/nt_app.h" /* dt для idle-аккумулятора */

static int64_t get_gold(void *ud) {
    (void)ud;
    return items_purse("tmpl.gold");
}
static int64_t get_hero_xp(void *ud) {
    (void)ud;
    return progression_xp_current("hero");
}
static int64_t get_hero_needed(void *ud) {
    (void)ud;
    return progression_xp_needed("hero");
}
static int64_t get_hero_level(void *ud) {
    (void)ud;
    return progression_level("hero");
}

/* Демо idle-доход xp: float-аккумулятор в game glue -> флаш в i64 (паттерн Р1;
   ДРОБНОЕ производство копит в glue, НЕ в count). Кормит purse tmpl.xp;
   progression auto-трек hero авто-покупает уровни -> живой count-up + levelup
   для глаза лида (см. README "Demo idle-income + autosave churn" -- И3a/И3b
   cross-note). Ноль (или отрицательный) DEMO_XP_PER_SEC -> статичный бар,
   гейты §8 всё равно проходят по рендеру. */
#define DEMO_XP_PER_SEC 8.0F
static float s_xp_accum;

void demo_hud_update(float dt) {
    s_xp_accum += DEMO_XP_PER_SEC * dt;
    if (s_xp_accum >= 1.0F) {
        int64_t whole = (int64_t)s_xp_accum;
        s_xp_accum -= (float)whole;
        items_add("purse", "tmpl.xp", whole, "loot:demo_idle");
    }
}

void demo_hud_draw_ui(nt_ui_context_t *ctx) {
    const resource_panel_entry_t entries[] = {
        {.id = "gold", .label = "Gold", .kind = RESOURCE_PANEL_COUNTER, .value = get_gold},
        {.id = "hero", .label = "Hero", .kind = RESOURCE_PANEL_BAR, .value = get_hero_xp, .max = get_hero_needed, .level = get_hero_level},
    };
    resource_panel_ui(ctx, entries, 2);
}
