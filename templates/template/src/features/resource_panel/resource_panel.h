#ifndef FEATURES_RESOURCE_PANEL_H
#define FEATURES_RESOURCE_PANEL_H
// feature-layer: L2
/* Единственный публичный хедер фичи resource_panel — вся публичная поверхность.
   Виджет-фича: entries + геттеры даёт ИГРА (композиция). НЕ включает НИ ОДНОГО
   другого src/features/<x> хедера (греп-гейт) — знает про items/progression НОЛЬ; читает
   значения ТОЛЬКО через геттеры entry. Арт — хендлы-в-конфиге с graceful-фолбэком.
   int64 везде; count-up-интерполяция (float) — ВНУТРИ, не в логике значений. */
#include "atlas/nt_atlas.h" /* nt_atlas_region_ref_t (движок, НЕ фича) */
#include "ui/nt_ui.h"       /* nt_ui_context_t (движок, НЕ фича) */
#include <stdbool.h>
#include <stdint.h>

typedef enum resource_panel_kind_t {
    RESOURCE_PANEL_COUNTER = 0, /* одно число (золото) */
    RESOURCE_PANEL_BAR,         /* прогресс value/max + уровень-подпись (xp-бар) */
} resource_panel_kind_t;

typedef struct resource_panel_entry_t {
    const char *id;                    /* СТАБИЛЬНЫЙ id: ключ diff/count-up + anchor-lookup */
    const char *label;                 /* имя ресурса ("Gold"/"Hero") */
    resource_panel_kind_t kind;
    const nt_atlas_region_ref_t *icon; /* ОПЦ. хендл арта от игры; NULL -> graceful фолбэк (rect/без иконки) */
    void *ud;                          /* прозрачный, в каждый геттер */
    int64_t (*value)(void *ud);        /* ОБЯЗАТЕЛЕН. counter: число; bar: текущий прогресс (xp) */
    int64_t (*max)(void *ud);          /* bar: знаменатель (xp до след. уровня); NULL для counter/при неизвестном max */
    int64_t (*level)(void *ud);        /* bar ОПЦ.: уровень для подписи; NULL -> без уровня */
} resource_panel_entry_t;
/* Контракт NULL: `value` ОБЯЗАТЕЛЕН (панель зовёт его всегда). `max`/`level` —
   опциональны: панель ГАРДИТ их на NULL перед вызовом. `max==NULL` -> форма bar без
   знаменателя рисуется как счётчик-с-меткой (без заливки) ИЛИ игнорится (см. §6.3 п.1). */

/* Рисует entries по строке (порядок = вертикальный z/layout). Вызывается КАЖДЫЙ кадр
   игрой (из game_features_draw_ui через композицию). Внутри: poll геттеров -> diff ->
   count-up displayed -> акцент при смене -> render (счётчик/бар). */
void resource_panel_ui(nt_ui_context_t *ctx, const resource_panel_entry_t *entries, int count);

/* Шов под будущую coin_fx: экранная позиция (центр иконки/значения) последнего
   отрисованного entry по id. false если id не рисовался в этом кадре. Транзиентно
   (пересчитывается каждый кадр). ЕДИНСТВЕННЫЙ аксессор шва (карточка). */
bool resource_panel_anchor(const char *entry_id, float *out_x, float *out_y);

#endif /* FEATURES_RESOURCE_PANEL_H */
