#include "game_save_cloud.h"

#include "cJSON.h"
#include "game_save.h"
#include "game_storage.h"
#include "log/nt_log.h"
#include "time/nt_time.h"

#include <stdlib.h>
#include <string.h>

#ifndef GAME_SAVE_AUTOSAVE_SLOT
#define GAME_SAVE_AUTOSAVE_SLOT "autosave"
#endif

#define GAME_SAVE_CLOUD_DEFAULT_SLOT GAME_SAVE_AUTOSAVE_SLOT
#define GAME_SAVE_CLOUD_DEFAULT_BASE_SLOT "cloud_sync_base"
#define GAME_SAVE_CLOUD_BOOT_WAIT_SEC 4.0
#define GAME_SAVE_CLOUD_RETRY_SEC 5.0

typedef struct game_save_cloud_runtime {
    game_save_cloud_config_t config;
    game_save_sync_t sync;
    char *persisted_base;
    char *auto_local_snapshot;
    bool initialized;
    bool asked;
    bool read_pending;
    bool write_pending;
    bool started;
    bool has_read_attempt;
    bool has_write_attempt;
    bool first_wait_started;
    bool policy_pending;
    bool auto_remote_pending;
    double asked_at;
    double first_wait_at;
    double last_attempt_at;
    double last_read_at;
    int64_t last_local_saved_at;
} game_save_cloud_runtime_t;

static game_save_cloud_runtime_t s_cloud;

static char *copy_text(const char *text) {
    if (text == NULL) return NULL;
    const size_t length = strlen(text);
    char *copy = malloc(length + 1U);
    if (copy != NULL) memcpy(copy, text, length + 1U);
    return copy;
}

static bool same_text(const char *first, const char *second) {
    return first != NULL && second != NULL && strcmp(first, second) == 0;
}

static const char *slot_name(void) {
    return s_cloud.config.slot != NULL ? s_cloud.config.slot : GAME_SAVE_CLOUD_DEFAULT_SLOT;
}

static const char *base_slot_name(void) {
    return s_cloud.config.base_slot != NULL
        ? s_cloud.config.base_slot : GAME_SAVE_CLOUD_DEFAULT_BASE_SLOT;
}

static const char *cloud_key(void) {
    return s_cloud.config.key != NULL ? s_cloud.config.key : slot_name();
}

static void clear_auto_remote(void) {
    free(s_cloud.auto_local_snapshot);
    s_cloud.auto_local_snapshot = NULL;
    s_cloud.auto_remote_pending = false;
}

static char *envelope_build(int64_t saved_at, const char *document) {
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;
    char *text = NULL;
    if (cJSON_AddNumberToObject(root, "saved_at", (double)saved_at) &&
        cJSON_AddStringToObject(root, "doc", document)) {
        text = cJSON_PrintUnformatted(root);
    }
    cJSON_Delete(root);
    return text;
}

static bool envelope_parse(const char *text, char **document_out) {
    cJSON *root = cJSON_Parse(text);
    if (root == NULL) return false;
    const cJSON *stamp = cJSON_GetObjectItemCaseSensitive(root, "saved_at");
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "doc");
    const bool valid = cJSON_IsNumber(stamp) && cJSON_IsString(body) &&
        body->valuestring != NULL;
    if (valid) *document_out = copy_text(body->valuestring);
    cJSON_Delete(root);
    return valid && *document_out != NULL;
}

static bool persist_base(void) {
    const char *base = game_save_sync_base_document(&s_cloud.sync);
    if (base == NULL || same_text(base, s_cloud.persisted_base)) return true;
    char error[160] = {0};
    if (!game_storage_write(base_slot_name(), base, error, (int)sizeof error)) {
        nt_log_warn("game_save_cloud: could not persist shared base (%s)",
                    error[0] != '\0' ? error : "no reason reported");
        return false;
    }
    char *copy = copy_text(base);
    if (copy == NULL) return false;
    free(s_cloud.persisted_base);
    s_cloud.persisted_base = copy;
    return true;
}

static bool refresh_local(bool local_is_fresh) {
    char *document = NULL;
    game_storage_read_status_t status = GAME_STORAGE_READ_ABSENT;
    char error[160] = {0};
    if (!game_storage_read(slot_name(), &document, &status, error, (int)sizeof error) ||
        document == NULL) {
        if (status == GAME_STORAGE_READ_ABSENT && local_is_fresh) {
            document = game_save_export_string(error, (int)sizeof error);
        }
        if (document == NULL) return false;
    }
    const bool changed = !same_text(game_save_sync_local_document(&s_cloud.sync), document);
    const bool accepted = game_save_sync_set_local(&s_cloud.sync, document, local_is_fresh);
    free(document);
    if (accepted && changed) s_cloud.policy_pending = true;
    return accepted;
}

static void consider_auto_resolution(void) {
    if (!s_cloud.policy_pending) return;
    s_cloud.policy_pending = false;
    if (s_cloud.config.choose == NULL ||
        game_save_sync_state(&s_cloud.sync) != GAME_SAVE_SYNC_CONFLICT) return;
    const char *local = game_save_sync_local_document(&s_cloud.sync);
    const char *remote = game_save_sync_remote_document_value(&s_cloud.sync);
    if (local == NULL || remote == NULL) return;
    switch (s_cloud.config.choose(local, remote)) {
        case GAME_SAVE_KEEP_LOCAL:
            (void)game_save_sync_resolve(&s_cloud.sync, GAME_SAVE_KEEP_LOCAL);
            break;
        case GAME_SAVE_KEEP_REMOTE: {
            if (s_cloud.config.same_features == NULL) return;
            char *snapshot = copy_text(local);
            if (snapshot != NULL &&
                game_save_sync_resolve(&s_cloud.sync, GAME_SAVE_KEEP_REMOTE)) {
                clear_auto_remote();
                s_cloud.auto_local_snapshot = snapshot;
                s_cloud.auto_remote_pending = true;
            } else {
                free(snapshot);
            }
            break;
        }
        case GAME_SAVE_ASK:
        default:
            break;
    }
}

static void consume_remote_result(void) {
    if (!s_cloud.read_pending) return;
    char *envelope = NULL;
    const game_save_cloud_read_status_t status = s_cloud.config.transport.read(&envelope);
    if (status == GAME_SAVE_CLOUD_READ_PENDING) return;
    s_cloud.read_pending = false;
    s_cloud.policy_pending = true;
    if (status == GAME_SAVE_CLOUD_READ_READY) {
        char *document = NULL;
        if (envelope != NULL && envelope_parse(envelope, &document)) {
            (void)game_save_sync_remote_document(&s_cloud.sync, document);
        } else {
            game_save_sync_remote_unavailable(&s_cloud.sync);
        }
        free(document);
    } else if (status == GAME_SAVE_CLOUD_READ_EMPTY) {
        game_save_sync_remote_empty(&s_cloud.sync);
    } else {
        game_save_sync_remote_unavailable(&s_cloud.sync);
        if (!s_cloud.started) {
            s_cloud.last_read_at = nt_time_now() - GAME_SAVE_CLOUD_RETRY_SEC;
        }
    }
    free(envelope);
    (void)persist_base();
}

static void request_remote_refresh(bool initial) {
    const double now = nt_time_now();
    if (!s_cloud.asked ||
        (s_cloud.has_read_attempt && now - s_cloud.last_read_at < GAME_SAVE_CLOUD_RETRY_SEC)) return;
    s_cloud.last_read_at = now;
    s_cloud.has_read_attempt = true;
    s_cloud.read_pending = true;
    s_cloud.asked_at = initial ? now : s_cloud.asked_at;
    s_cloud.config.transport.load(cloud_key());
    game_save_sync_remote_pending(&s_cloud.sync);
}

static void begin_read(void) {
    if (s_cloud.asked || !s_cloud.config.transport.supported()) return;
    s_cloud.asked = true;
    s_cloud.asked_at = nt_time_now();
    s_cloud.last_read_at = s_cloud.asked_at;
    s_cloud.has_read_attempt = true;
    s_cloud.read_pending = true;
    s_cloud.config.transport.load(cloud_key());
}

static bool adopt_remote(void) {
    if (game_save_sync_state(&s_cloud.sync) != GAME_SAVE_SYNC_ADOPT_REMOTE) return false;
    const char *remote = game_save_sync_remote_document_value(&s_cloud.sync);
    if (remote == NULL) return false;
    char error[160] = {0};
    char *live_before = game_save_export_string(error, (int)sizeof error);
    if (live_before == NULL || !game_save_import_string(remote, error, (int)sizeof error)) {
        free(live_before);
        game_save_sync_reject_remote(&s_cloud.sync);
        return false;
    }
    if (!game_storage_write_blocking(slot_name(), remote, error, (int)sizeof error)) {
        (void)game_save_import_string(live_before, NULL, 0);
        free(live_before);
        game_save_sync_reject_remote(&s_cloud.sync);
        return false;
    }
    free(live_before);
    if (!game_save_sync_commit_remote_adoption(&s_cloud.sync)) {
        (void)game_save_sync_set_local(&s_cloud.sync, remote, false);
        game_save_sync_reject_remote(&s_cloud.sync);
        return true;
    }
    (void)persist_base();
    return true;
}

static void finish_write(void) {
    if (!s_cloud.write_pending) return;
    const game_save_cloud_write_status_t status = s_cloud.config.transport.write_status();
    if (status == GAME_SAVE_CLOUD_WRITE_PENDING) return;
    s_cloud.write_pending = false;
    game_save_sync_store_finished(&s_cloud.sync,
                                  status == GAME_SAVE_CLOUD_WRITE_ACKNOWLEDGED);
    if (status == GAME_SAVE_CLOUD_WRITE_ACKNOWLEDGED) (void)persist_base();
}

static void start_write(void) {
    if (s_cloud.write_pending ||
        game_save_sync_state(&s_cloud.sync) != GAME_SAVE_SYNC_UPLOAD_READY) return;
    const double now = nt_time_now();
    /* The retry floor and the configured interval are the same gate: both say
       how soon a new upload may start, and the wider one wins. */
    const double gap = s_cloud.config.min_write_interval_sec > GAME_SAVE_CLOUD_RETRY_SEC
        ? s_cloud.config.min_write_interval_sec : GAME_SAVE_CLOUD_RETRY_SEC;
    if (s_cloud.has_write_attempt && now - s_cloud.last_attempt_at < gap) return;
    if (!game_save_sync_store_started(&s_cloud.sync)) return;
    char *envelope = envelope_build(game_save_last_saved_at(),
                                    game_save_sync_sent_document(&s_cloud.sync));
    if (envelope == NULL) {
        game_save_sync_store_finished(&s_cloud.sync, false);
        return;
    }
    const bool sent = s_cloud.config.transport.store(cloud_key(), envelope);
    free(envelope);
    s_cloud.last_attempt_at = now;
    s_cloud.has_write_attempt = true;
    if (!sent) {
        game_save_sync_store_finished(&s_cloud.sync, false);
        return;
    }
    s_cloud.write_pending = true;
}

bool game_save_cloud_init(const game_save_cloud_config_t *config) {
    if (config == NULL || config->transport.supported == NULL ||
        config->transport.load == NULL || config->transport.read == NULL ||
        config->transport.store == NULL || config->transport.write_status == NULL) return false;
    game_save_cloud_shutdown();
    s_cloud.config = *config;
    game_save_sync_init(&s_cloud.sync);
    s_cloud.initialized = true;
    char *base = NULL;
    game_storage_read_status_t status = GAME_STORAGE_READ_ABSENT;
    char error[160] = {0};
    if (game_storage_read(base_slot_name(), &base, &status, error, (int)sizeof error) &&
        base != NULL && game_save_sync_set_base(&s_cloud.sync, base)) {
        s_cloud.persisted_base = copy_text(base);
    }
    free(base);
    return true;
}

void game_save_cloud_shutdown(void) {
    if (s_cloud.initialized) game_save_sync_destroy(&s_cloud.sync);
    free(s_cloud.persisted_base);
    clear_auto_remote();
    s_cloud = (game_save_cloud_runtime_t){0};
}

bool game_save_cloud_boot_settled(void) {
    if (!s_cloud.initialized) return true;
    const double now = nt_time_now();
    if (!s_cloud.first_wait_started) {
        s_cloud.first_wait_at = now;
        s_cloud.first_wait_started = true;
    }
    begin_read();
    consume_remote_result();
#if defined(__EMSCRIPTEN__)
    if (!s_cloud.asked) return now - s_cloud.first_wait_at >= GAME_SAVE_CLOUD_BOOT_WAIT_SEC;
#else
    if (!s_cloud.asked) return true;
#endif
    if (!s_cloud.read_pending) return true;
    return now - s_cloud.asked_at >= GAME_SAVE_CLOUD_BOOT_WAIT_SEC;
}

bool game_save_cloud_start(bool local_is_fresh) {
    if (!s_cloud.initialized || s_cloud.started) return false;
    begin_read();
    if (!refresh_local(local_is_fresh)) {
        s_cloud.started = true;
        game_save_sync_disallow_remote_adoption(&s_cloud.sync);
        return false;
    }
    consume_remote_result();
    consider_auto_resolution();
    const bool adopted = adopt_remote();
    if (adopted) clear_auto_remote();
    s_cloud.started = true;
    game_save_sync_disallow_remote_adoption(&s_cloud.sync);
    return adopted;
}

void game_save_cloud_tick(void) {
    if (!s_cloud.initialized) return;
    begin_read();
    finish_write();
    consume_remote_result();
    const int64_t saved_at = game_save_last_saved_at();
    if (saved_at != 0 && saved_at != s_cloud.last_local_saved_at && refresh_local(false)) {
        s_cloud.last_local_saved_at = saved_at;
    }
    consider_auto_resolution();
    const game_save_sync_state_t state = game_save_sync_state(&s_cloud.sync);
    if (s_cloud.read_pending || state == GAME_SAVE_SYNC_NEEDS_REMOTE_REFRESH ||
        state == GAME_SAVE_SYNC_REMOTE_UNAVAILABLE) {
        request_remote_refresh(false);
        return;
    }
    start_write();
}

game_save_sync_state_t game_save_cloud_state(void) {
    return s_cloud.initialized ? game_save_sync_state(&s_cloud.sync)
        : GAME_SAVE_SYNC_WAITING_REMOTE;
}

const char *game_save_cloud_conflict_remote_document(void) {
    return game_save_cloud_state() == GAME_SAVE_SYNC_CONFLICT
        ? game_save_sync_remote_document_value(&s_cloud.sync) : NULL;
}

bool game_save_cloud_resolve(game_save_choice_t resolution) {
    if (!s_cloud.initialized) return false;
    const bool resolved = game_save_sync_resolve(&s_cloud.sync, resolution);
    if (!resolved && resolution != GAME_SAVE_ASK && s_cloud.auto_remote_pending) {
        game_save_sync_reject_remote(&s_cloud.sync);
        clear_auto_remote();
    }
    return resolved;
}

bool game_save_cloud_apply_remote_at_safe_point(void) {
    if (!s_cloud.initialized) return false;
    if (s_cloud.auto_remote_pending) {
        char error[160] = {0};
        char *current = game_save_export_string(error, (int)sizeof error);
        const char *remote = game_save_sync_remote_document_value(&s_cloud.sync);
        const bool unchanged = current != NULL && s_cloud.config.same_features != NULL &&
            s_cloud.config.same_features(s_cloud.auto_local_snapshot, current);
        const bool still_prefer_remote = unchanged && remote != NULL &&
            s_cloud.config.choose != NULL &&
            s_cloud.config.choose(current, remote) == GAME_SAVE_KEEP_REMOTE;
        if (!still_prefer_remote) {
            if (current != NULL) (void)game_save_sync_set_local(&s_cloud.sync, current, false);
            game_save_sync_reject_remote(&s_cloud.sync);
            free(current);
            clear_auto_remote();
            return false;
        }
        free(current);
        const bool adopted = adopt_remote();
        clear_auto_remote();
        return adopted;
    }
    return adopt_remote();
}
