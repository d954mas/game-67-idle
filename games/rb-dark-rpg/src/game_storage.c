#include "game_storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define GAME_STORAGE_PATH_MAX 512
#define GAME_STORAGE_MAX_BYTES (1024 * 1024)

#if defined(__EMSCRIPTEN__)
/* clang-format off */
EM_JS_DEPS(game_storage_web, "$UTF8ToString,$lengthBytesUTF8,$stringToUTF8,malloc")

EM_JS(int, game_storage_web_key_exists, (const char *path_ptr), {
    try {
        var key = "rb-dark-rpg:" + UTF8ToString(path_ptr);
        return window.localStorage.getItem(key) !== null ? 1 : 0;
    } catch (e) {
        return 0;
    }
})

EM_JS(int, game_storage_web_save, (const char *path_ptr, const char *json_ptr), {
    try {
        var key = "rb-dark-rpg:" + UTF8ToString(path_ptr);
        window.localStorage.setItem(key, UTF8ToString(json_ptr));
        return 1;
    } catch (e) {
        return 0;
    }
})

EM_JS(char *, game_storage_web_load, (const char *path_ptr), {
    try {
        var key = "rb-dark-rpg:" + UTF8ToString(path_ptr);
        var data = window.localStorage.getItem(key);
        if (data === null) {
            return 0;
        }
        var size = lengthBytesUTF8(data) + 1;
        /* Bare `malloc` is undefined in the EM_JS scope (the wasm export is
           `_malloc`): the throw made every load look like "no save", so the
           game wiped the real save with defaults on boot (T0333 finding). */
        var ptr = _malloc(size);
        if (!ptr) {
            return 0;
        }
        stringToUTF8(data, ptr, size);
        return ptr;
    } catch (e) {
        return 0;
    }
})

EM_JS(int, game_storage_web_delete, (const char *path_ptr), {
    try {
        var key = "rb-dark-rpg:" + UTF8ToString(path_ptr);
        window.localStorage.removeItem(key);
        return 1;
    } catch (e) {
        return 0;
    }
})
/* clang-format on */
#endif

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
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

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
#endif

bool game_storage_resolve_key(const char *key, const char *document, char *out, int out_cap, char *error, int error_cap) {
    if (!is_safe_segment(key) || !is_safe_segment(document)) {
        set_error(error, error_cap, "storage key and document must be simple names");
        return false;
    }
    if (!out || out_cap <= 0 || snprintf(out, (size_t)out_cap, "build/saves/%s/%s.json", key, document) >= out_cap) {
        set_error(error, error_cap, "resolved storage path is too long");
        return false;
    }
    return true;
}

bool game_storage_key_exists(const char *key, const char *document) {
    char path[GAME_STORAGE_PATH_MAX];
    char error[128];
    if (!game_storage_resolve_key(key, document, path, (int)sizeof(path), error, (int)sizeof(error))) {
        return false;
    }
#if defined(__EMSCRIPTEN__)
    return game_storage_web_key_exists(path) != 0;
#else
    FILE *file = fopen(path, "rb");
    if (!file) {
        return false;
    }
    fclose(file);
    return true;
#endif
}

bool game_storage_save_json(const char *key, const char *document, const char *json, char *error, int error_cap) {
    char path[GAME_STORAGE_PATH_MAX];
    if (!json || !game_storage_resolve_key(key, document, path, (int)sizeof(path), error, error_cap)) {
        return false;
    }
#if defined(__EMSCRIPTEN__)
    if (!game_storage_web_save(path, json)) {
        set_error(error, error_cap, "failed to write browser storage");
        return false;
    }
    return true;
#else
    if (!ensure_parent_dirs(path, error, error_cap)) {
        return false;
    }
    FILE *file = fopen(path, "wb");
    if (!file) {
        set_error(error, error_cap, "failed to open storage file for write");
        return false;
    }
    const size_t len = strlen(json);
    bool ok = fwrite(json, 1, len, file) == len;
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        set_error(error, error_cap, "failed to write storage file");
        return false;
    }
    return true;
#endif
}

bool game_storage_load_json(const char *key, const char *document, char **out_json, char *error, int error_cap) {
    char path[GAME_STORAGE_PATH_MAX];
    if (!out_json || !game_storage_resolve_key(key, document, path, (int)sizeof(path), error, error_cap)) {
        return false;
    }
    *out_json = NULL;
#if defined(__EMSCRIPTEN__)
    char *data = game_storage_web_load(path);
    if (!data) {
        set_error(error, error_cap, "failed to open browser storage for read");
        return false;
    }
    *out_json = data;
    return true;
#else
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
    const size_t read = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (read != (size_t)size) {
        free(data);
        set_error(error, error_cap, "failed to read storage file");
        return false;
    }
    data[size] = '\0';
    *out_json = data;
    return true;
#endif
}

bool game_storage_delete_json(const char *key, const char *document, char *error, int error_cap) {
    char path[GAME_STORAGE_PATH_MAX];
    if (!game_storage_resolve_key(key, document, path, (int)sizeof(path), error, error_cap)) {
        return false;
    }
#if defined(__EMSCRIPTEN__)
    if (!game_storage_web_delete(path)) {
        set_error(error, error_cap, "failed to delete browser storage");
        return false;
    }
    return true;
#else
    if (remove(path) == 0 || errno == ENOENT) {
        return true;
    }
    set_error(error, error_cap, "failed to delete storage file");
    return false;
#endif
}
