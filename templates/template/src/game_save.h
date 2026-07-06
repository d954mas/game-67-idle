#ifndef GAME_SAVE_H
#define GAME_SAVE_H

#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"

#ifndef GAME_SAVE_MAX_FRAGMENTS
#define GAME_SAVE_MAX_FRAGMENTS 32
#endif

/* ---- Контракт фрагмента: весь ABI между шеллом и фичей (§3 + §14 п.2) ---- */
typedef bool (*GameSaveMigrateFn)(cJSON *frag, char *err, int cap); /* v(i)->v(i+1) */

typedef struct GameSaveFragment {
    const char *id;                  /* ключ в features{} и C-префикс; [a-z_][a-z0-9_]* */
    int version;                     /* = steps_count + 1 */
    const GameSaveMigrateFn *steps;  /* NULL пока миграций нет (90% случаев) */
    void  (*reset)(void);            /* нейтральные ПУСТЫЕ дефолты (§14 п.2) */
    void  (*on_new_game)(void);      /* NULLABLE: стартовый контент, только свежий сейв */
    cJSON*(*to_json)(void);          /* только данные, без "v" (штампует шелл) */
    bool  (*from_json)(const cJSON *frag, char *err, int cap); /* толерантный */
    void  (*reconcile)(void);        /* NULLABLE: пост-load фиксап (карантин items) */
    cJSON*(*get_path_json)(const char *sub, char *err, int cap);       /* NULLABLE: DevAPI read (A5) */
    bool  (*set_path_json)(const char *sub, const cJSON *v, char *err, int cap); /* NULLABLE: DevAPI write (A5) */
    cJSON*(*schema_json)(void);      /* NULLABLE */
} GameSaveFragment;

/* Регистрация ДО первого load; порядок = порядок reconcile/on_new_game.
   Фрагмент game регистрируется ПОСЛЕДНИМ (§14 п.2). Указатель должен пережить рантайм. */
void game_save_register_fragment(const GameSaveFragment *fragment);

/* ---- Registry read-access for the DevAPI dispatch (§8 «диспатч по реестру»).
   Read-only view of the registry filled by game_save_register_fragment. ---- */
int  game_save_fragment_count(void);                             /* число зарегистрированных */
const GameSaveFragment *game_save_fragment_at(int index);        /* NULL если вне диапазона */
const GameSaveFragment *game_save_find_fragment(const char *id); /* NULL если ключ неизвестен */

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

/* ---- Статус и результат загрузки (§4, §14 п.10, Р7, Р10) ---- */
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

/* Грузит автослот (GAME_SAVE_AUTOSAVE_SLOT). Оркестрация §6/§14; никогда не
   all-or-nothing (плохой фрагмент не роняет остальные). Заполняет *result. */
void game_save_load(game_save_load_result_t *result);

/* Явная новая игра: reset всех -> on_new_game всех -> save -> возобновляет автосейв (Р10). */
bool game_save_new_game(char *error, int error_cap);

/* Синхронный форс-сейв в обход дебаунса (visibility-flush, §14 п.5). */
bool game_save_flush(char *error, int error_cap);

/* Кадровый тик: сейв при (now-dirty_at >= DEBOUNCE) || (now-last_save >= MAX_INTERVAL).
   No-op пока не dirty или автосейв на паузе (CORRUPT_RESET/NEWER до new_game). */
void game_save_tick(void);

/* Пометка «есть несохранённое». dirty_at = момент ПЕРВОЙ пометки после чистого
   (§14 п.6). Зовут мутаторы фич/UI. */
void game_save_mark_dirty(void);

/* Wall-clock ms сохранённого saved_at (оффлайн-Δt для идла; кламп отрицательных —
   задача игры). 0 если ещё не грузили/сохраняли. */
int64_t game_save_last_saved_at(void);

/* Персистентность недоступна (web квота/private, §14 п.3): игра может показать тост. */
bool game_save_is_unpersisted(void);

/* Экспорт/импорт строкой (Р12, §14 п.4) — тем же transform-путём; валидация конверта. */
char *game_save_export_string(char *error, int error_cap);   /* malloc, free вызывающим */
bool  game_save_import_string(const char *text, char *error, int error_cap);

/* Web: ставит visibilitychange(->hidden) + pagehide -> game_save_flush (НЕ
   beforeunload, §4). No-op на native. */
void game_save_install_web_flush(void);

/* ---- Transform-шов (§14 п.15). Дефолт пуст -> сейв = читаемый JSON ('{'). ---- */
typedef struct {
    const char *id;
    char *(*encode)(const char *in, char *error, int error_cap);  /* malloc out; save-порядок */
    char *(*decode)(const char *in, char *error, int error_cap);  /* malloc out; load-реверс */
} game_save_transform_t;

/* Устанавливает упорядоченную цепочку (save: по порядку; load: реверс). Пустая по
   умолчанию. Автодетект на load: префикс "NTSV1:<ids>:" -> трансформы; '{' -> плоский JSON. */
void game_save_set_transforms(const game_save_transform_t *chain, int count);

#ifdef GAME_SAVE_TESTING
/* Тест-шов (§A3.6): инжектит ОБА часа — монотонные (debounce/MAX_INTERVAL) и
   wall-clock (saved_at). Объявлено только под GAME_SAVE_TESTING. */
void game_save__set_clocks_for_test(int64_t (*mono)(void), int64_t (*wall)(void));
#endif

#endif /* GAME_SAVE_H */
