#include "game_save_sync.h"

#include <stdlib.h>
#include <string.h>

static char *copy_document(const char *document) {
    if (document == NULL) return NULL;
    const size_t length = strlen(document);
    char *copy = malloc(length + 1U);
    if (copy != NULL) memcpy(copy, document, length + 1U);
    return copy;
}

static bool replace_document(char **destination, const char *document) {
    char *copy = copy_document(document);
    if (document != NULL && copy == NULL) return false;
    free(*destination);
    *destination = copy;
    return true;
}

static bool documents_equal(const char *first, const char *second) {
    return first != NULL && second != NULL && strcmp(first, second) == 0;
}

static void evaluate(game_save_sync_t *sync) {
    if (sync->local == NULL) {
        sync->state = GAME_SAVE_SYNC_WAITING_REMOTE;
        return;
    }
    if (sync->remote_is_empty) {
        if (sync->local_resolution_pending) {
            sync->state = sync->resolved_remote_was_empty
                ? GAME_SAVE_SYNC_UPLOAD_READY : GAME_SAVE_SYNC_CONFLICT;
        } else {
            sync->state = sync->base == NULL
                ? GAME_SAVE_SYNC_UPLOAD_READY : GAME_SAVE_SYNC_CONFLICT;
        }
        return;
    }
    if (sync->remote == NULL) {
        sync->state = GAME_SAVE_SYNC_WAITING_REMOTE;
        return;
    }
    if (documents_equal(sync->local, sync->remote)) {
        if (!replace_document(&sync->base, sync->local)) {
            sync->state = GAME_SAVE_SYNC_CONFLICT;
            return;
        }
        free(sync->resolved_remote);
        sync->resolved_remote = NULL;
        sync->resolved_remote_was_empty = false;
        sync->local_resolution_pending = false;
        sync->state = GAME_SAVE_SYNC_SYNCHRONIZED;
    } else if (sync->local_resolution_pending) {
        if (!sync->resolved_remote_was_empty &&
            documents_equal(sync->remote, sync->resolved_remote)) {
            sync->state = GAME_SAVE_SYNC_UPLOAD_READY;
        } else {
            free(sync->resolved_remote);
            sync->resolved_remote = NULL;
            sync->resolved_remote_was_empty = false;
            sync->local_resolution_pending = false;
            sync->state = GAME_SAVE_SYNC_CONFLICT;
        }
    } else if (sync->base == NULL) {
        sync->state = sync->local_is_fresh && sync->remote_adoption_allowed
            ? GAME_SAVE_SYNC_ADOPT_REMOTE : GAME_SAVE_SYNC_CONFLICT;
    } else if (documents_equal(sync->local, sync->base)) {
        sync->state = sync->remote_adoption_allowed
            ? GAME_SAVE_SYNC_ADOPT_REMOTE : GAME_SAVE_SYNC_CONFLICT;
    } else if (documents_equal(sync->remote, sync->base)) {
        sync->state = GAME_SAVE_SYNC_UPLOAD_READY;
    } else {
        sync->state = GAME_SAVE_SYNC_CONFLICT;
    }
}

void game_save_sync_init(game_save_sync_t *sync) {
    if (sync == NULL) return;
    *sync = (game_save_sync_t){
        .state = GAME_SAVE_SYNC_WAITING_REMOTE,
        .remote_adoption_allowed = true,
    };
}

void game_save_sync_destroy(game_save_sync_t *sync) {
    if (sync == NULL) return;
    free(sync->base);
    free(sync->local);
    free(sync->remote);
    free(sync->sent);
    free(sync->resolved_remote);
    *sync = (game_save_sync_t){.state = GAME_SAVE_SYNC_WAITING_REMOTE};
}

bool game_save_sync_set_base(game_save_sync_t *sync, const char *document) {
    return sync != NULL && replace_document(&sync->base, document);
}

bool game_save_sync_set_local(game_save_sync_t *sync, const char *document, bool is_fresh) {
    if (sync == NULL || document == NULL) return false;
    if (documents_equal(sync->local, document)) return true;
    const bool unresolved_conflict = sync->state == GAME_SAVE_SYNC_CONFLICT;
    const bool observed_remote = sync->state == GAME_SAVE_SYNC_WAITING_REMOTE &&
        (sync->remote != NULL || sync->remote_is_empty);
    if (!replace_document(&sync->local, document)) return false;
    sync->local_is_fresh = is_fresh;
    if (unresolved_conflict || observed_remote) {
        evaluate(sync);
    } else if (sync->state != GAME_SAVE_SYNC_STORE_PENDING &&
               sync->state != GAME_SAVE_SYNC_UPLOAD_READY) {
        sync->state = documents_equal(sync->local, sync->base)
            ? GAME_SAVE_SYNC_SYNCHRONIZED : GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH;
    }
    return true;
}

void game_save_sync_disallow_remote_adoption(game_save_sync_t *sync) {
    if (sync == NULL) return;
    sync->remote_adoption_allowed = false;
    if (sync->state == GAME_SAVE_SYNC_ADOPT_REMOTE) sync->state = GAME_SAVE_SYNC_CONFLICT;
}

void game_save_sync_remote_pending(game_save_sync_t *sync) {
    if (sync == NULL || sync->state == GAME_SAVE_SYNC_STORE_PENDING) return;
    sync->remote_is_empty = false;
    free(sync->remote);
    sync->remote = NULL;
    sync->state = GAME_SAVE_SYNC_WAITING_REMOTE;
}

void game_save_sync_remote_unavailable(game_save_sync_t *sync) {
    if (sync == NULL || sync->state == GAME_SAVE_SYNC_STORE_PENDING) return;
    sync->remote_is_empty = false;
    free(sync->remote);
    sync->remote = NULL;
    sync->state = GAME_SAVE_SYNC_REMOTE_UNAVAILABLE;
}

void game_save_sync_remote_empty(game_save_sync_t *sync) {
    if (sync == NULL || sync->state == GAME_SAVE_SYNC_STORE_PENDING) return;
    free(sync->remote);
    sync->remote = NULL;
    sync->remote_is_empty = true;
    evaluate(sync);
}

bool game_save_sync_remote_document(game_save_sync_t *sync, const char *document) {
    if (sync == NULL || document == NULL || sync->state == GAME_SAVE_SYNC_STORE_PENDING ||
        !replace_document(&sync->remote, document)) return false;
    sync->remote_is_empty = false;
    evaluate(sync);
    return true;
}

game_save_sync_state_t game_save_sync_state(const game_save_sync_t *sync) {
    return sync != NULL ? sync->state : GAME_SAVE_SYNC_REMOTE_UNAVAILABLE;
}

const char *game_save_sync_base_document(const game_save_sync_t *sync) {
    return sync != NULL ? sync->base : NULL;
}

const char *game_save_sync_local_document(const game_save_sync_t *sync) {
    return sync != NULL ? sync->local : NULL;
}

const char *game_save_sync_remote_document_value(const game_save_sync_t *sync) {
    return sync != NULL ? sync->remote : NULL;
}

const char *game_save_sync_upload_document(const game_save_sync_t *sync) {
    return sync != NULL && sync->state == GAME_SAVE_SYNC_UPLOAD_READY ? sync->local : NULL;
}

const char *game_save_sync_sent_document(const game_save_sync_t *sync) {
    return sync != NULL && sync->state == GAME_SAVE_SYNC_STORE_PENDING ? sync->sent : NULL;
}

bool game_save_sync_store_started(game_save_sync_t *sync) {
    const char *document = game_save_sync_upload_document(sync);
    if (sync == NULL || document == NULL || !replace_document(&sync->sent, document)) return false;
    sync->state = GAME_SAVE_SYNC_STORE_PENDING;
    return true;
}

void game_save_sync_store_finished(game_save_sync_t *sync, bool acknowledged) {
    if (sync == NULL || sync->state != GAME_SAVE_SYNC_STORE_PENDING) return;
    if (acknowledged && sync->sent != NULL && replace_document(&sync->base, sync->sent)) {
        sync->remote_is_empty = false;
        (void)replace_document(&sync->remote, sync->sent);
        free(sync->resolved_remote);
        sync->resolved_remote = NULL;
        sync->resolved_remote_was_empty = false;
        sync->local_resolution_pending = false;
        sync->state = documents_equal(sync->local, sync->base)
            ? GAME_SAVE_SYNC_SYNCHRONIZED : GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH;
    } else {
        sync->state = GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH;
    }
    free(sync->sent);
    sync->sent = NULL;
}

bool game_save_sync_resolve(game_save_sync_t *sync, game_save_choice_t resolution) {
    if (sync == NULL || sync->state != GAME_SAVE_SYNC_CONFLICT) return false;
    if (resolution == GAME_SAVE_KEEP_REMOTE && sync->remote != NULL) {
        sync->state = GAME_SAVE_SYNC_ADOPT_REMOTE;
        return true;
    }
    if (resolution == GAME_SAVE_KEEP_LOCAL &&
        (sync->remote != NULL || sync->remote_is_empty) &&
        replace_document(&sync->resolved_remote, sync->remote)) {
        sync->resolved_remote_was_empty = sync->remote_is_empty;
        sync->local_resolution_pending = true;
        sync->state = GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH;
        return true;
    }
    return false;
}

void game_save_sync_reject_remote(game_save_sync_t *sync) {
    if (sync != NULL && sync->remote != NULL) sync->state = GAME_SAVE_SYNC_CONFLICT;
}

bool game_save_sync_commit_remote_adoption(game_save_sync_t *sync) {
    if (sync == NULL || sync->state != GAME_SAVE_SYNC_ADOPT_REMOTE || sync->remote == NULL ||
        !replace_document(&sync->local, sync->remote) || !replace_document(&sync->base, sync->remote)) return false;
    sync->local_is_fresh = false;
    free(sync->resolved_remote);
    sync->resolved_remote = NULL;
    sync->resolved_remote_was_empty = false;
    sync->local_resolution_pending = false;
    sync->state = GAME_SAVE_SYNC_SYNCHRONIZED;
    return true;
}
