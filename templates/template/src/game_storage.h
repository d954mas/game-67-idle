#ifndef GAME_STORAGE_H
#define GAME_STORAGE_H

#include <stdbool.h>

/* L0 байтовый бэкенд слота: native atomic-файл / web localStorage за одной
   сигнатурой. slot = логическое имя ([a-z0-9_-]+, проверяется). Значения —
   NUL-терминированный ТЕКСТ (бинарь трансформов приходит уже base64, §14 п.15).
   APP_ID = compile define GAME_STORAGE_APP_ID (неймспейс общего web-origin itch).
   Один тред. */

/* Атомарная запись слота.
   native: build/saves/<slot>.json.tmp (WRITE_THROUGH) -> replace_file(tmp->primary);
           primary никогда не отсутствует и не рвётся (§14 п.1).
   web:    localStorage["<APP_ID>/save/<slot>"] = text в try/catch;
           false = квота/Safari-private (наверх как SAVE_UNPERSISTED). */
bool game_storage_write(const char *slot, const char *text, char *error, int error_cap);

/* Чтение primary. *out — malloc'нутая NUL-строка (владелец вызывающий, free()).
   false: слота нет / ошибка. */
bool game_storage_read(const char *slot, char **out, char *error, int error_cap);

/* true если primary слота существует и читаем. */
bool game_storage_exists(const char *slot);

/* Пишет .bak = копию заведомо-хорошего primary (§14 п.1: один раз за сессию,
   после успешной загрузки last-known-good).
   native: primary -> <slot>.bak. web: no-op, true (web .bak вырезан, §14 п.3). */
bool game_storage_write_backup(const char *slot, char *error, int error_cap);

/* Чтение .bak (фолбэк лоадера). native only; web всегда false. */
bool game_storage_read_backup(const char *slot, char **out, char *error, int error_cap);

/* Откладывает битый primary для форензики/ручной починки (Р10, восстанавливает
   .corrupt из §14 п.14 — читатель есть: лид правит сейвы руками).
   native: rename primary -> <slot>.corrupt-<unix_ms>. web: copy value -> "<key>.corrupt". */
bool game_storage_quarantine(const char *slot, char *error, int error_cap);

/* Стартовый пробник персистентности (§14 п.3).
   native: всегда true. web: setItem+getItem+removeItem пробного ключа ->
   false если браузер не сохраняет. */
bool game_storage_probe(char *error, int error_cap);

#endif /* GAME_STORAGE_H */
