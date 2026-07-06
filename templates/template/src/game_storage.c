#include "game_storage.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif
#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#ifndef GAME_STORAGE_APP_ID
#error "GAME_STORAGE_APP_ID must be defined via CMake (see templates/template/CMakeLists.txt)"
#endif

#define GAME_STORAGE_PATH_MAX 512
#define GAME_STORAGE_KEY_MAX 256
#define GAME_STORAGE_MAX_BYTES (1024 * 1024)

static void set_error(char *error, int error_cap, const char *message) {
    if (error && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
}

static bool is_safe_segment(const char *value) {
    if (!value || !value[0]) {
        return false;
    }
    for (const char *p = value; *p; p++) {
        const char c = *p;
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

/* ---- web backend (localStorage). Ported from games/rb-dark-rpg/src/game_storage.c
   (EM_JS shape + try/catch), but the "rb-dark-rpg:" hardcoded prefix is gone: the
   caller resolves the FULL "<APP_ID>/save/<slot>" key on the C side (resolve_web_key
   below) and these EM_JS shims just use whatever key string they are handed. ---- */
#if defined(__EMSCRIPTEN__)
/* clang-format off */
EM_JS_DEPS(game_storage_web, "$UTF8ToString,$lengthBytesUTF8,$stringToUTF8,malloc")

EM_JS(int, game_storage_web_key_exists, (const char *key_ptr), {
    try {
        var key = UTF8ToString(key_ptr);
        return window.localStorage.getItem(key) !== null ? 1 : 0;
    } catch (e) {
        return 0;
    }
})

EM_JS(int, game_storage_web_save, (const char *key_ptr, const char *text_ptr), {
    try {
        var key = UTF8ToString(key_ptr);
        window.localStorage.setItem(key, UTF8ToString(text_ptr));
        return 1;
    } catch (e) {
        return 0;
    }
})

EM_JS(char *, game_storage_web_load, (const char *key_ptr), {
    try {
        var key = UTF8ToString(key_ptr);
        var data = window.localStorage.getItem(key);
        if (data === null) {
            return 0;
        }
        var size = lengthBytesUTF8(data) + 1;
        var ptr = malloc(size);
        if (!ptr) {
            return 0;
        }
        stringToUTF8(data, ptr, size);
        return ptr;
    } catch (e) {
        return 0;
    }
})

EM_JS(int, game_storage_web_delete, (const char *key_ptr), {
    try {
        var key = UTF8ToString(key_ptr);
        window.localStorage.removeItem(key);
        return 1;
    } catch (e) {
        return 0;
    }
})

/* New for A2 (§14 p.3): round-trips a throwaway key so a full browser/private-mode
   quota rejection is caught at startup instead of on the first real save. */
EM_JS(int, game_storage_web_probe, (const char *key_ptr), {
    try {
        var key = UTF8ToString(key_ptr);
        window.localStorage.setItem(key, "1");
        var ok = window.localStorage.getItem(key) === "1";
        window.localStorage.removeItem(key);
        return ok ? 1 : 0;
    } catch (e) {
        return 0;
    }
})
/* clang-format on */

static bool resolve_web_key(const char *slot, char *out, size_t out_cap, char *error, int error_cap) {
    if (!is_safe_segment(slot)) {
        set_error(error, error_cap, "storage slot must be a simple name");
        return false;
    }
    if (snprintf(out, out_cap, "%s/save/%s", GAME_STORAGE_APP_ID, slot) >= (int)out_cap) {
        set_error(error, error_cap, "resolved storage key is too long");
        return false;
    }
    return true;
}
#endif /* __EMSCRIPTEN__ */

/* ---- native backend (atomic file + .bak). Atomic recipe ported from the
   generated game_state_save (generate_state.py:1514-1566): temp-write then
   replace_file, never a direct "wb" open of primary. ---- */
#if !defined(__EMSCRIPTEN__)
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

static bool ensure_parent_dirs(const char *path, char *error, int error_cap) {
    char temp[GAME_STORAGE_PATH_MAX];
    if (!path || snprintf(temp, sizeof(temp), "%s", path) >= (int)sizeof(temp)) {
        set_error(error, error_cap, "storage path is too long");
        return false;
    }
    for (char *p = temp; *p; p++) {
        if (*p != '/' && *p != '\\') {
            continue;
        }
        if (p == temp || (*(p - 1) == ':')) {
            continue;
        }
        char saved = *p;
        *p = '\0';
        if (!make_dir_if_needed(temp)) {
            set_error(error, error_cap, "failed to create storage directory");
            return false;
        }
        *p = saved;
    }
    return true;
}

/* MoveFileEx(REPLACE_EXISTING|WRITE_THROUGH) never leaves primary absent or torn
   (design §14 p.1); plain rename() on POSIX is already atomic-replace by contract. */
static bool replace_file(const char *tmp_path, const char *path) {
#ifdef _WIN32
    return MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tmp_path, path) == 0;
#endif
}

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    fclose(file);
    return true;
}

static bool write_file_atomic(const char *tmp_path, const char *primary_path, const char *text, char *error, int error_cap) {
    if (!ensure_parent_dirs(primary_path, error, error_cap)) {
        return false;
    }
    FILE *file = fopen(tmp_path, "wb");
    if (!file) {
        set_error(error, error_cap, "failed to open storage temp file for write");
        return false;
    }
    const size_t len = strlen(text);
    bool ok = fwrite(text, 1, len, file) == len;
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        (void)remove(tmp_path);
        set_error(error, error_cap, "failed to write storage temp file");
        return false;
    }
    if (!replace_file(tmp_path, primary_path)) {
        (void)remove(tmp_path);
        set_error(error, error_cap, "failed to replace storage file");
        return false;
    }
    return true;
}

static bool read_file_bytes(const char *path, char **out, char *error, int error_cap) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        set_error(error, error_cap, "failed to open storage file for read");
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        set_error(error, error_cap, "failed to seek storage file");
        return false;
    }
    long size = ftell(file);
    if (size < 0 || size > GAME_STORAGE_MAX_BYTES) {
        fclose(file);
        set_error(error, error_cap, "storage file is too large");
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        set_error(error, error_cap, "failed to rewind storage file");
        return false;
    }
    char *data = (char *)malloc((size_t)size + 1U);
    if (!data) {
        fclose(file);
        set_error(error, error_cap, "failed to allocate storage buffer");
        return false;
    }
    const size_t read_size = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (read_size != (size_t)size) {
        free(data);
        set_error(error, error_cap, "failed to read storage file");
        return false;
    }
    data[size] = '\0';
    *out = data;
    return true;
}

/* Wall-clock ms for quarantine filenames. Windows path is real millisecond
   precision (FILETIME); the POSIX fallback is second-granular (no portable ms
   clock in plain C17) -- both produce a valid, merely coarser, unix_ms value. */
static int64_t storage_unix_ms_now(void) {
#ifdef _WIN32
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    ULARGE_INTEGER uli;
    uli.LowPart = ft.dwLowDateTime;
    uli.HighPart = ft.dwHighDateTime;
    const unsigned long long kEpochDiff100ns = 116444736000000000ULL; /* 1601 -> 1970 */
    const unsigned long long ticks = uli.QuadPart - kEpochDiff100ns;
    return (int64_t)(ticks / 10000ULL);
#else
    return (int64_t)time(NULL) * 1000;
#endif
}

/* Quarantine collision (deep-review finding): a bare rename() to a
   "<slot>.corrupt-<unix_ms>" name is not enough -- two quarantines of the same
   slot inside one clock tick (POSIX-second granularity, or even the same
   Windows millisecond under test) target the SAME path: POSIX rename()
   silently clobbers the first .corrupt (forensics lost, §14 p.14 defeated),
   Windows MoveFileEx/rename fails outright (EEXIST-equivalent). Resolve a
   name that does not yet exist, trying "-1", "-2", ... suffixes so both
   platforms behave identically (every quarantine keeps its own file). */
#define GAME_STORAGE_QUARANTINE_MAX_ATTEMPTS 1000

static bool resolve_quarantine_path(const char *slot, char *out, size_t out_cap, char *error, int error_cap) {
    const int64_t ts = storage_unix_ms_now();
    for (int attempt = 0; attempt < GAME_STORAGE_QUARANTINE_MAX_ATTEMPTS; attempt++) {
        const int written = (attempt == 0)
            ? snprintf(out, out_cap, "build/saves/%s.corrupt-%lld", slot, (long long)ts)
            : snprintf(out, out_cap, "build/saves/%s.corrupt-%lld-%d", slot, (long long)ts, attempt);
        if (written < 0 || written >= (int)out_cap) {
            set_error(error, error_cap, "resolved quarantine path is too long");
            return false;
        }
        if (!file_exists(out)) {
            return true;
        }
    }
    set_error(error, error_cap, "too many quarantine collisions for this slot");
    return false;
}

typedef struct {
    char primary[GAME_STORAGE_PATH_MAX];
    char primary_tmp[GAME_STORAGE_PATH_MAX];
    char bak[GAME_STORAGE_PATH_MAX];
    char bak_tmp[GAME_STORAGE_PATH_MAX];
} game_storage_native_paths_t;

static bool resolve_native_paths(const char *slot, game_storage_native_paths_t *paths, char *error, int error_cap) {
    if (!is_safe_segment(slot)) {
        set_error(error, error_cap, "storage slot must be a simple name");
        return false;
    }
    if (snprintf(paths->primary, sizeof(paths->primary), "build/saves/%s.json", slot) >= (int)sizeof(paths->primary) ||
        snprintf(paths->primary_tmp, sizeof(paths->primary_tmp), "%s.tmp", paths->primary) >= (int)sizeof(paths->primary_tmp) ||
        snprintf(paths->bak, sizeof(paths->bak), "build/saves/%s.bak", slot) >= (int)sizeof(paths->bak) ||
        snprintf(paths->bak_tmp, sizeof(paths->bak_tmp), "%s.tmp", paths->bak) >= (int)sizeof(paths->bak_tmp)) {
        set_error(error, error_cap, "resolved storage path is too long");
        return false;
    }
    return true;
}
#endif /* !__EMSCRIPTEN__ */

/* ---- public slot API (game_storage.h) ---- */

bool game_storage_write(const char *slot, const char *text, char *error, int error_cap) {
    if (!text) {
        set_error(error, error_cap, "storage text is required");
        return false;
    }
#if defined(__EMSCRIPTEN__)
    char key[GAME_STORAGE_KEY_MAX];
    if (!resolve_web_key(slot, key, sizeof(key), error, error_cap)) {
        return false;
    }
    if (!game_storage_web_save(key, text)) {
        set_error(error, error_cap, "failed to write browser storage");
        return false;
    }
    return true;
#else
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    return write_file_atomic(paths.primary_tmp, paths.primary, text, error, error_cap);
#endif
}

bool game_storage_read(const char *slot, char **out, char *error, int error_cap) {
    if (!out) {
        set_error(error, error_cap, "storage output pointer is required");
        return false;
    }
    *out = NULL;
#if defined(__EMSCRIPTEN__)
    char key[GAME_STORAGE_KEY_MAX];
    if (!resolve_web_key(slot, key, sizeof(key), error, error_cap)) {
        return false;
    }
    char *data = game_storage_web_load(key);
    if (!data) {
        set_error(error, error_cap, "failed to open browser storage for read");
        return false;
    }
    *out = data;
    return true;
#else
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    return read_file_bytes(paths.primary, out, error, error_cap);
#endif
}

bool game_storage_exists(const char *slot) {
#if defined(__EMSCRIPTEN__)
    char key[GAME_STORAGE_KEY_MAX];
    if (!resolve_web_key(slot, key, sizeof(key), NULL, 0)) {
        return false;
    }
    return game_storage_web_key_exists(key) != 0;
#else
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, NULL, 0)) {
        return false;
    }
    return file_exists(paths.primary);
#endif
}

bool game_storage_write_backup(const char *slot, char *error, int error_cap) {
#if defined(__EMSCRIPTEN__)
    (void)slot;
    (void)error;
    (void)error_cap;
    return true; /* web .bak is cut, design §14 p.3 */
#else
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    if (!file_exists(paths.primary)) {
        return true; /* nothing to back up */
    }
    char *data = NULL;
    if (!read_file_bytes(paths.primary, &data, error, error_cap)) {
        return false;
    }
    bool ok = write_file_atomic(paths.bak_tmp, paths.bak, data, error, error_cap);
    free(data);
    return ok;
#endif
}

bool game_storage_read_backup(const char *slot, char **out, char *error, int error_cap) {
    if (!out) {
        set_error(error, error_cap, "storage output pointer is required");
        return false;
    }
    *out = NULL;
#if defined(__EMSCRIPTEN__)
    (void)slot;
    set_error(error, error_cap, "web has no backup slot");
    return false;
#else
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    return read_file_bytes(paths.bak, out, error, error_cap);
#endif
}

bool game_storage_quarantine(const char *slot, char *error, int error_cap) {
#if defined(__EMSCRIPTEN__)
    char key[GAME_STORAGE_KEY_MAX];
    if (!resolve_web_key(slot, key, sizeof(key), error, error_cap)) {
        return false;
    }
    char *data = game_storage_web_load(key);
    if (!data) {
        set_error(error, error_cap, "no primary to quarantine");
        return false;
    }
    char corrupt_key[GAME_STORAGE_KEY_MAX];
    if (snprintf(corrupt_key, sizeof(corrupt_key), "%s.corrupt", key) >= (int)sizeof(corrupt_key)) {
        free(data);
        set_error(error, error_cap, "resolved quarantine key is too long");
        return false;
    }
    bool saved = game_storage_web_save(corrupt_key, data) != 0;
    free(data);
    if (!saved) {
        set_error(error, error_cap, "failed to write quarantine copy");
        return false;
    }
    if (!game_storage_web_delete(key)) {
        set_error(error, error_cap, "failed to clear quarantined primary");
        return false;
    }
    return true;
#else
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    if (!file_exists(paths.primary)) {
        set_error(error, error_cap, "no primary to quarantine");
        return false;
    }
    char corrupt_path[GAME_STORAGE_PATH_MAX];
    if (!resolve_quarantine_path(slot, corrupt_path, sizeof(corrupt_path), error, error_cap)) {
        return false;
    }
    if (rename(paths.primary, corrupt_path) != 0) {
        set_error(error, error_cap, "failed to quarantine storage file");
        return false;
    }
    return true;
#endif
}

bool game_storage_probe(char *error, int error_cap) {
#if defined(__EMSCRIPTEN__)
    char key[GAME_STORAGE_KEY_MAX];
    if (snprintf(key, sizeof(key), "%s/__probe", GAME_STORAGE_APP_ID) >= (int)sizeof(key)) {
        set_error(error, error_cap, "resolved probe key is too long");
        return false;
    }
    if (!game_storage_web_probe(key)) {
        set_error(error, error_cap, "browser storage is not persistent");
        return false;
    }
    return true;
#else
    (void)error;
    (void)error_cap;
    return true;
#endif
}

/* ---- compat wrappers until A5 (generated game_state_devapi.c: generate_state.py
   :310/317/343). slot = key+document folded together; behavioural round-trip only,
   not byte-path compatibility with the pre-A2 layout (dev-only, A5 rewrites). ---- */

static bool compat_fold_slot(const char *key, const char *document, char *slot, size_t slot_cap, char *error, int error_cap) {
    if (!is_safe_segment(key) || !is_safe_segment(document) || snprintf(slot, slot_cap, "%s_%s", key, document) >= (int)slot_cap) {
        set_error(error, error_cap, "storage key and document must be simple names");
        return false;
    }
    return true;
}

bool game_storage_resolve_key(const char *key, const char *document, char *out, int out_cap, char *error, int error_cap) {
    char slot[GAME_STORAGE_PATH_MAX];
    if (!compat_fold_slot(key, document, slot, sizeof(slot), error, error_cap)) {
        return false;
    }
    if (!out || out_cap <= 0) {
        set_error(error, error_cap, "resolved storage path buffer is required");
        return false;
    }
#if defined(__EMSCRIPTEN__)
    char web_key[GAME_STORAGE_KEY_MAX];
    if (!resolve_web_key(slot, web_key, sizeof(web_key), error, error_cap)) {
        return false;
    }
    if (snprintf(out, (size_t)out_cap, "%s", web_key) >= out_cap) {
        set_error(error, error_cap, "resolved storage path is too long");
        return false;
    }
    return true;
#else
    game_storage_native_paths_t paths;
    if (!resolve_native_paths(slot, &paths, error, error_cap)) {
        return false;
    }
    if (snprintf(out, (size_t)out_cap, "%s", paths.primary) >= out_cap) {
        set_error(error, error_cap, "resolved storage path is too long");
        return false;
    }
    return true;
#endif
}

bool game_storage_save_json(const char *key, const char *document, const char *json, char *error, int error_cap) {
    char slot[GAME_STORAGE_PATH_MAX];
    if (!compat_fold_slot(key, document, slot, sizeof(slot), error, error_cap)) {
        return false;
    }
    return game_storage_write(slot, json, error, error_cap);
}

bool game_storage_load_json(const char *key, const char *document, char **out_json, char *error, int error_cap) {
    char slot[GAME_STORAGE_PATH_MAX];
    if (!compat_fold_slot(key, document, slot, sizeof(slot), error, error_cap)) {
        return false;
    }
    return game_storage_read(slot, out_json, error, error_cap);
}
