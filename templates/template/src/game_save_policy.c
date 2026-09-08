#include "game_save_policy.h"

#include <limits.h>

#include "game_save.h"
#include "game_state_json.h"

typedef struct game_save_progress {
    int hero_level;
    bool tutorial_done;
    int64_t playtime_ms;
} game_save_progress_t;

static bool read_progress(const cJSON *document, game_save_progress_t *out,
                          const cJSON **features_out) {
    if (!cJSON_IsObject(document) || out == NULL || features_out == NULL) return false;
    const cJSON *features = cJSON_GetObjectItemCaseSensitive(document, "features");
    const cJSON *game = features != NULL
        ? cJSON_GetObjectItemCaseSensitive(features, "game") : NULL;
    const cJSON *progression = features != NULL
        ? cJSON_GetObjectItemCaseSensitive(features, "progression") : NULL;
    const cJSON *tutorial = game != NULL
        ? cJSON_GetObjectItemCaseSensitive(game, "tutorial") : NULL;
    const cJSON *done = tutorial != NULL
        ? cJSON_GetObjectItemCaseSensitive(tutorial, "done") : NULL;
    const cJSON *tracks = progression != NULL
        ? cJSON_GetObjectItemCaseSensitive(progression, "tracks") : NULL;
    const cJSON *hero = tracks != NULL
        ? cJSON_GetObjectItemCaseSensitive(tracks, "hero") : NULL;
    if (!cJSON_IsObject(features) || !cJSON_IsObject(game) ||
        !cJSON_IsObject(progression) || !cJSON_IsObject(tutorial) ||
        !cJSON_IsBool(done) || !cJSON_IsObject(tracks)) return false;

    int hero_level = 0;
    if (hero != NULL) {
        const cJSON *level = cJSON_GetObjectItemCaseSensitive(hero, "level");
        if (!cJSON_IsObject(hero) || !cJSON_IsNumber(level) ||
            level->valuedouble < 0.0 || level->valuedouble > 9999.0 ||
            level->valuedouble != (double)(int)level->valuedouble) return false;
        hero_level = (int)level->valuedouble;
    }
    int64_t playtime_ms = 0;
    if (!gsj_read_i64(document, "playtime_ms", 0, INT64_MAX, &playtime_ms,
                      NULL, 0)) return false;
    *out = (game_save_progress_t){
        .hero_level = hero_level,
        .tutorial_done = cJSON_IsTrue(done),
        .playtime_ms = playtime_ms,
    };
    *features_out = features;
    return true;
}

static bool parse_progress(const char *document, cJSON **root_out,
                           game_save_progress_t *progress_out,
                           const cJSON **features_out) {
    if (document == NULL || document[0] != '{' ||
        !game_save_validate_document_string(document, NULL, 0)) return false;
    cJSON *root = cJSON_Parse(document);
    if (root == NULL || !read_progress(root, progress_out, features_out)) {
        cJSON_Delete(root);
        return false;
    }
    *root_out = root;
    return true;
}

bool game_save_policy_same_features(
    const char *first_document, const char *second_document) {
    cJSON *first_root = NULL;
    cJSON *second_root = NULL;
    game_save_progress_t first_progress;
    game_save_progress_t second_progress;
    const cJSON *first_features = NULL;
    const cJSON *second_features = NULL;
    const bool valid = parse_progress(first_document, &first_root, &first_progress,
                                      &first_features) &&
                       parse_progress(second_document, &second_root, &second_progress,
                                      &second_features);
    const bool same = valid && cJSON_Compare(first_features, second_features, true);
    cJSON_Delete(first_root);
    cJSON_Delete(second_root);
    return same;
}

game_save_choice_t game_save_policy_decide(
    const char *local_document, const char *remote_document) {
    cJSON *local_root = NULL;
    cJSON *remote_root = NULL;
    game_save_progress_t local;
    game_save_progress_t remote;
    const cJSON *local_features = NULL;
    const cJSON *remote_features = NULL;
    const bool valid = parse_progress(local_document, &local_root, &local,
                                      &local_features) &&
                       parse_progress(remote_document, &remote_root, &remote,
                                      &remote_features);
    if (!valid) {
        cJSON_Delete(local_root);
        cJSON_Delete(remote_root);
        return GAME_SAVE_ASK;
    }
    if (cJSON_Compare(local_features, remote_features, true)) {
        const game_save_choice_t result =
            remote.playtime_ms > local.playtime_ms
                ? GAME_SAVE_KEEP_REMOTE
                : GAME_SAVE_KEEP_LOCAL;
        cJSON_Delete(local_root);
        cJSON_Delete(remote_root);
        return result;
    }
    const bool local_dominates =
        local.hero_level >= remote.hero_level &&
        local.tutorial_done >= remote.tutorial_done &&
        (local.hero_level > remote.hero_level ||
         local.tutorial_done > remote.tutorial_done);
    const bool remote_dominates =
        remote.hero_level >= local.hero_level &&
        remote.tutorial_done >= local.tutorial_done &&
        (remote.hero_level > local.hero_level ||
         remote.tutorial_done > local.tutorial_done);
    game_save_choice_t result = GAME_SAVE_ASK;
    if (local_dominates) {
        result = GAME_SAVE_KEEP_LOCAL;
    } else if (remote_dominates) {
        result = GAME_SAVE_KEEP_REMOTE;
    } else if (local.hero_level == remote.hero_level &&
               local.tutorial_done == remote.tutorial_done &&
               local.playtime_ms > 0 && remote.playtime_ms > 0) {
        if (local.playtime_ms > remote.playtime_ms) {
            result = GAME_SAVE_KEEP_LOCAL;
        } else if (remote.playtime_ms > local.playtime_ms) {
            result = GAME_SAVE_KEEP_REMOTE;
        }
    }
    cJSON_Delete(local_root);
    cJSON_Delete(remote_root);
    return result;
}
