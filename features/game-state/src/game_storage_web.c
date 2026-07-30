#include "game_storage_web.h"

#include <emscripten/emscripten.h>

/* The adapter receives fully resolved, app-scoped keys. It is the only
   game-state source that touches localStorage or the browser heap directly. */
/* clang-format off */
EM_JS_DEPS(game_storage_web, "$UTF8ToString,$lengthBytesUTF8,$stringToUTF8,malloc,free")

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

EM_JS(char *, game_storage_web_load, (const char *key_ptr, int *status_ptr), {
    var ptr = 0;
    var key = null;
    var data = null;
    var quarantineCopy = function(k, raw) {
        if (k === null || raw === null) { return false; }
        try {
            var quarantineKey = k + ".corrupt";
            window.localStorage.setItem(quarantineKey, raw);
            return window.localStorage.getItem(quarantineKey) === raw;
        } catch (copyError) {
            return false;
        }
    };
    try {
        key = UTF8ToString(key_ptr);
        data = window.localStorage.getItem(key);
        if (data === null) {
            if (status_ptr) { HEAP32[status_ptr >> 2] = 1; }
            return 0;
        }
        var size = lengthBytesUTF8(data) + 1;
        ptr = _malloc(size);
        if (!ptr) {
            var copied = quarantineCopy(key, data);
            if (status_ptr) { HEAP32[status_ptr >> 2] = copied ? 2 : 3; }
            return 0;
        }
        stringToUTF8(data, ptr, size);
        if (status_ptr) { HEAP32[status_ptr >> 2] = 0; }
        return ptr;
    } catch (e) {
        if (ptr) { _free(ptr); }
        var copied = quarantineCopy(key, data);
        if (status_ptr) { HEAP32[status_ptr >> 2] = copied ? 2 : 3; }
        return 0;
    }
})

EM_JS(int, game_storage_web_quarantine, (const char *key_ptr), {
    try {
        var key = UTF8ToString(key_ptr);
        var data = window.localStorage.getItem(key);
        if (data === null) { return 0; }
        var quarantineKey = key + ".corrupt";
        window.localStorage.setItem(quarantineKey, data);
        if (window.localStorage.getItem(quarantineKey) !== data) { return 0; }
        try { window.localStorage.removeItem(key); } catch (deleteError) {}
        return 1;
    } catch (e) {
        return 0;
    }
})

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
