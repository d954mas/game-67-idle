/* game_save_devapi.c — hand-written universal DevAPI dispatch over the fragment
   registry (§A5). Replaces the transitional generated <id>_state_devapi.c: the 7
   game.state.* commands route by the HEAD of the path to the owning fragment's
   vtable (get_path_json/set_path_json/to_json/schema_json) and to the shell
   (save/load/reset). No per-field metadata, no game_state.h — universal over the
   11-member GameSaveFragment descriptor. Dev-only, single thread.

   Compiled ONLY under GAME_DEVAPI_ENABLED (CMake target_sources guard), exactly
   like the generated devapi it replaces; native-debug/release never pull it in. */

#if NT_DEVAPI_ENABLED

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "devapi/nt_devapi.h"
#include "game_save.h"

/* Dev-only, single-threaded: one static buffer for dynamic error messages the
   engine reads after the handler returns (err->message must outlive the call).
   Ported 1:1 from the transitional devapi — already universal, no fragment name. */
static char s_state_err[256];

static bool state_fail(nt_devapi_error *err, const char *code, const char *message) {
    (void)snprintf(s_state_err, sizeof(s_state_err), "%s", message);
    err->code = code;
    err->message = s_state_err;
    return false;
}

/* err already carries a message snprintf'd into s_state_err by a fragment call. */
static bool state_fail_buf(nt_devapi_error *err, const char *code) {
    err->code = code;
    err->message = s_state_err;
    return false;
}

/* Splits "head.sub.sub" -> fragment by head + remainder sub ("" when head is the
   whole path). head_buf is the caller's local buffer. Returns NULL when the
   fragment is unknown (caller emits "unknown fragment") or the head overflows. */
static const GameSaveFragment *route_path(const char *path, char *head_buf, size_t head_cap,
                                          const char **out_sub) {
    const char *dot = strchr(path, '.');
    const size_t head_len = dot ? (size_t)(dot - path) : strlen(path);
    if (head_len >= head_cap) {
        return NULL; /* buffer guard */
    }
    memcpy(head_buf, path, head_len);
    head_buf[head_len] = '\0';
    *out_sub = dot ? dot + 1 : ""; /* "" == the whole fragment */
    return game_save_find_fragment(head_buf);
}

/* { <frag.id>: frag->to_json(), ... [, "orphans": {<id>: subtree, ...}] } over registered
   fragments; NO "v" (read view, not a save — §8). Retained orphan blobs (§14 п.16) ride in
   a SEPARATE "orphans" section, appended after the live fragments and omitted entirely when
   there are none (Q1, lead 2026-07-07) — a healthy response stays byte-identical to before.
   Key-collision note: a registered fragment whose id were literally "orphans" would shadow
   this section, but "orphans" is reserved here for the orphan set (fragment ids are payload
   keys, not "orphans"). Ownership: s_orphans is owned by game_save, so only cJSON_Duplicate
   copies go out; a failed Duplicate skips that one orphan rather than dropping the aggregate. */
static cJSON *build_aggregate(void) {
    cJSON *agg = cJSON_CreateObject();
    if (!agg) {
        return NULL;
    }
    const int n = game_save_fragment_count();
    for (int i = 0; i < n; i++) {
        const GameSaveFragment *f = game_save_fragment_at(i);
        cJSON *payload = f->to_json ? f->to_json() : NULL;
        if (!payload) {
            payload = cJSON_CreateObject();
        }
        cJSON_AddItemToObject(agg, f->id, payload);
    }
    const int orphan_n = game_save_orphan_count();
    if (orphan_n > 0) {
        cJSON *orphans = cJSON_CreateObject();
        if (orphans) {
            for (int i = 0; i < orphan_n; i++) {
                const char *id = NULL;
                const cJSON *sub = game_save_orphan_at(i, &id);
                if (!sub || !id) {
                    continue;
                }
                cJSON *dup = cJSON_Duplicate(sub, true); /* copy only; s_orphans keeps ownership */
                if (dup) {
                    cJSON_AddItemToObject(orphans, id, dup);
                }
            }
            cJSON_AddItemToObject(agg, "orphans", orphans);
        }
    }
    return agg;
}

static const char *load_status_string(game_save_load_status_t status) {
    switch (status) {
        case GAME_SAVE_LOAD_FRESH:
            return "fresh";
        case GAME_SAVE_LOAD_LOADED:
            return "loaded";
        case GAME_SAVE_LOAD_RECOVERED_BAK:
            return "recovered_bak";
        case GAME_SAVE_LOAD_CORRUPT_RESET:
            return "corrupt_reset";
        case GAME_SAVE_LOAD_NEWER:
            return "newer";
    }
    return "unknown";
}

/* ---- 7 handlers. error.code is FROZEN to "bad_params"/"internal" (§MED-3);
   descriptive strings live only in err->message. ---- */

static bool ep_state_schema(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)user;
    (void)err;
    const int n = game_save_fragment_count();
    for (int i = 0; i < n; i++) {
        const GameSaveFragment *f = game_save_fragment_at(i);
        if (!f->schema_json) {
            continue;
        }
        cJSON *schema = f->schema_json();
        if (schema) {
            cJSON_AddItemToObject(result_obj, f->id, schema);
        }
    }
    return true; /* aggregate { <fragment>: schema, ... } */
}

static bool ep_state_get(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
    const cJSON *path = cJSON_GetObjectItemCaseSensitive(params, "path");
    const char *state_path = cJSON_IsString(path) ? path->valuestring : "";
    cJSON *value = NULL;
    if (state_path[0] == '\0') {
        value = build_aggregate(); /* get "" -> { <fragment>: payload, ... } */
        if (!value) {
            return state_fail(err, "internal", "failed to build state aggregate");
        }
    } else {
        char head[64];
        const char *sub = "";
        const GameSaveFragment *f = route_path(state_path, head, sizeof head, &sub);
        if (!f) {
            return state_fail(err, "bad_params", "unknown fragment");
        }
        if (sub[0] == '\0') {
            value = f->to_json ? f->to_json() : NULL; /* whole fragment */
            if (!value) {
                return state_fail(err, "internal", "failed to build fragment json");
            }
        } else {
            if (!f->get_path_json) {
                return state_fail(err, "bad_params", "fragment does not support path reads");
            }
            value = f->get_path_json(sub, s_state_err, (int)sizeof s_state_err);
            if (!value) {
                return state_fail_buf(err, "bad_params"); /* message already in s_state_err */
            }
        }
    }
    cJSON_AddItemToObject(result_obj, "path", cJSON_CreateString(state_path));
    cJSON_AddItemToObject(result_obj, "value", value); /* owned -> result_obj */
    return true;
}

static bool ep_state_set(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
    const cJSON *path = cJSON_GetObjectItemCaseSensitive(params, "path");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, "value");
    if (!cJSON_IsString(path) || path->valuestring[0] == '\0' || !value) {
        return state_fail(err, "bad_params", "path and value are required");
    }
    char head[64];
    const char *sub = "";
    const GameSaveFragment *f = route_path(path->valuestring, head, sizeof head, &sub);
    if (!f) {
        return state_fail(err, "bad_params", "unknown fragment");
    }
    if (sub[0] == '\0') {
        return state_fail(err, "bad_params", "set requires a sub-path");
    }
    if (!f->set_path_json) {
        return state_fail(err, "bad_params", "read-only fragment");
    }
    if (!f->set_path_json(sub, value, s_state_err, (int)sizeof s_state_err)) {
        return state_fail_buf(err, "bad_params");
    }
    game_save_mark_dirty();
    /* Echo the stored value. The fresh get_path_json result is already owned, so
       it goes straight into result_obj; only the params-owned input `value` needs
       cJSON_Duplicate — handing `value` itself to result_obj double-frees (§MED-4). */
    cJSON *echo = f->get_path_json ? f->get_path_json(sub, s_state_err, (int)sizeof s_state_err) : NULL;
    if (!echo) {
        echo = cJSON_Duplicate(value, true);
    }
    cJSON_AddItemToObject(result_obj, "path", cJSON_CreateString(path->valuestring));
    cJSON_AddItemToObject(result_obj, "value", echo);
    return true;
}

/* Per-fragment atomic patch (§14 п.7 / §MED-2): group values by owning fragment,
   snapshot the fragment BEFORE applying its group, restore the whole group via
   from_json(snapshot) on any key failure. No cross-fragment rollback. */
static bool ep_state_patch(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)user;
    const cJSON *values = cJSON_GetObjectItemCaseSensitive(params, "values");
    if (!cJSON_IsObject(values)) {
        return state_fail(err, "bad_params", "values object is required");
    }
    cJSON *results = cJSON_AddObjectToObject(result_obj, "results");
    bool any_ok = false;
    const int n = game_save_fragment_count();
    for (int i = 0; i < n; i++) { /* outer loop = fragment => the group is atomic */
        const GameSaveFragment *f = game_save_fragment_at(i);
        bool group_has = false;
        bool group_ok = true;
        cJSON *snapshot = NULL;
        for (const cJSON *m = values->child; m; m = m->next) {
            char head[64];
            const char *sub = "";
            if (route_path(m->string, head, sizeof head, &sub) != f) {
                continue;
            }
            if (!group_has) { /* first key of the group: snapshot BEFORE any mutation */
                group_has = true;
                snapshot = f->to_json ? f->to_json() : NULL;
            }
            if (sub[0] == '\0' || !f->set_path_json ||
                !f->set_path_json(sub, m, s_state_err, (int)sizeof s_state_err)) {
                group_ok = false; /* key failure => roll the whole group back */
                break;
            }
        }
        if (group_has && !group_ok && snapshot && f->from_json) {
            (void)f->from_json(snapshot, s_state_err, (int)sizeof s_state_err); /* restore */
        }
        cJSON_Delete(snapshot); /* snapshot is always freed */
        if (group_has) {        /* per-key result: the whole group takes group_ok */
            for (const cJSON *m = values->child; m; m = m->next) {
                char head[64];
                const char *sub = "";
                if (route_path(m->string, head, sizeof head, &sub) != f) {
                    continue;
                }
                cJSON_AddBoolToObject(results, m->string, group_ok);
            }
            any_ok = any_ok || group_ok;
        }
    }
    for (const cJSON *m = values->child; m; m = m->next) { /* unknown head => false */
        if (!cJSON_GetObjectItemCaseSensitive(results, m->string)) {
            cJSON_AddBoolToObject(results, m->string, false);
        }
    }
    if (any_ok) {
        game_save_mark_dirty();
    }
    return true; /* patch never fails the channel; result is per-key */
}

static bool ep_state_save(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)user;
    if (!game_save_flush(s_state_err, (int)sizeof s_state_err)) {
        return state_fail_buf(err, "internal");
    }
    cJSON_AddBoolToObject(result_obj, "saved", true);
    return true;
}

static bool ep_state_load(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)user;
    game_save_load_result_t result;
    game_save_load(&result);
    cJSON *value = build_aggregate();
    if (!value) {
        return state_fail(err, "internal", "failed to build state aggregate");
    }
    cJSON_AddItemToObject(result_obj, "status", cJSON_CreateString(load_status_string(result.status)));
    cJSON_AddItemToObject(result_obj, "value", value);
    return true;
}

static bool ep_state_reset(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)user;
    /* Р10: reset all fragments + on_new_game (starting content) + save + resume autosave. */
    if (!game_save_new_game(s_state_err, (int)sizeof s_state_err)) {
        return state_fail_buf(err, "internal");
    }
    cJSON_AddBoolToObject(result_obj, "reset", true);
    return true;
}

void game_save_register_devapi(void) {
    static const nt_devapi_command_desc descs[] = {
        {"game.state.schema", "game", "Return per-fragment state schemas.", "none", "{<fragment>: schema}", "immediate", "none"},
        {"game.state.get", "game", "Get state by path (\"\"=all fragments).", "path", "path, value", "immediate", "none"},
        {"game.state.set", "game", "Set a state value by path (<fragment>.<sub>).", "path, value", "path, value", "immediate", "mutates state"},
        {"game.state.patch", "game", "Patch multiple paths.", "values", "results", "immediate", "mutates state"},
        {"game.state.save", "game", "Flush state to storage.", "none", "saved", "immediate", "writes file"},
        {"game.state.load", "game", "Reload state from storage.", "none", "status, value", "immediate", "mutates state"},
        {"game.state.reset", "game", "New game: reset all fragments + starting content.", "none", "reset", "immediate", "mutates state"},
    };
    const nt_devapi_handler_fn fns[] = {
        ep_state_schema, ep_state_get, ep_state_set, ep_state_patch,
        ep_state_save, ep_state_load, ep_state_reset,
    };
    for (size_t i = 0; i < sizeof(fns) / sizeof(fns[0]); ++i) {
        (void)nt_devapi_register(&descs[i], fns[i], NULL);
    }
}

#endif /* NT_DEVAPI_ENABLED */
