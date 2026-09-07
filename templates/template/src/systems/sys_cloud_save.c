#include "systems/sys_cloud_save.h"

#include "cJSON.h"
#include "features/platform_sdk/platform_sdk_cloud.h"
#include "game_save.h"
#include "game_storage.h"
#include "log/nt_log.h"
#include "time/nt_time.h"

#include <stdlib.h>
#include <string.h>

/* The same slot the game saves into locally: the build names it once and both
   sides of the mirror must mean the same document. */
#ifndef GAME_SAVE_AUTOSAVE_SLOT
#define GAME_SAVE_AUTOSAVE_SLOT "autosave"
#endif
#define CLOUD_SAVE_SLOT GAME_SAVE_AUTOSAVE_SLOT
#define CLOUD_SAVE_KEY GAME_SAVE_AUTOSAVE_SLOT

/* A portal that never answers must not hold the game on a loading screen: the
   local save is a complete game on its own. */
#define CLOUD_SAVE_WAIT_SEC 4.0

/* The portal is asked no more often than a player switches devices, not as
   often as the run changes. */
#define CLOUD_SAVE_MIRROR_SEC 5.0

static bool s_asked;
static double s_asked_at;
#if defined(__EMSCRIPTEN__)
static double s_first_wait;
#endif
static int64_t s_mirrored_at;
static double s_last_mirror;

static char *envelope_build(int64_t saved_at, const char *doc) {
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    char *text = NULL;
    if (cJSON_AddNumberToObject(root, "saved_at", (double)saved_at) &&
        cJSON_AddStringToObject(root, "doc", doc)) {
        text = cJSON_PrintUnformatted(root);
    }
    cJSON_Delete(root);
    return text;
}

/* The envelope is ours: the game writes it, so the stamp can be read back
   without decoding the save document itself. */
static bool envelope_parse(const char *text, int64_t *saved_at, char **doc) {
    cJSON *root = cJSON_Parse(text);
    if (!root) return false;
    const cJSON *stamp = cJSON_GetObjectItemCaseSensitive(root, "saved_at");
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "doc");
    bool ok = false;
    if (cJSON_IsNumber(stamp) && cJSON_IsString(body) && body->valuestring != NULL) {
        char *copy = malloc(strlen(body->valuestring) + 1U);
        if (copy) {
            memcpy(copy, body->valuestring, strlen(body->valuestring) + 1U);
            *saved_at = (int64_t)stamp->valuedouble;
            *doc = copy;
            ok = true;
        }
    }
    cJSON_Delete(root);
    return ok;
}

void cloud_save_begin(void) {
    if (s_asked || !platform_sdk_cloud_supported()) return;
    s_asked = true;
    s_asked_at = nt_time_now();
    platform_sdk_cloud_load(CLOUD_SAVE_KEY);
}

bool cloud_save_settled(void) {
#if defined(__EMSCRIPTEN__)
    const double now = nt_time_now();
    if (s_first_wait <= 0.0) s_first_wait = now;
    /* The portal's own script and the game's module load in an order neither
       controls, so "there is no storage here" is only true once waiting for it
       has failed. Asking again each frame costs a property lookup. */
    cloud_save_begin();
    if (!s_asked) return now - s_first_wait >= CLOUD_SAVE_WAIT_SEC;
    if (platform_sdk_cloud_status() != PLATFORM_SDK_CLOUD_PENDING) return true;
    if (now - s_asked_at >= CLOUD_SAVE_WAIT_SEC) {
        nt_log_warn("cloud_save: the portal did not answer in %.0f s; playing the local save",
                    CLOUD_SAVE_WAIT_SEC);
        return true;
    }
    return false;
#else
    /* No portal outside the browser: the local save is the whole story. */
    return true;
#endif
}

bool cloud_save_adopt(bool local_is_fresh) {
    /* One line per boot, and it is the only account of a decision the player
       can neither see nor undo. */
    nt_log_info("cloud_save: status %d, local %s", (int)platform_sdk_cloud_status(),
                local_is_fresh ? "fresh" : "loaded");
    if (platform_sdk_cloud_status() != PLATFORM_SDK_CLOUD_READY) return false;
    char *envelope = platform_sdk_cloud_take();
    if (!envelope) return false;

    int64_t cloud_saved_at = 0;
    char *doc = NULL;
    bool adopted = false;
    if (!envelope_parse(envelope, &cloud_saved_at, &doc)) {
        nt_log_warn("cloud_save: the portal's copy is not a save envelope; keeping the local one");
    } else if (!local_is_fresh && cloud_saved_at <= game_save_last_saved_at()) {
        nt_log_info("cloud_save: keeping the local run (local %lld, account %lld)",
                    (long long)game_save_last_saved_at(), (long long)cloud_saved_at);
        /* Same device, or this browser holds the newer run. The mirror will
           push it up on the next tick. */
    } else {
        char error[160] = {0};
        if (game_storage_write_blocking(CLOUD_SAVE_SLOT, doc, error, (int)sizeof error)) {
            adopted = true;
            s_mirrored_at = cloud_saved_at;
            nt_log_info("cloud_save: took the account's run (%lld)", (long long)cloud_saved_at);
        } else {
            nt_log_warn("cloud_save: could not adopt the portal's copy (%s)",
                        error[0] != '\0' ? error : "no reason reported");
        }
    }
    free(doc);
    free(envelope);
    return adopted;
}

void cloud_save_tick(void) {
    if (!s_asked) return;
    const int64_t saved_at = game_save_last_saved_at();
    if (saved_at == 0 || saved_at == s_mirrored_at) return;
    const double now = nt_time_now();
    if (s_last_mirror > 0.0 && now - s_last_mirror < CLOUD_SAVE_MIRROR_SEC) return;

    char *doc = NULL;
    game_storage_read_status_t status = GAME_STORAGE_READ_OK;
    char error[160] = {0};
    if (!game_storage_read(CLOUD_SAVE_SLOT, &doc, &status, error, (int)sizeof error) || !doc) {
        return;
    }
    char *envelope = envelope_build(saved_at, doc);
    free(doc);
    if (!envelope) return;
    platform_sdk_cloud_store(CLOUD_SAVE_KEY, envelope);
    free(envelope);
    s_mirrored_at = saved_at;
    s_last_mirror = now;
}
