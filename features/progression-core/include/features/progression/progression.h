#ifndef FEATURES_PROGRESSION_H
#define FEATURES_PROGRESSION_H
// feature-layer: L2
/* Единственный публичный хедер фичи progression — вся публичная поверхность;
   остальное в папке static. L2 surface: зависит от L0-шелла (game_save-toolkit +
   gsj_ + движок) И от L1-фичи items (единственное фича-ребро — доказательство
   слоёвости: manual/auto-режимы читают+тратят purse через items API). Обратного
   ребра нет: items НИКОГДА не включает progression.h. Вся математика уровней/xp —
   int64; кривая запечена в int64-таблицу на codegen (ноль float-формул в C). */
#include "features/items/items.h" /* L2->L1: items_purse / items_add / items_remove */
#include <stdbool.h>
#include <stdint.h>

typedef enum progression_mode_t {
    PROGRESSION_MODE_MANUAL = 0,
    PROGRESSION_MODE_AUTO,
    PROGRESSION_MODE_THRESHOLD,
} progression_mode_t;

/* on_level_up-эмиссия: РОВНО одна из форм (другой указатель NULL / поле игнор):
   currency (def_id!=NULL) -> items_add(purse, def_id, amount);
   xp-to-track (to_track!=NULL) -> progression_add_xp(to_track, amount) (каскад).
   Codegen (features/progression-core/scripts/generate_progression_tracks.py) НЕ печёт эту ветку в И3 (LEAN-порез
   A) — каждый испечённый трек несёт .on_level_up=NULL/.on_level_up_count=0; этот
   рантайм-путь покрыт ТОЛЬКО рукописным тест-каталогом (tests/test_progression_catalog.c). */
typedef struct progression_emit_t {
    const char *def_id;   /* валюта в purse, либо NULL */
    const char *to_track; /* трек-получатель xp (каскад), либо NULL */
    int64_t amount;
} progression_emit_t;

typedef struct progression_track_def_t {
    const char *id;
    progression_mode_t mode;
    const char *currency_def;              /* purse-валюта manual/auto; NULL для threshold */
    int max_level;
    const int64_t *cost;                   /* запечённая таблица [0..max_level-1]; cost[L] = цена L->L+1 */
    int cost_count;                        /* == max_level */
    const progression_emit_t *on_level_up; /* NULL если нет */
    int on_level_up_count;
} progression_track_def_t;

const progression_track_def_t *progression_track_def(const char *track); /* NULL если неизвестен */

/* ---- Запросы (чистые чтения) ---- */
int progression_level(const char *track);         /* 0 если трек без записи/неизвестен */
int progression_max_level(const char *track);
int64_t progression_xp_current(const char *track); /* manual/auto: items_purse(currency); threshold: внутр. xp */
int64_t progression_xp_needed(const char *track);  /* cost(level); 0 если на max */
bool progression_can_level_up(const char *track);  /* xp_current >= xp_needed && level<max */

/* ---- Мутации (reason обязателен, дизайн §10; формат verb:subject) ---- */
bool progression_level_up(const char *track, const char *reason); /* MANUAL: тратит purse; true если поднял */
void progression_add_xp(const char *track, int64_t n, const char *reason);        /* THRESHOLD: копит внутр. xp (резолв в update) */
void progression_set_level(const char *track, int level, const char *reason);     /* Р6: пролог (сильный герой) */
void progression_reset(const char *track, const char *reason);                    /* Р6: престиж (level=0 И внутр. xp=0) */

/* ---- Тик: auto/threshold авто-лвлапы, T5-капы, эмит progression.levelup ---- */
void progression_update(void);

#endif /* FEATURES_PROGRESSION_H */
