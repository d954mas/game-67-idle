#include "game_storage_backend.h"

#include "game_storage_web.h"
#include "log/nt_log.h"

#include <stdio.h>
#include <string.h>

#ifndef GAME_STORAGE_APP_ID
#error "GAME_STORAGE_APP_ID must be defined via CMake"
#endif

#define GAME_STORAGE_KEY_MAX 256

static void set_error(char *error, int error_cap, const char *message) {
    if (error != NULL && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
}

static bool is_safe_segment(const char *value) {
    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (const char *p = value; *p != '\0'; ++p) {
        const char c = *p;
        const bool safe =
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == '_' || c == '-';
        if (!safe) {
            return false;
        }
    }
    return true;
}

static bool resolve_key(
    const char *slot, char *out, size_t out_cap,
    char *error, int error_cap) {
    if (!is_safe_segment(slot)) {
        set_error(error, error_cap, "storage slot must be a simple name");
        return false;
    }
    if (snprintf(out, out_cap, "%s/save/%s", GAME_STORAGE_APP_ID, slot) >=
        (int)out_cap) {
        set_error(error, error_cap, "resolved storage key is too long");
        return false;
    }
    return true;
}

/* No filesystem on web: the slot IS a localStorage key. Accepted and ignored so
   the launch flag means the same thing on every platform -- nothing. */
void game_storage_set_root(const char *absolute_path) { (void)absolute_path; }

bool game_storage_backend_write(
    const char *slot, const char *text, char *error, int error_cap, bool may_wait) {
    /* Ignored here, deliberately. localStorage is synchronous and the failures
       it reports -- quota exhausted, private mode -- do not clear by waiting,
       so there is nothing to wait FOR. The browser main thread could not block
       for it anyway. See game_storage_write_blocking in game_storage.h. */
    (void)may_wait;
    char key[GAME_STORAGE_KEY_MAX];
    if (!resolve_key(slot, key, sizeof key, error, error_cap)) {
        return false;
    }
    if (strlen(text) > (size_t)GAME_STORAGE_MAX_BYTES) {
        set_error(error, error_cap, "storage text is too large");
        return false;
    }
    if (!game_storage_web_save(key, text)) {
        set_error(error, error_cap, "failed to write browser storage");
        return false;
    }
    return true;
}

bool game_storage_backend_read(
    const char *slot, char **out, game_storage_read_status_t *status,
    char *error, int error_cap) {
    char key[GAME_STORAGE_KEY_MAX];
    if (!resolve_key(slot, key, sizeof key, error, error_cap)) {
        return false;
    }
    int web_status = GAME_STORAGE_READ_ERROR;
    char *data = game_storage_web_load(
        key, (size_t)GAME_STORAGE_MAX_BYTES, &web_status);
    if (data != NULL) {
        *out = data;
        if (status != NULL) {
            *status = GAME_STORAGE_READ_OK;
        }
        return true;
    }
    if (web_status == GAME_STORAGE_READ_ABSENT) {
        if (status != NULL) {
            *status = GAME_STORAGE_READ_ABSENT;
        }
        set_error(error, error_cap, "browser storage has no such slot");
        return false;
    }
    const game_storage_read_status_t read_status =
        web_status == GAME_STORAGE_READ_ERROR
            ? GAME_STORAGE_READ_ERROR
            : GAME_STORAGE_READ_ERROR_PRESERVED;
    if (status != NULL) {
        *status = read_status;
    }
    if (read_status == GAME_STORAGE_READ_ERROR) {
        set_error(error, error_cap, "failed to read browser storage; quarantine copy verified");
        nt_log_warn(
            "game_storage: read slot '%s' failed; quarantine copy verified (web)",
            slot);
    } else {
        set_error(error, error_cap, "failed to read browser storage; primary preserved without quarantine");
        nt_log_warn(
            "game_storage: read slot '%s' failed; primary preserved, quarantine unavailable (web)",
            slot);
    }
    return false;
}

bool game_storage_backend_exists(const char *slot) {
    char key[GAME_STORAGE_KEY_MAX];
    return resolve_key(slot, key, sizeof key, NULL, 0) &&
           game_storage_web_key_exists(key) != 0;
}

bool game_storage_backend_write_backup(
    const char *slot, char *error, int error_cap) {
    char key[GAME_STORAGE_KEY_MAX];
    return resolve_key(slot, key, sizeof key, error, error_cap);
}

bool game_storage_backend_read_backup(
    const char *slot, char **out, char *error, int error_cap) {
    (void)slot;
    (void)out;
    set_error(error, error_cap, "web has no backup slot");
    return false;
}

bool game_storage_backend_quarantine(
    const char *slot, char *error, int error_cap) {
    char key[GAME_STORAGE_KEY_MAX];
    if (!resolve_key(slot, key, sizeof key, error, error_cap)) {
        return false;
    }
    if (!game_storage_web_quarantine(key)) {
        set_error(error, error_cap, "failed to create verified quarantine copy");
        return false;
    }
    return true;
}

bool game_storage_backend_probe(char *error, int error_cap) {
    char key[GAME_STORAGE_KEY_MAX];
    if (snprintf(key, sizeof key, "%s/__probe", GAME_STORAGE_APP_ID) >=
        (int)sizeof key) {
        set_error(error, error_cap, "resolved probe key is too long");
        return false;
    }
    if (!game_storage_web_probe(key)) {
        set_error(error, error_cap, "browser storage is not persistent");
        return false;
    }
    return true;
}
