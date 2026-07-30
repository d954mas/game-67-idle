#include "game_storage_backend.h"

#include "log/nt_log.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#define GAME_STORAGE_PATH_MAX 512
#define GAME_STORAGE_MAX_BYTES (1024 * 1024)
#define GAME_STORAGE_QUARANTINE_MAX_ATTEMPTS 1000

typedef struct game_storage_native_paths_t {
    char primary[GAME_STORAGE_PATH_MAX];
    char primary_tmp[GAME_STORAGE_PATH_MAX];
    char backup[GAME_STORAGE_PATH_MAX];
    char backup_tmp[GAME_STORAGE_PATH_MAX];
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
            set_error(error, error_cap, "failed to create storage directory");
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
    if (size < 0 || size > GAME_STORAGE_MAX_BYTES) {
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

static int64_t unix_ms_now(void) {
#ifdef _WIN32
    FILETIME file_time;
    GetSystemTimeAsFileTime(&file_time);
    ULARGE_INTEGER ticks;
    ticks.LowPart = file_time.dwLowDateTime;
    ticks.HighPart = file_time.dwHighDateTime;
    return (int64_t)((ticks.QuadPart - 116444736000000000ULL) / 10000ULL);
#else
    return (int64_t)time(NULL) * 1000;
#endif
}

static bool resolve_native_paths(
    const char *slot, game_storage_native_paths_t *paths,
    char *error, int error_cap) {
    if (!is_safe_segment(slot)) {
        set_error(error, error_cap, "storage slot must be a simple name");
        return false;
    }
    if (snprintf(paths->primary, sizeof paths->primary,
                 "build/saves/%s.json", slot) >= (int)sizeof paths->primary ||
        snprintf(paths->primary_tmp, sizeof paths->primary_tmp,
                 "%s.tmp", paths->primary) >= (int)sizeof paths->primary_tmp ||
        snprintf(paths->backup, sizeof paths->backup,
                 "build/saves/%s.bak", slot) >= (int)sizeof paths->backup ||
        snprintf(paths->backup_tmp, sizeof paths->backup_tmp,
                 "%s.tmp", paths->backup) >= (int)sizeof paths->backup_tmp) {
        set_error(error, error_cap, "resolved storage path is too long");
        return false;
    }
    return true;
}

static bool resolve_quarantine_path(
    const char *slot, char *out, size_t out_cap,
    char *error, int error_cap) {
    const int64_t timestamp = unix_ms_now();
    for (int attempt = 0;
         attempt < GAME_STORAGE_QUARANTINE_MAX_ATTEMPTS;
         ++attempt) {
        const int written =
            attempt == 0
                ? snprintf(
                      out, out_cap, "build/saves/%s.corrupt-%lld",
                      slot, (long long)timestamp)
                : snprintf(
                      out, out_cap, "build/saves/%s.corrupt-%lld-%d",
                      slot, (long long)timestamp, attempt);
        if (written < 0 || written >= (int)out_cap) {
            set_error(error, error_cap, "resolved quarantine path is too long");
            return false;
        }
        if (!path_exists(out)) {
            return true;
        }
    }
    set_error(
        error, error_cap, "too many quarantine collisions for this slot");
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
            slot, quarantine_path, sizeof quarantine_path, NULL, 0)) {
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
    const bool destination_stream_ok = ferror(destination) == 0;
    const bool destination_closed = fclose(destination) == 0;
    const bool destination_ok =
        destination_stream_ok && destination_closed;
    if (failed || !source_ok || !destination_ok) {
        (void)remove(quarantine_path);
        return false;
    }
    return true;
}

bool game_storage_backend_write(
    const char *slot, const char *text, char *error, int error_cap) {
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
            slot, quarantine_path, sizeof quarantine_path,
            error, error_cap)) {
        return false;
    }
    if (rename(paths.primary, quarantine_path) != 0) {
        set_error(error, error_cap, "failed to quarantine storage file");
        return false;
    }
    return true;
}

bool game_storage_backend_probe(char *error, int error_cap) {
    (void)error;
    (void)error_cap;
    return true;
}
