#ifndef GAME_STORAGE_H
#define GAME_STORAGE_H

#include <stdbool.h>

/* L0 байтовый бэкенд слота: native atomic-файл / web localStorage за одной
   сигнатурой. slot = логическое имя ([a-z0-9_-]+, проверяется). Значения —
   NUL-терминированный ТЕКСТ (бинарь трансформов приходит уже base64).
   APP_ID = compile define GAME_STORAGE_APP_ID (неймспейс общего web-origin itch).
   Один тред. */

/* Атомарная запись слота.
   native: build/saves/<slot>.json.tmp (WRITE_THROUGH) -> replace_file(tmp->primary);
           primary никогда не отсутствует и не рвётся.
   web:    localStorage["<APP_ID>/save/<slot>"] = text в try/catch;
           false = квота/Safari-private (наверх как SAVE_UNPERSISTED). */
bool game_storage_write(const char *slot, const char *text, char *error, int error_cap);

/* Исход чтения: ABSENT («сейва ещё нет» → FRESH) отделён от ERROR («сейв есть,
   но прочитать не удалось»). ERROR НЕ должен молча затираться дефолтом: слой
   storage сначала кладёт сырьё в карантин (.corrupt-конвенция), а вызывающий
   трактует это как порчу, а не как отсутствие (data-loss баг malloc-load). */
typedef enum {
    GAME_STORAGE_READ_OK = 0,      /* *out держит байты (return true) */
    GAME_STORAGE_READ_ABSENT = 1,  /* слота нет (return false) */
    GAME_STORAGE_READ_ERROR = 2,   /* слот есть, но чтение упало; оригинал скопирован в карантин (return false) */
} game_storage_read_status_t;

/* Чтение primary. *out — malloc'нутая NUL-строка (владелец вызывающий, free()).
   status (nullable) различает ABSENT и ERROR (см. enum выше). При ERROR сырьё
   уже best-effort скопировано в карантин ТОЙ ЖЕ .corrupt-конвенции (native: файл
   build/saves/<slot>.corrupt-<ts>; web: ключ "<key>.corrupt" прямо в JS).
   false: ABSENT или ERROR; true: OK. */
bool game_storage_read(const char *slot, char **out, game_storage_read_status_t *status, char *error, int error_cap);

/* true если primary слота существует и читаем. */
bool game_storage_exists(const char *slot);

/* Пишет .bak = копию заведомо-хорошего primary (один раз за сессию,
   после успешной загрузки last-known-good).
   native: primary -> <slot>.bak. web: no-op, true (web .bak вырезан). */
bool game_storage_write_backup(const char *slot, char *error, int error_cap);

/* Чтение .bak (фолбэк лоадера). native only; web всегда false. */
bool game_storage_read_backup(const char *slot, char **out, char *error, int error_cap);

/* Откладывает битый primary для форензики/ручной починки (Р10, восстанавливает
   .corrupt для ручной починки сейва).
   native: rename primary -> <slot>.corrupt-<unix_ms>. web: copy value -> "<key>.corrupt". */
bool game_storage_quarantine(const char *slot, char *error, int error_cap);

/* Стартовый пробник персистентности.
   native: всегда true. web: setItem+getItem+removeItem пробного ключа ->
   false если браузер не сохраняет. */
bool game_storage_probe(char *error, int error_cap);

#endif /* GAME_STORAGE_H */
