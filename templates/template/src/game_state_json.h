#ifndef GAME_STATE_JSON_H
#define GAME_STATE_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cJSON.h"

/* L0 JSON-тулкит (gsj_*). Лист-модуль: не зависит от game_save/game_storage/
   генерируемого GameState. Один тред. Владение: ридеры получают чужое cJSON-
   дерево как const и НЕ владеют им; строки копируются в буфер вызывающего. */

/* Пишет message в error (усечение безопасно). No-op если error==NULL || cap<=0. */
void gsj_set_error(char *error, int error_cap, const char *message);

/* snprintf-копия src→dst с проверкой ёмкости; false если NULL/не влезло. */
bool gsj_copy_text(char *dst, size_t dst_cap, const char *src);

/* CaseSensitive lookup; NULL если obj не объект или ключа нет. */
const cJSON *gsj_object_item(const cJSON *obj, const char *name);

/* Толерантные ридеры (контракт «absent = ok, дефолт остаётся»):
   ключа нет            -> true, *out не трогается;
   есть, но не тот тип  -> false + error;
   есть, вне диапазона  -> false + error;
   есть, валиден        -> true, *out записан. */
bool gsj_read_bool(const cJSON *obj, const char *name, bool *out,
                   char *error, int error_cap);
bool gsj_read_int_range(const cJSON *obj, const char *name,
                        int min_value, int max_value, int *out,
                        char *error, int error_cap);
bool gsj_read_float_range(const cJSON *obj, const char *name,
                          float min_value, float max_value, float *out,
                          char *error, int error_cap);
bool gsj_read_string(const cJSON *obj, const char *name, char *out,
                     size_t out_cap, char *error, int error_cap);

/* enum по таблице имён (совпадение по строке ИЛИ по целому индексу, legacy). */
int  gsj_enum_index(const char *value, const char *const *names, int count);
bool gsj_read_enum(const cJSON *obj, const char *name,
                   const char *const *names, int count, int *out,
                   char *error, int error_cap);

/* Парсеры одного узла (для элементов map/list). */
bool gsj_parse_int_value(const cJSON *item, int min_value, int max_value,
                         int *out, char *error, int error_cap);
bool gsj_parse_enum_value(const cJSON *item, const char *const *names,
                          int count, int *out, char *error, int error_cap);

/* i64-провод. Большие счётчики едут JSON-СТРОКОЙ (double рвётся >2^53).
   read: принимает строку ВСЕГДА; число — ТОЛЬКО если точно представимо в int64
   (целое и |v| <= 2^53). Absent = true, *out не трогается. */
bool gsj_read_i64(const cJSON *obj, const char *name,
                  int64_t min_value, int64_t max_value, int64_t *out,
                  char *error, int error_cap);
bool gsj_parse_i64_value(const cJSON *item, int64_t min_value, int64_t max_value,
                         int64_t *out, char *error, int error_cap);
/* Кладёт value как JSON-СТРОКУ в obj (узел во владении obj). NULL при OOM. */
cJSON *gsj_add_i64(cJSON *obj, const char *name, int64_t value);
/* value -> десятичная строка в buf (cap >= 21). Возвращает buf. */
char  *gsj_i64_to_string(int64_t value, char *buf, size_t cap);

#endif /* GAME_STATE_JSON_H */
