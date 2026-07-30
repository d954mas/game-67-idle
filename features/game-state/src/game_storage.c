#include "game_storage.h"

#include "game_storage_backend.h"

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

bool game_storage_write(
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
    return game_storage_backend_write(slot, text, error, error_cap);
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
