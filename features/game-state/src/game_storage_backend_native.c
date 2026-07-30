#include "game_storage_backend.h"

#include "log/nt_log.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define GAME_STORAGE_PATH_MAX 512
#define GAME_STORAGE_QUARANTINE_MAX_FILES 3

#ifndef GAME_STORAGE_APP_ID
#error "GAME_STORAGE_APP_ID must be defined via CMake"
#endif

typedef struct game_storage_native_paths_t {
    char directory[GAME_STORAGE_PATH_MAX];
    char primary[GAME_STORAGE_PATH_MAX];
    char primary_tmp[GAME_STORAGE_PATH_MAX];
    char backup[GAME_STORAGE_PATH_MAX];
    char backup_tmp[GAME_STORAGE_PATH_MAX];
    char quarantine_base[GAME_STORAGE_PATH_MAX];
} game_storage_native_paths_t;

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

static bool is_absolute_path(const char *path) {
    if (path == NULL || path[0] == '\0') {
        return false;
    }
#ifdef _WIN32
    if (path[0] == '\\' && path[1] == '\\') {
        return true;
    }
    const bool has_drive =
        (path[0] >= 'A' && path[0] <= 'Z') ||
        (path[0] >= 'a' && path[0] <= 'z');
    return has_drive && path[1] == ':' &&
           (path[2] == '/' || path[2] == '\\');
#else
    return path[0] == '/';
#endif
}

static bool resolve_storage_root(
    char *out, size_t out_cap, char *error, int error_cap) {
#ifdef GAME_STORAGE_NATIVE_ROOT
    const char *override_root = GAME_STORAGE_NATIVE_ROOT;
#else
    const char *override_root = getenv("GAME_STORAGE_ROOT");
#endif
    if (override_root != NULL && is_absolute_path(override_root)) {
        if (snprintf(out, out_cap, "%s", override_root) >= (int)out_cap) {
            set_error(error, error_cap, "native storage root is too long");
            return false;
        }
        return true;
    }

    const char *root = NULL;
#ifdef _WIN32
    root = getenv("LOCALAPPDATA");
    if (root == NULL || !is_absolute_path(root)) {
        static char temp_path[GAME_STORAGE_PATH_MAX];
        const DWORD length = GetTempPathA((DWORD)sizeof temp_path, temp_path);
        if (length == 0 || length >= sizeof temp_path) {
            set_error(error, error_cap, "failed to resolve native storage root");
            return false;
        }
        root = temp_path;
    }
#else
    root = getenv("XDG_STATE_HOME");
    char home_state[GAME_STORAGE_PATH_MAX];
    if (root == NULL || !is_absolute_path(root)) {
        const char *home = getenv("HOME");
        if (home != NULL && is_absolute_path(home)) {
            if (snprintf(home_state, sizeof home_state, "%s/.local/state", home) >=
                (int)sizeof home_state) {
                set_error(error, error_cap, "native storage root is too long");
                return false;
            }
            root = home_state;
        } else {
            root = "/tmp";
        }
    }
#endif
    if (snprintf(out, out_cap, "%s/neotolis/%s/saves", root,
                 GAME_STORAGE_APP_ID) >= (int)out_cap) {
        set_error(error, error_cap, "native storage root is too long");
        return false;
    }
    return true;
}

static bool make_dir_if_needed(const char *path) {
#ifdef _WIN32
    if (_mkdir(path) == 0) {
        return true;
    }
#else
    if (mkdir(path, 0755) == 0) {
        return true;
    }
#endif
    return errno == EEXIST;
}

static bool ensure_parent_dirs(
    const char *path, char *error, int error_cap) {
    char temp[GAME_STORAGE_PATH_MAX];
    if (path == NULL ||
        snprintf(temp, sizeof temp, "%s", path) >= (int)sizeof temp) {
        set_error(error, error_cap, "storage path is too long");
        return false;
    }
    for (char *p = temp; *p != '\0'; ++p) {
        if (*p != '/' && *p != '\\') {
            continue;
        }
        if (p == temp || *(p - 1) == ':') {
            continue;
        }
        const char saved = *p;
        *p = '\0';
        if (!make_dir_if_needed(temp)) {
            if (error != NULL && error_cap > 0) {
                (void)snprintf(
                    error, (size_t)error_cap,
                    "failed to create storage directory: %s", temp);
            }
            return false;
        }
        *p = saved;
    }
    return true;
}

static bool replace_file(const char *temporary, const char *primary) {
#ifdef _WIN32
    return MoveFileExA(
               temporary, primary,
               MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(temporary, primary) == 0;
#endif
}

static bool sync_parent_directory(const char *path) {
#ifdef _WIN32
    (void)path;
    /* MoveFileEx(...WRITE_THROUGH) is the durable replacement primitive. */
    return true;
#else
    char directory[GAME_STORAGE_PATH_MAX];
    if (snprintf(directory, sizeof directory, "%s", path) >=
        (int)sizeof directory) {
        return false;
    }
    char *separator = strrchr(directory, '/');
    if (separator == NULL || separator == directory) {
        return false;
    }
    *separator = '\0';
    const int descriptor = open(directory, O_RDONLY);
    if (descriptor < 0) {
        return false;
    }
    const int result = fsync(descriptor);
    (void)close(descriptor);
    return result == 0;
#endif
}

static bool flush_file_to_disk(FILE *file) {
    if (fflush(file) != 0) {
        return false;
    }
#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return fsync(fileno(file)) == 0;
#endif
}

static bool path_exists(const char *path) {
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat info;
    return stat(path, &info) == 0;
#endif
}

static bool readable_regular_file_exists(const char *path) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path);
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return false;
    }
#else
    struct stat info;
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return false;
    }
#endif
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    (void)fclose(file);
    return true;
}

static bool write_file_atomic(
    const char *temporary, const char *primary, const char *text,
    char *error, int error_cap) {
    if (!ensure_parent_dirs(primary, error, error_cap)) {
        return false;
    }
    FILE *file = fopen(temporary, "wb");
    if (file == NULL) {
        set_error(error, error_cap, "failed to open storage temp file for write");
        return false;
    }
    const size_t length = strlen(text);
    bool ok = fwrite(text, 1, length, file) == length;
    if (ok) {
        ok = flush_file_to_disk(file);
    }
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        (void)remove(temporary);
        set_error(error, error_cap, "failed to write storage temp file");
        return false;
    }
    if (!replace_file(temporary, primary)) {
        (void)remove(temporary);
        set_error(error, error_cap, "failed to replace storage file");
        return false;
    }
    if (!sync_parent_directory(primary)) {
        set_error(error, error_cap, "failed to sync storage directory");
        return false;
    }
    return true;
}

static bool read_file_bytes(
    const char *path, char **out, game_storage_read_status_t *status,
    char *error, int error_cap) {
    if (status != NULL) {
        *status = GAME_STORAGE_READ_ERROR;
    }
    errno = 0;
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        if (errno == ENOENT) {
            if (status != NULL) {
                *status = GAME_STORAGE_READ_ABSENT;
            }
            set_error(error, error_cap, "no storage file for slot");
        } else {
            set_error(error, error_cap, "failed to open storage file for read");
        }
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        (void)fclose(file);
        set_error(error, error_cap, "failed to seek storage file");
        return false;
    }
    const long size = ftell(file);
    if (size < 0 || (unsigned long)size > (unsigned long)GAME_STORAGE_MAX_BYTES) {
        (void)fclose(file);
        set_error(error, error_cap, "storage file is too large");
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        (void)fclose(file);
        set_error(error, error_cap, "failed to rewind storage file");
        return false;
    }
    char *data = malloc((size_t)size + 1U);
    if (data == NULL) {
        (void)fclose(file);
        set_error(error, error_cap, "failed to allocate storage buffer");
        return false;
    }
    const size_t bytes_read = fread(data, 1, (size_t)size, file);
    (void)fclose(file);
    if (bytes_read != (size_t)size) {
        free(data);
        set_error(error, error_cap, "failed to read storage file");
        return false;
    }
    data[size] = '\0';
    *out = data;
    if (status != NULL) {
        *status = GAME_STORAGE_READ_OK;
    }
    return true;
}

static bool resolve_native_paths(
    const char *slot, game_storage_native_paths_t *paths,
    char *error, int error_cap) {
    if (!is_safe_segment(slot)) {
        set_error(error, error_cap, "storage slot must be a simple name");
        return false;
    }
    if (!is_safe_segment(GAME_STORAGE_APP_ID)) {
        set_error(error, error_cap, "GAME_STORAGE_APP_ID must be a simple name");
        return false;
    }
    char root[GAME_STORAGE_PATH_MAX];
    if (!resolve_storage_root(root, sizeof root, error, error_cap) ||
        snprintf(paths->directory, sizeof paths->directory, "%s", root) >=
            (int)sizeof paths->directory ||
        snprintf(paths->primary, sizeof paths->primary,
                 "%s/%s.json", paths->directory, slot) >=
            (int)sizeof paths->primary ||
        snprintf(paths->primary_tmp, sizeof paths->primary_tmp,
                 "%s.tmp", paths->primary) >= (int)sizeof paths->primary_tmp ||
        snprintf(paths->backup, sizeof paths->backup,
                 "%s/%s.bak", paths->directory, slot) >=
            (int)sizeof paths->backup ||
        snprintf(paths->backup_tmp, sizeof paths->backup_tmp,
                 "%s.tmp", paths->backup) >= (int)sizeof paths->backup_tmp ||
        snprintf(paths->quarantine_base, sizeof paths->quarantine_base,
                 "%s/%s.corrupt", paths->directory, slot) >=
            (int)sizeof paths->quarantine_base) {
        set_error(error, error_cap, "resolved storage path is too long");
        return false;
    }
    return true;
}

static bool resolve_quarantine_path(
    const game_storage_native_paths_t *paths, char *out, size_t out_cap,
    char *error, int error_cap) {
    for (int attempt = 0; attempt < GAME_STORAGE_QUARANTINE_MAX_FILES; ++attempt) {
        const int written =
            attempt == 0
                ? snprintf(
                      out, out_cap, "%s", paths->quarantine_base)
                : snprintf(
                      out, out_cap, "%s-%d", paths->quarantine_base, attempt);
        if (written < 0 || written >= (int)out_cap) {
            set_error(error, error_cap, "resolved quarantine path is too long");
            return false;
        }
        if (!path_exists(out)) {
            return true;
        }
    }
    set_error(
        error, error_cap, "quarantine retention limit reached for this slot");
    return false;
}

static bool quarantine_unreadable_copy(const char *slot) {
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, NULL, 0)) {
        return false;
    }
    FILE *source = fopen(paths.primary, "rb");
    if (source == NULL) {
        return false;
    }
    char quarantine_path[GAME_STORAGE_PATH_MAX];
    if (!resolve_quarantine_path(
            &paths, quarantine_path, sizeof quarantine_path, NULL, 0)) {
        (void)fclose(source);
        return false;
    }
    FILE *destination = fopen(quarantine_path, "wb");
    if (destination == NULL) {
        (void)fclose(source);
        return false;
    }
    char buffer[8192];
    bool failed = false;
    size_t count;
    while ((count = fread(buffer, 1, sizeof buffer, source)) > 0) {
        if (fwrite(buffer, 1, count, destination) != count) {
            failed = true;
            break;
        }
    }
    const bool source_ok = ferror(source) == 0;
    (void)fclose(source);
    const bool destination_stream_ok =
        ferror(destination) == 0 && flush_file_to_disk(destination);
    const bool destination_closed = fclose(destination) == 0;
    const bool destination_ok =
        destination_stream_ok && destination_closed;
    if (failed || !source_ok || !destination_ok) {
        (void)remove(quarantine_path);
        return false;
    }
    if (!sync_parent_directory(quarantine_path)) {
        (void)remove(quarantine_path);
        return false;
    }
    return true;
}

bool game_storage_backend_write(
    const char *slot, const char *text, char *error, int error_cap) {
    if (text == NULL || strlen(text) > (size_t)GAME_STORAGE_MAX_BYTES) {
        set_error(error, error_cap, "storage text is too large");
        return false;
    }
    game_storage_native_paths_t paths;
    return resolve_native_paths(slot, &paths, error, error_cap) &&
           write_file_atomic(
               paths.primary_tmp, paths.primary, text, error, error_cap);
}

bool game_storage_backend_read(
    const char *slot, char **out, game_storage_read_status_t *status,
    char *error, int error_cap) {
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    game_storage_read_status_t read_status = GAME_STORAGE_READ_ERROR;
    const bool ok = read_file_bytes(
        paths.primary, out, &read_status, error, error_cap);
    if (!ok && read_status == GAME_STORAGE_READ_ERROR) {
        const bool copied = quarantine_unreadable_copy(slot);
        read_status = copied
            ? GAME_STORAGE_READ_ERROR
            : GAME_STORAGE_READ_ERROR_PRESERVED;
        nt_log_warn(
            copied
                ? "game_storage: read slot '%s' failed (%s); quarantine copy verified"
                : "game_storage: read slot '%s' failed (%s); primary preserved, quarantine unavailable",
            slot, error != NULL ? error : "");
    }
    if (status != NULL) {
        *status = read_status;
    }
    return ok;
}

bool game_storage_backend_exists(const char *slot) {
    game_storage_native_paths_t paths;
    return resolve_native_paths(slot, &paths, NULL, 0) &&
           readable_regular_file_exists(paths.primary);
}

bool game_storage_backend_write_backup(
    const char *slot, char *error, int error_cap) {
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    if (!readable_regular_file_exists(paths.primary)) {
        return true;
    }
    char *data = NULL;
    if (!read_file_bytes(paths.primary, &data, NULL, error, error_cap)) {
        return false;
    }
    const bool ok = write_file_atomic(
        paths.backup_tmp, paths.backup, data, error, error_cap);
    free(data);
    return ok;
}

bool game_storage_backend_read_backup(
    const char *slot, char **out, char *error, int error_cap) {
    game_storage_native_paths_t paths;
    return resolve_native_paths(slot, &paths, error, error_cap) &&
           read_file_bytes(paths.backup, out, NULL, error, error_cap);
}

bool game_storage_backend_quarantine(
    const char *slot, char *error, int error_cap) {
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    if (!readable_regular_file_exists(paths.primary)) {
        set_error(error, error_cap, "no primary to quarantine");
        return false;
    }
    char quarantine_path[GAME_STORAGE_PATH_MAX];
    if (!resolve_quarantine_path(
            &paths, quarantine_path, sizeof quarantine_path,
            error, error_cap)) {
        return false;
    }
    if (rename(paths.primary, quarantine_path) != 0) {
        set_error(error, error_cap, "failed to quarantine storage file");
        return false;
    }
    if (!sync_parent_directory(paths.primary)) {
        set_error(error, error_cap, "failed to sync storage directory");
        return false;
    }
    return true;
}

bool game_storage_backend_probe(char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    return true;
}
