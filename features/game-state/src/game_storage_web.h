#ifndef GAME_STORAGE_WEB_H
#define GAME_STORAGE_WEB_H

#include <stddef.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten/em_macros.h>
#define GAME_STORAGE_WEB_IMPORT(name) EM_IMPORT(name)
#else
#define GAME_STORAGE_WEB_IMPORT(name)
#endif

/* A failing call fills `reason` with the thrown exception name, the only thing
   separating a full store (QuotaExceededError) from a blocked one
   (SecurityError). Untouched on success. */
#define GAME_STORAGE_WEB_REASON_MAX 96

/* Cross-TU LTO needs the import identity at every call site, not only beside
   the EM_JS body that supplies its JavaScript metadata. */
int game_storage_web_key_exists(const char *key)
    GAME_STORAGE_WEB_IMPORT(game_storage_web_key_exists);
int game_storage_web_save(
    const char *key, const char *text, char *reason, int reason_cap)
    GAME_STORAGE_WEB_IMPORT(game_storage_web_save);
char *game_storage_web_load(
    const char *key, size_t max_bytes, int *status, char *reason,
    int reason_cap) GAME_STORAGE_WEB_IMPORT(game_storage_web_load);
int game_storage_web_quarantine(const char *key, char *reason, int reason_cap)
    GAME_STORAGE_WEB_IMPORT(game_storage_web_quarantine);
int game_storage_web_probe(const char *key, char *reason, int reason_cap)
    GAME_STORAGE_WEB_IMPORT(game_storage_web_probe);

#undef GAME_STORAGE_WEB_IMPORT

#endif /* GAME_STORAGE_WEB_H */
