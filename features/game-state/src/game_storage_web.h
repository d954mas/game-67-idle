#ifndef GAME_STORAGE_WEB_H
#define GAME_STORAGE_WEB_H

#include <stddef.h>

/* A failing call fills `reason` with the thrown exception name, the only thing
   separating a full store (QuotaExceededError) from a blocked one
   (SecurityError). Untouched on success. */
#define GAME_STORAGE_WEB_REASON_MAX 96

int game_storage_web_key_exists(const char *key);
int game_storage_web_save(
    const char *key, const char *text, char *reason, int reason_cap);
char *game_storage_web_load(
    const char *key, size_t max_bytes, int *status, char *reason,
    int reason_cap);
int game_storage_web_quarantine(const char *key, char *reason, int reason_cap);
int game_storage_web_probe(const char *key, char *reason, int reason_cap);

#endif /* GAME_STORAGE_WEB_H */
