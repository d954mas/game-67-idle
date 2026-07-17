#ifndef GAME_SAVE_H
#define GAME_SAVE_H

#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"

#ifndef GAME_SAVE_MAX_FRAGMENTS
#define GAME_SAVE_MAX_FRAGMENTS 32
#endif

/* ---- Контракт фрагмента: весь ABI между шеллом и фичей ---- */
typedef bool (*GameSaveMigrateFn)(cJSON *frag, char *err, int cap); /* v(i)->v(i+1) */
typedef bool (*GameSaveDocumentMigrateFn)(cJSON *features, char *err, int cap); /* save v(i)->v(i+1) */
typedef bool (*GameSaveDocumentValidateFn)(const cJSON *features, char *err, int cap);

typedef struct GameSaveFragment {
    const char *id;                  /* ключ в features{} и C-префикс; [a-z_][a-z0-9_]* */
    int version;                     /* = steps_count + 1 */
    const GameSaveMigrateFn *steps;  /* NULL пока миграций нет (90% случаев) */
    void  (*reset)(void);            /* нейтральные ПУСТЫЕ дефолты */
    void  (*on_new_game)(void);      /* NULLABLE: стартовый контент, только свежий сейв */
    cJSON*(*to_json)(void);          /* только данные, без "v" (штампует шелл) */
    bool  (*from_json)(const cJSON *frag, char *err, int cap); /* толерантный */
    void  (*reconcile)(void);        /* NULLABLE: пост-load фиксап (карантин items) */
    cJSON*(*get_path_json)(const char *sub, char *err, int cap);       /* NULLABLE: DevAPI read (A5) */
    bool  (*set_path_json)(const char *sub, const cJSON *v, char *err, int cap); /* NULLABLE: DevAPI write (A5) */
    cJSON*(*schema_json)(void);      /* NULLABLE */
} GameSaveFragment;

/* Регистрация ДО первого load; порядок = порядок reconcile/on_new_game.
   Фрагмент game регистрируется ПОСЛЕДНИМ. Указатель должен пережить рантайм. */
void game_save_register_fragment(const GameSaveFragment *fragment);

/* Ordered game-owned cross-fragment migrations. steps[v-1] migrates the
   features object from save_version v to v+1. A document validator is mandatory
   for old versions and must stage-parse every affected fragment without live
   mutation. Load/import run both on a deep copy before publishing state. */
void game_save_set_document_migrations(const GameSaveDocumentMigrateFn *steps, int step_count);

/* One game-owned cross-fragment invariant checked before a save document is
   published and after raw DevAPI writes. NULL restores fragment-only behavior. */
void game_save_set_document_validator(GameSaveDocumentValidateFn validator);
bool game_save_validate_current(char *error, int error_cap);

/* ---- Registry read-access for the DevAPI dispatch (registry dispatch contract).
   Read-only view of the registry filled by game_save_register_fragment. ---- */
int  game_save_fragment_count(void);                             /* число зарегистрированных */
const GameSaveFragment *game_save_fragment_at(int index);        /* NULL если вне диапазона */
const GameSaveFragment *game_save_find_fragment(const char *id); /* NULL если ключ неизвестен */

/* ---- Orphan read-access (retained unknown feature keys). Read-only view of the
   retained-orphan set, symmetric to game_save_fragment_count/at, for the DevAPI aggregate's
   "orphans" section. The returned subtree is OWNED by game_save — callers must not free or
   mutate it (the aggregate hands out cJSON_Duplicate copies only). ---- */
int  game_save_orphan_count(void);                               /* число удержанных сирот */
const cJSON *game_save_orphan_at(int index, const char **id);    /* субтри + id (out-param); NULL вне диапазона (id не трогается) */

#if NT_DEVAPI_ENABLED
/* Регистрирует 7 команд game.state.* над реестром фрагментов (A5; заменяет
   генерируемый <id>_state_register_devapi). Хендлеры читают реестр ЛЕНИВО в момент
   ВЫЗОВА команды (бот подключается в кадровом цикле, много позже init), поэтому
   порядок регистрации команд относительно game_save_register_fragment НЕ важен —
   сохранённая точка вызова (main.c, внутри devapi_start()) корректна как есть.
   Звать один раз после nt_devapi_init(). Тело —
   templates/template/src/game_save_devapi.c. */
void game_save_register_devapi(void);
#endif

/* ---- Статус и результат загрузки (Р7, Р10) ---- */
typedef enum {
    GAME_SAVE_LOAD_FRESH = 0,      /* файла нет -> reset+on_new_game+save */
    GAME_SAVE_LOAD_LOADED,         /* primary распарсен */
    GAME_SAVE_LOAD_RECOVERED_BAK,  /* primary плох, поднят .bak */
    GAME_SAVE_LOAD_CORRUPT_RESET,  /* primary+bak плохи -> карантин+new_game, автосейв на паузе */
    GAME_SAVE_LOAD_NEWER,          /* сейв новее билда -> НОЛЬ записи, только чтение/экспорт */
} game_save_load_status_t;

typedef struct {
    game_save_load_status_t status;
    int         reset_fragment_count;                       /* сколько фрагментов упало в дефолт */
    const char *reset_fragments[GAME_SAVE_MAX_FRAGMENTS];   /* их id (для тоста «X не загрузился») */
    char        message[128];                               /* краткая диагностика */
} game_save_load_result_t;

/* Инициализация оркестратора (после регистрации ВСЕХ фрагментов). */
void game_save_init(void);

/* Грузит автослот (GAME_SAVE_AUTOSAVE_SLOT). Обычные fragment-local ошибки
   изолированы; versioned document migrations stage+validate весь features{} до
   публикации любого затронутого фрагмента. Заполняет *result. */
void game_save_load(game_save_load_result_t *result);

/* Явная новая игра: reset всех -> on_new_game всех -> save -> возобновляет автосейв (Р10). */
bool game_save_new_game(char *error, int error_cap);

/* Р11 «Hold to reset progress» (T0327 hygiene): фича (settings_screen) зовёт ИЗ draw_ui,
   не немедленно -- шелл применяет в начале СЛЕДУЮЩЕГО кадра (см. game_save_apply_pending_
   new_game). skip_fragment_id NULLABLE -- id ОДНОГО фрагмента, который НЕ трогать (settings/
   громкости: "не их кнопка"); NULL = как game_save_new_game, без исключений. Повторный
   запрос до применения просто перезаписывает предыдущий (идемпотентно). */
void game_save_request_new_game(const char *skip_fragment_id);

/* Шелл-only: применяет отложенный запрос (reset+on_new_game всех КРОМЕ skip -> force-save),
   если есть; иначе no-op. Возвращает true, если применил -- сигнал вызывающему (main.c)
   сбросить то, что game_save не знает (позицию игрока и т.п.) в том же кадре. Звать ОДИН
   раз в начале frame(), до game_features_update (EMIT-фаза). */
bool game_save_apply_pending_new_game(void);

/* Синхронный форс-сейв в обход дебаунса (visibility-flush). */
bool game_save_flush(char *error, int error_cap);

/* Кадровый тик: сейв при (now-dirty_at >= DEBOUNCE) || (now-last_save >= MAX_INTERVAL).
   No-op пока не dirty или автосейв на паузе (CORRUPT_RESET/NEWER до new_game). */
void game_save_tick(void);

/* Пометка «есть несохранённое». dirty_at = момент ПЕРВОЙ пометки после чистого
  . Зовут мутаторы фич/UI. */
void game_save_mark_dirty(void);

/* Wall-clock ms сохранённого saved_at (оффлайн-Δt для идла; кламп отрицательных —
   задача игры). 0 если ещё не грузили/сохраняли. */
int64_t game_save_last_saved_at(void);

/* Персистентность недоступна (web квота/private): игра может показать тост. */
bool game_save_is_unpersisted(void);

/* Экспорт/импорт строкой (Р12) — тем же transform-путём; валидация конверта. */
char *game_save_export_string(char *error, int error_cap);   /* malloc, free вызывающим */
bool  game_save_import_string(const char *text, char *error, int error_cap);

/* Web: ставит visibilitychange(->hidden) + pagehide -> game_save_flush (НЕ
   beforeunload). No-op на native. */
void game_save_install_web_flush(void);

/* ---- Transform-шов. Дефолт пуст -> сейв = читаемый JSON ('{'). ---- */
typedef struct {
    const char *id;
    char *(*encode)(const char *in, char *error, int error_cap);  /* malloc out; save-порядок */
    char *(*decode)(const char *in, char *error, int error_cap);  /* malloc out; load-реверс */
} game_save_transform_t;

/* Устанавливает упорядоченную цепочку (save: по порядку; load: реверс). Пустая по
   умолчанию. Автодетект на load: префикс "NTSV1:<ids>:" -> трансформы; '{' -> плоский JSON. */
void game_save_set_transforms(const game_save_transform_t *chain, int count);

#ifdef GAME_SAVE_TESTING
/* Тест-шов: инжектит ОБА часа — монотонные (debounce/MAX_INTERVAL) и
   wall-clock (saved_at). Объявлено только под GAME_SAVE_TESTING. */
void game_save__set_clocks_for_test(int64_t (*mono)(void), int64_t (*wall)(void));
#endif

#endif /* GAME_SAVE_H */
