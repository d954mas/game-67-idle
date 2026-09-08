#ifndef GAME_SAVE_CLOUD_H
#define GAME_SAVE_CLOUD_H

#include <stdbool.h>

#include "game_save_sync.h"

typedef enum game_save_cloud_read_status {
    GAME_SAVE_CLOUD_READ_PENDING = 0,
    GAME_SAVE_CLOUD_READ_READY,
    GAME_SAVE_CLOUD_READ_EMPTY,
    GAME_SAVE_CLOUD_READ_FAILED,
} game_save_cloud_read_status_t;

typedef enum game_save_cloud_write_status {
    GAME_SAVE_CLOUD_WRITE_PENDING = 0,
    GAME_SAVE_CLOUD_WRITE_ACKNOWLEDGED,
    GAME_SAVE_CLOUD_WRITE_FAILED,
} game_save_cloud_write_status_t;

typedef struct game_save_cloud_transport {
    bool (*supported)(void);
    void (*load)(const char *key);
    /* READY transfers malloc-compatible text through owned_text; all other
       results leave it null. The coordinator frees the transferred text. */
    game_save_cloud_read_status_t (*read)(char **owned_text);
    /* key and text are borrowed for this call. Async transports copy text
       before returning. */
    bool (*store)(const char *key, const char *text);
    game_save_cloud_write_status_t (*write_status)(void);
} game_save_cloud_transport_t;

/* Decides only from the supplied documents. The coordinator calls it on actual
   document changes and rechecks KEEP_REMOTE at the safe point. */
typedef game_save_choice_t (*game_save_cloud_choose_fn)(
    const char *local_document, const char *remote_document);
typedef bool (*game_save_cloud_same_features_fn)(
    const char *first_document, const char *second_document);

typedef struct game_save_cloud_config {
    game_save_cloud_transport_t transport;
    game_save_cloud_choose_fn choose;
    game_save_cloud_same_features_fn same_features;
    const char *slot;
    const char *base_slot;
    const char *key;
} game_save_cloud_config_t;

/* This singleton shallow-copies config; strings and callbacks remain borrowed
   until shutdown. A null slot/key uses the active game-save default slot. */
bool game_save_cloud_init(const game_save_cloud_config_t *config);
void game_save_cloud_shutdown(void);

/* Starts and polls the initial read until it resolves or the bounded wait ends. */
bool game_save_cloud_boot_settled(void);
/* Starts once and returns true only when it adopted the account document. */
bool game_save_cloud_start(bool local_is_fresh);
void game_save_cloud_tick(void);

game_save_sync_state_t game_save_cloud_state(void);
const char *game_save_cloud_conflict_remote_document(void);
bool game_save_cloud_resolve(game_save_choice_t resolution);
/* Applies a previously chosen remote document and returns true only on adoption. */
bool game_save_cloud_apply_remote_at_safe_point(void);

#endif /* GAME_SAVE_CLOUD_H */
