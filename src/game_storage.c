#include "game_storage.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NT_PLATFORM_WEB
#include <emscripten/emscripten.h>
#elif defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <sys/stat.h>
#endif

#define GAME_STORAGE_PATH_MAX 512
#define GAME_STORAGE_MAX_FILE_BYTES (1024 * 1024)

static GameStorageConfig s_config = {
    .namespace_name = "game",
    .native_root = "build/saves",
};

static void set_error(char *error, int error_cap, const char *message) {
    if (error && error_cap > 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", message);
    }
}

void game_storage_init(const GameStorageConfig *config) {
    if (!config) {
        return;
    }
    if (config->namespace_name && config->namespace_name[0]) {
        s_config.namespace_name = config->namespace_name;
    }
    if (config->native_root && config->native_root[0]) {
        s_config.native_root = config->native_root;
    }
}

static bool is_safe_token_char(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') ||
           c == '_' || c == '-' || c == '.';
}

static bool token_is_valid(const char *value) {
    if (!value || !value[0]) {
        return false;
    }
    size_t len = strlen(value);
    if (len > 64 || strcmp(value, ".") == 0 || strcmp(value, "..") == 0) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        if (!is_safe_token_char(value[i])) {
            return false;
        }
    }
    return strstr(value, "..") == NULL;
}

bool game_storage_key_is_valid(const char *value) {
    return token_is_valid(value);
}

bool game_storage_doc_is_valid(const char *value) {
    return token_is_valid(value);
}

#ifdef NT_PLATFORM_WEB
EM_JS(char *, game_storage_web_load, (const char *key), {
    try {
        var value = localStorage.getItem(UTF8ToString(key));
        if (value === null) {
            return 0;
        }
        var length = lengthBytesUTF8(value) + 1;
        var ptr = _malloc(length);
        stringToUTF8(value, ptr, length);
        return ptr;
    } catch (e) {
        return 0;
    }
})

EM_JS(int, game_storage_web_save, (const char *key, const char *data), {
    try {
        localStorage.setItem(UTF8ToString(key), UTF8ToString(data));
        return 0;
    } catch (e) {
        return 1;
    }
})
#else
static bool make_dir_if_needed(const char *path) {
#ifdef _WIN32
    if (_mkdir(path) == 0) {
        return true;
    }
    return errno == EEXIST;
#else
    if (mkdir(path, 0755) == 0) {
        return true;
    }
    return errno == EEXIST;
#endif
}

static bool ensure_parent_dirs(const char *path, char *error, int error_cap) {
    char temp[GAME_STORAGE_PATH_MAX];
    if (snprintf(temp, sizeof(temp), "%s", path) >= (int)sizeof(temp)) {
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

static bool replace_file(const char *tmp_path, const char *path) {
#ifdef _WIN32
    return MoveFileExA(tmp_path, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    return rename(tmp_path, path) == 0;
#endif
}
#endif

bool game_storage_resolve_key(const char *key, const char *doc, char *out, int out_cap, char *error, int error_cap) {
    if (!game_storage_key_is_valid(key)) {
        set_error(error, error_cap, "bad storage key");
        return false;
    }
    if (!game_storage_doc_is_valid(doc)) {
        set_error(error, error_cap, "bad storage doc");
        return false;
    }
    if (!out || out_cap <= 0) {
        set_error(error, error_cap, "resolved storage output is required");
        return false;
    }
#ifdef NT_PLATFORM_WEB
    int written = snprintf(out, (size_t)out_cap, "%s.%s.%s", s_config.namespace_name, key, doc);
#else
    int written = snprintf(out, (size_t)out_cap, "%s/%s/%s.json", s_config.native_root, key, doc);
#endif
    if (written < 0 || written >= out_cap) {
        set_error(error, error_cap, "resolved storage key is too long");
        return false;
    }
    return true;
}

bool game_storage_save_json(const char *key, const char *doc, const char *json, char *error, int error_cap) {
    if (!json) {
        set_error(error, error_cap, "json is required");
        return false;
    }
    char resolved[GAME_STORAGE_PATH_MAX];
    if (!game_storage_resolve_key(key, doc, resolved, (int)sizeof(resolved), error, error_cap)) {
        return false;
    }
#ifdef NT_PLATFORM_WEB
    if (game_storage_web_save(resolved, json) != 0) {
        set_error(error, error_cap, "failed to save localStorage state");
        return false;
    }
    return true;
#else
    if (!ensure_parent_dirs(resolved, error, error_cap)) {
        return false;
    }
    char tmp_path[GAME_STORAGE_PATH_MAX];
    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", resolved) >= (int)sizeof(tmp_path)) {
        set_error(error, error_cap, "storage temp path is too long");
        return false;
    }
    FILE *file = fopen(tmp_path, "wb");
    if (!file) {
        set_error(error, error_cap, "failed to open storage file for write");
        return false;
    }
    size_t len = strlen(json);
    bool ok = fwrite(json, 1, len, file) == len;
    ok = fclose(file) == 0 && ok;
    if (!ok) {
        (void)remove(tmp_path);
        set_error(error, error_cap, "failed to write storage file");
        return false;
    }
    if (!replace_file(tmp_path, resolved)) {
        (void)remove(tmp_path);
        set_error(error, error_cap, "failed to replace storage file");
        return false;
    }
    return true;
#endif
}

bool game_storage_load_json(const char *key, const char *doc, char **out_json, char *error, int error_cap) {
    if (!out_json) {
        set_error(error, error_cap, "out_json is required");
        return false;
    }
    *out_json = NULL;
    char resolved[GAME_STORAGE_PATH_MAX];
    if (!game_storage_resolve_key(key, doc, resolved, (int)sizeof(resolved), error, error_cap)) {
        return false;
    }
#ifdef NT_PLATFORM_WEB
    char *data = game_storage_web_load(resolved);
    if (!data) {
        set_error(error, error_cap, "storage key not found");
        return false;
    }
    *out_json = data;
    return true;
#else
    FILE *file = fopen(resolved, "rb");
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
    if (size < 0 || size > GAME_STORAGE_MAX_FILE_BYTES) {
        fclose(file);
        set_error(error, error_cap, "storage file too large");
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
    size_t read_size = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (read_size != (size_t)size) {
        free(data);
        set_error(error, error_cap, "failed to read storage file");
        return false;
    }
    data[size] = '\0';
    *out_json = data;
    return true;
#endif
}
