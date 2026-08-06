#ifndef GAME_STORAGE_H
#define GAME_STORAGE_H

#include <stdbool.h>
#include <stddef.h>

/* A save slot is a bounded text document on every platform.  Keep this
   public so callers and both adapters share one limit instead of relying on
   backend-specific quota or allocation behavior. */
#ifndef GAME_STORAGE_MAX_BYTES
#define GAME_STORAGE_MAX_BYTES (1024U * 1024U)
#endif

/* L0 байтовый бэкенд слота: native atomic-файл / web localStorage за одной
   сигнатурой. slot = логическое имя ([a-z0-9_-]+, проверяется). Значения —
   NUL-терминированный ТЕКСТ (бинарь трансформов приходит уже base64).
   APP_ID = compile define GAME_STORAGE_APP_ID (неймспейс общего web-origin itch).
   Один тред. */

/* Durable-атомарная запись слота.
   native: <OS-data>/neotolis/<APP_ID>/saves/<slot>.json.tmp -> synced replace;
           GAME_STORAGE_ROOT может задать абсолютный конечный каталог для теста.
           temp-файл и POSIX parent directory синхронизируются до success.
   web:    localStorage["<APP_ID>/save/<slot>"] = text в try/catch;
           false = квота/Safari-private (наверх как SAVE_UNPERSISTED).

   `error` называет причину в форме «сообщение (канал значение)» на обеих
   платформах: для web это единственное, что отличает QuotaExceededError
   (место кончилось) от SecurityError (хранилище закрыто на всю сессию). */
bool game_storage_write(const char *slot, const char *text, char *error, int error_cap);

/* То же самое, но вызывающему МОЖНО ждать.

   Разделение по вызывающему, не по слоту: загрузка
   синхронная, запись асинхронная — с бэкапом и ретраями. На Windows чужой
   процесс, держащий путь (антивирус, открывший временный файл сразу после
   нашего close), заставляет переименование ОТКАЗАТЬ, а не подождать; окно
   рассасывается за миллисекунды.

   `game_storage_write` НИКОГДА не ждёт: её зовёт game_save_tick изнутри кадра,
   а кадры терять нельзя. Отказ там — не потеря: состояние остаётся грязным и
   тик повторит через дебаунс.

   Эту зовут синхронные вызывающие, у которых кадра нет: загрузка, восстановление
   из .bak, карантин, явный flush. Они ждут до ~0.5 c — на старте это невидимо, а
   альтернатива в том, что игрок доигрывает сессию с неперезаписанным битым
   primary.

   Признак «кадра нет» — свойство ВЫЗЫВАЮЩЕГО, а не операции. Сброс прогресса
   умеет и то и другое: прямой game_save_new_game (старт на битом сейве,
   dev-эндпойнт) ждёт, а game_save_apply_pending_new_game — нет, потому что шелл
   зовёт его дважды изнутри кадра. Не добавляй сюда вызывающего, не посмотрев,
   откуда он на самом деле зовётся: ровно на этом месте стояло утверждение, что
   в релизной нативной сборке ожидание в кадр не попадает вообще, и оно было
   ложным. */
bool game_storage_write_blocking(const char *slot, const char *text, char *error, int error_cap);

/* Исход чтения: ABSENT («сейва ещё нет» → FRESH) отделён от ERROR («сейв есть,
   но прочитать не удалось»). ERROR НЕ должен молча затираться дефолтом: слой
   storage сначала кладёт сырьё в карантин (.corrupt-конвенция), а вызывающий
   трактует это как порчу, а не как отсутствие (data-loss баг malloc-load). */
typedef enum {
    GAME_STORAGE_READ_OK = 0,      /* *out держит байты (return true) */
    GAME_STORAGE_READ_ABSENT = 1,  /* слота нет (return false) */
    GAME_STORAGE_READ_ERROR = 2,   /* чтение упало; проверенная quarantine-копия существует (return false) */
    GAME_STORAGE_READ_ERROR_PRESERVED = 3, /* чтение упало; primary сохранён, но quarantine-копии нет (return false) */
} game_storage_read_status_t;

/* Чтение primary. *out — malloc'нутая NUL-строка (владелец вызывающий, free()).
   status (nullable) различает ABSENT, ERROR и ERROR_PRESERVED (см. enum выше).
   ERROR означает, что сырьё проверенно скопировано в стабильный карантин ТОЙ ЖЕ
   .corrupt-конвенции (native: файл <slot>.corrupt; web: ключ "<key>.corrupt").
   ERROR_PRESERVED означает, что primary не изменён,
   но безопасную quarantine-копию создать не удалось; вызывающий не должен писать
   в слот до явного решения пользователя.
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
   native: rename primary в bounded set <slot>.corrupt[-N] (максимум 3);
   web: replace единственной bounded-копии "<key>.corrupt". */
bool game_storage_quarantine(const char *slot, char *error, int error_cap);

/* Стартовый пробник персистентности.
   native: всегда true. web: setItem+getItem+removeItem пробного ключа ->
   false если браузер не сохраняет. */
bool game_storage_probe(char *error, int error_cap);

/* Куда класть сейвы. Задаётся ФЛАГОМ запуска (`--save-root <path>`), а не
   переменной окружения: у игры уже есть флаги, и невидимый канал через
   окружение — не то место, где должен решаться путь к данным игрока.

   Абсолютный путь; NULL/пустая строка снимают override. Зовётся до первого
   чтения или записи (main.c разбирает аргументы до game_save_load).

   Приоритет: этот вызов -> GAME_STORAGE_NATIVE_ROOT (компайл-тайм, только у
   тестовых целей) -> GAME_STORAGE_ROOT (окружение, для инструментов) ->
   штатное место ОС. web игнорирует: там ключи localStorage. */
void game_storage_set_root(const char *absolute_path);

#endif /* GAME_STORAGE_H */
