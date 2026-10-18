#include "game_storage.h"

#include "game_storage_backend.h"

#include "log/nt_log.h"

#include <stdio.h>
#include <string.h>

static void set_error(char *error, int error_cap, const char *message) {
    if (error != NULL && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
}

static bool slot_is_valid(const char *slot) {
    if (slot == NULL || slot[0] == '\0') {
        return false;
    }
    for (const char *p = slot; *p != '\0'; ++p) {
        const char c = *p;
        if (!((c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}

static bool validate_slot(const char *slot, char *error, int error_cap) {
    if (slot_is_valid(slot)) {
        return true;
    }
    set_error(error, error_cap, "storage slot must be a simple name");
    return false;
}

/* The two public writes differ ONLY in whether the caller may be made to wait;
   every validation rule is shared, so it lives here once. */
static bool validate_write(
    const char *slot, const char *text, char *error, int error_cap) {
    if (!validate_slot(slot, error, error_cap)) {
        return false;
    }
    if (text == NULL) {
        set_error(error, error_cap, "storage text is required");
        return false;
    }
    if (strlen(text) > (size_t)GAME_STORAGE_MAX_BYTES) {
        set_error(error, error_cap, "storage text is too large");
        return false;
    }
    return true;
}

bool game_storage_write(
    const char *slot, const char *text, char *error, int error_cap) {
    return validate_write(slot, text, error, error_cap) &&
           game_storage_backend_write(slot, text, error, error_cap, false);
}

bool game_storage_write_blocking(
    const char *slot, const char *text, char *error, int error_cap) {
    return validate_write(slot, text, error, error_cap) &&
           game_storage_backend_write(slot, text, error, error_cap, true);
}

bool game_storage_write_save(
    const char *slot, const char *text, bool may_wait, char *error, int error_cap) {
    if (!validate_write(slot, text, error, error_cap)) {
        return false;
    }
    char reason[192] = {0};
    bool ok = game_storage_backend_write(slot, text, reason, (int)sizeof reason, may_wait);
    static const game_storage_premigration_t room_order[] = {
        GAME_STORAGE_PREMIGRATION_DROPPABLE,
        GAME_STORAGE_PREMIGRATION_KEPT_LATEST,
        GAME_STORAGE_PREMIGRATION_KEPT,
    };
    for (size_t i = 0; !ok && i < sizeof room_order / sizeof room_order[0]; i++) {
        if (strstr(reason, "QuotaExceededError") == NULL) {
            break;
        }
        const game_storage_premigration_t kind = room_order[i];
        if (!game_storage_backend_premigration_exists(slot, kind)) {
            continue;
        }
        char remove_error[128] = {0};
        if (!game_storage_backend_remove_premigration(slot, kind, remove_error, (int)sizeof remove_error)) {
            nt_log_warn("game_storage: storage is full and copy '%s%s' could not be removed (%s)",
                        slot, game_storage_premigration_suffix(kind), remove_error);
            break;
        }
        nt_log_warn("game_storage: storage is full; removed copy '%s%s' to save progress",
                    slot, game_storage_premigration_suffix(kind));
        reason[0] = '\0';
        ok = game_storage_backend_write(slot, text, reason, (int)sizeof reason, may_wait);
    }
    if (!ok) {
        set_error(error, error_cap, reason);
    }
    return ok;
}

bool game_storage_read(
    const char *slot, char **out, game_storage_read_status_t *status,
    char *error, int error_cap) {
    if (status != NULL) {
        *status = GAME_STORAGE_READ_ERROR;
    }
    if (out == NULL) {
        set_error(error, error_cap, "storage output pointer is required");
        return false;
    }
    *out = NULL;
    if (!validate_slot(slot, error, error_cap)) {
        return false;
    }
    return game_storage_backend_read(slot, out, status, error, error_cap);
}

bool game_storage_exists(const char *slot) {
    return slot_is_valid(slot) && game_storage_backend_exists(slot);
}

bool game_storage_write_backup(
    const char *slot, char *error, int error_cap) {
    if (!validate_slot(slot, error, error_cap)) {
        return false;
    }
    return game_storage_backend_write_backup(slot, error, error_cap);
}

bool game_storage_read_backup(
    const char *slot, char **out, char *error, int error_cap) {
    if (out == NULL) {
        set_error(error, error_cap, "storage output pointer is required");
        return false;
    }
    *out = NULL;
    if (!validate_slot(slot, error, error_cap)) {
        return false;
    }
    return game_storage_backend_read_backup(slot, out, error, error_cap);
}

bool game_storage_write_premigration(
    const char *slot, game_storage_premigration_t kind, const char *text, char *error, int error_cap) {
    return validate_write(slot, text, error, error_cap) &&
           game_storage_backend_write_premigration(slot, kind, text, error, error_cap);
}

bool game_storage_read_premigration(
    const char *slot, game_storage_premigration_t kind, char **out, char *error, int error_cap) {
    if (out == NULL) {
        set_error(error, error_cap, "storage output pointer is required");
        return false;
    }
    *out = NULL;
    if (!validate_slot(slot, error, error_cap)) {
        return false;
    }
    return game_storage_backend_read_premigration(slot, kind, out, error, error_cap);
}

bool game_storage_premigration_exists(const char *slot, game_storage_premigration_t kind) {
    return slot_is_valid(slot) && game_storage_backend_premigration_exists(slot, kind);
}

bool game_storage_remove_premigration(
    const char *slot, game_storage_premigration_t kind, char *error, int error_cap) {
    if (!validate_slot(slot, error, error_cap)) {
        return false;
    }
    return game_storage_backend_remove_premigration(slot, kind, error, error_cap);
}

bool game_storage_quarantine(
    const char *slot, char *error, int error_cap) {
    if (!validate_slot(slot, error, error_cap)) {
        return false;
    }
    return game_storage_backend_quarantine(slot, error, error_cap);
}

bool game_storage_probe(char *error, int error_cap) {
    return game_storage_backend_probe(error, error_cap);
}
