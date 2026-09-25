#ifndef GAME_SAVE_SYNC_H
#define GAME_SAVE_SYNC_H

#include <stdbool.h>

typedef enum game_save_sync_state {
    GAME_SAVE_SYNC_WAITING_REMOTE = 0,
    GAME_SAVE_SYNC_REMOTE_UNAVAILABLE,
    GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH,
    GAME_SAVE_SYNC_UPLOAD_READY,
    GAME_SAVE_SYNC_STORE_PENDING,
    GAME_SAVE_SYNC_ADOPT_REMOTE,
    GAME_SAVE_SYNC_SYNCHRONIZED,
    GAME_SAVE_SYNC_CONFLICT,
} game_save_sync_state_t;

typedef enum game_save_choice {
    GAME_SAVE_ASK = 0,
    GAME_SAVE_KEEP_LOCAL,
    GAME_SAVE_KEEP_REMOTE,
} game_save_choice_t;

/* The coordinator owns copies of every supplied document.  Documents are
   opaque UTF-8 save payloads; transport, validation, and persistence remain
   consumer concerns. */
typedef struct game_save_sync {
    char *base;
    char *local;
    char *remote;
    char *sent;
    char *resolved_remote;
    game_save_sync_state_t state;
    bool local_is_fresh;
    bool remote_is_empty;
    bool remote_adoption_allowed;
    bool local_resolution_pending;
    bool resolved_remote_was_empty;
    /* The local document a remote adoption replaced while it held changes the
       base did not: player progress the adoption would otherwise destroy. */
    char *displaced;
} game_save_sync_t;

void game_save_sync_init(game_save_sync_t *sync);
void game_save_sync_destroy(game_save_sync_t *sync);

bool game_save_sync_set_base(game_save_sync_t *sync, const char *document);
bool game_save_sync_set_local(game_save_sync_t *sync, const char *document, bool is_fresh);
void game_save_sync_disallow_remote_adoption(game_save_sync_t *sync);
void game_save_sync_remote_pending(game_save_sync_t *sync);
void game_save_sync_remote_unavailable(game_save_sync_t *sync);
void game_save_sync_remote_empty(game_save_sync_t *sync);
bool game_save_sync_remote_document(game_save_sync_t *sync, const char *document);

game_save_sync_state_t game_save_sync_state(const game_save_sync_t *sync);
const char *game_save_sync_base_document(const game_save_sync_t *sync);
const char *game_save_sync_local_document(const game_save_sync_t *sync);
const char *game_save_sync_remote_document_value(const game_save_sync_t *sync);
const char *game_save_sync_upload_document(const game_save_sync_t *sync);
const char *game_save_sync_sent_document(const game_save_sync_t *sync);

/* Starts one transport write for the current upload document.  A successful
   finish commits exactly this copied snapshot as the shared base. */
bool game_save_sync_store_started(game_save_sync_t *sync);
void game_save_sync_store_finished(game_save_sync_t *sync, bool acknowledged);

/* Consumers validate and apply the returned remote document before committing
   adoption.  Keeping local requires a new remote read before any write. */
bool game_save_sync_resolve(game_save_sync_t *sync, game_save_choice_t resolution);
void game_save_sync_reject_remote(game_save_sync_t *sync);
bool game_save_sync_commit_remote_adoption(game_save_sync_t *sync);

/* After an adoption that replaced a non-fresh local document different from the
   remote one, that document is kept here, never discarded: the caller writes it
   to a quarantine slot, then clears it. A caller that never clears gets exactly
   one adoption: every later adoption that would displace a document is refused
   (commit returns false, the state stays ADOPT_REMOTE, local is untouched), so an
   unpersisted copy is never lost, and the sync does not converge until the
   clear. NULL when nothing was displaced. game_save_sync_destroy frees it. */
const char *game_save_sync_displaced_document(const game_save_sync_t *sync);
void game_save_sync_clear_displaced(game_save_sync_t *sync);

/* The local document the pending adoption would displace, or NULL: any
   non-fresh local that differs from the remote, whatever the base says. A
   caller that overwrites its own storage persists this copy first and adopts
   only if that write succeeded. */
const char *game_save_sync_adoption_displaces(const game_save_sync_t *sync);

/* The instance form of the cloud coordinator's automatic decision, for a caller
   that owns its transport and storage (several profiles, a server). The sync
   settles what the base decides: remote unchanged -> UPLOAD_READY, local
   unchanged -> ADOPT_REMOTE. A CONFLICT goes to `choose`; an emptied remote
   keeps local without asking, as the singleton does. KEEP_LOCAL still needs a
   fresh remote read before the upload (NEEDS_REMOTE_REFRESH), and ASK leaves the
   conflict for the player. Returns the state after deciding. */
typedef game_save_choice_t (*game_save_sync_choose_fn)(const char *local_document, const char *remote_document,
                                                      void *user);
game_save_sync_state_t game_save_sync_decide(game_save_sync_t *sync, game_save_sync_choose_fn choose, void *user);

#endif /* GAME_SAVE_SYNC_H */
