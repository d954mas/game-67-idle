#include "features/leaderboard/leaderboard_portal.h"

#include "features/platform_sdk/platform_sdk.h"

#include <stdio.h>
#include <string.h>

/* The portal answers arrive keyed by the portal's board id; the facade keys
 * everything by board index, so each completion is mapped back through the
 * board table. Rows are copied into fixed storage before the facade sees them
 * because the platform page is borrowed for the callback only. */

static struct {
    bool listening;
    bool was_ready;
    bool was_authorized;
    leaderboard_row_t top[LEADERBOARD_TOP_MAX];
    leaderboard_row_t around[LEADERBOARD_AROUND_MAX];
} g_portal;

static const char *portal_id_of(leaderboard_board_t board) {
    const leaderboard_board_def_t *def = leaderboard_board_def(board);
    if (def == NULL || def->portal_id == NULL || def->portal_id[0] == '\0') {
        return NULL;
    }
    return def->portal_id;
}

static leaderboard_board_t board_of(const char *portal_id) {
    const int count = leaderboard_board_count();
    for (int i = 0; i < count; i++) {
        const leaderboard_board_t board = {(uint8_t)i};
        const char *id = portal_id_of(board);
        if (id != NULL && portal_id != NULL && strcmp(id, portal_id) == 0) {
            return board;
        }
    }
    return (leaderboard_board_t){LEADERBOARD_INVALID_BOARD};
}

static leaderboard_result_t result_of(platform_sdk_leaderboard_status_t status) {
    switch (status) {
    case PLATFORM_SDK_LEADERBOARD_OK:
        return LEADERBOARD_RESULT_OK;
    case PLATFORM_SDK_LEADERBOARD_UNSUPPORTED:
        return LEADERBOARD_RESULT_UNSUPPORTED;
    case PLATFORM_SDK_LEADERBOARD_NEEDS_LOGIN:
        return LEADERBOARD_RESULT_NEEDS_LOGIN;
    case PLATFORM_SDK_LEADERBOARD_RATE_LIMITED:
        return LEADERBOARD_RESULT_RATE_LIMITED;
    case PLATFORM_SDK_LEADERBOARD_FAILED:
    default:
        return LEADERBOARD_RESULT_FAILED;
    }
}

/* A payload that does not fit is dropped whole: a cut "k=v;" string is
 * garbage to the codec, and the row is still a row without it. */
static void copy_or_drop(char *dst, size_t cap, const char *src) {
    if (src != NULL && strlen(src) < cap) {
        memcpy(dst, src, strlen(src) + 1u);
    } else {
        dst[0] = '\0';
    }
}

static int copy_rows(leaderboard_row_t *dst, int cap, const platform_sdk_leaderboard_entry_t *src, int count) {
    int copied = 0;
    if (src == NULL) {
        return 0;
    }
    for (int i = 0; i < count && copied < cap; i++) {
        leaderboard_row_t *row = &dst[copied++];
        memset(row, 0, sizeof *row);
        row->value = src[i].value;
        row->place = src[i].rank;
        row->you = src[i].you;
        snprintf(row->name, sizeof row->name, "%s", src[i].name != NULL ? src[i].name : "");
        copy_or_drop(row->avatar, sizeof row->avatar, src[i].avatar_url);
        copy_or_drop(row->extra, sizeof row->extra, src[i].extra);
    }
    return copied;
}

static void on_submit_done(const char *board_id, int32_t scope, platform_sdk_leaderboard_status_t status,
                           void *userdata) {
    (void)userdata;
    const leaderboard_board_t board = board_of(board_id);
    if (board.index == LEADERBOARD_INVALID_BOARD) {
        return;
    }
    leaderboard_backend_complete_submit(board, (leaderboard_scope_t)scope, result_of(status));
}

static void on_fetch_done(const char *board_id, int32_t scope, platform_sdk_leaderboard_status_t status,
                          const platform_sdk_leaderboard_page_t *page, void *userdata) {
    (void)userdata;
    const leaderboard_board_t board = board_of(board_id);
    if (board.index == LEADERBOARD_INVALID_BOARD) {
        return;
    }
    if (status != PLATFORM_SDK_LEADERBOARD_OK || page == NULL) {
        leaderboard_backend_complete_fetch(board, (leaderboard_scope_t)scope, NULL, result_of(status));
        return;
    }
    const int top_count = copy_rows(g_portal.top, LEADERBOARD_TOP_MAX, page->top, page->top_count);
    const int around_count = copy_rows(g_portal.around, LEADERBOARD_AROUND_MAX, page->around, page->around_count);
    const leaderboard_page_t out = {
        .top = g_portal.top,
        .top_count = top_count,
        .around = around_count > 0 ? g_portal.around : NULL,
        .around_count = around_count,
        .has_player = page->has_player,
        .player_place = page->player_rank,
        .player_value = page->player_value,
        .day = NULL,
    };
    leaderboard_backend_complete_fetch(board, (leaderboard_scope_t)scope, &out, LEADERBOARD_RESULT_OK);
}

static leaderboard_caps_t portal_caps(leaderboard_board_t board, void *ud) {
    (void)ud;
    leaderboard_caps_t caps = {0};
    const char *portal_id = portal_id_of(board);
    if (portal_id == NULL) {
        return caps;
    }
    const platform_sdk_leaderboard_caps_t portal = platform_sdk_leaderboard_caps(portal_id);
    caps.can_read = portal.can_read;
    caps.can_write = portal.can_write;
    caps.needs_login = portal.needs_login;
    caps.native_popup = portal.native_popup;
    caps.scopes = 1u << LEADERBOARD_SCOPE_ALL_TIME;
    return caps;
}

static bool portal_init(void *ud) {
    (void)ud;
    const platform_sdk_leaderboard_listener_t listener = {
        .submit_done = on_submit_done,
        .fetch_done = on_fetch_done,
        .userdata = NULL,
    };
    platform_sdk_leaderboard_set_listener(&listener);
    g_portal.listening = true;
    g_portal.was_ready = platform_sdk_status() == PLATFORM_SDK_BOOT_READY;
    g_portal.was_authorized = platform_sdk_authorized();
    return true;
}

/* UNSUPPORTED is the one answer that latches, so only the platform's own
 * "no such API here" earns it. Anything else -- not ready yet, torn down --
 * never started and is retried on the next trigger. */
static bool start(leaderboard_board_t board, leaderboard_scope_t scope, bool is_submit,
                  platform_sdk_result_t result) {
    if (result == PLATFORM_SDK_RESULT_OK) {
        return true;
    }
    if (result == PLATFORM_SDK_RESULT_UNSUPPORTED) {
        if (is_submit) {
            leaderboard_backend_complete_submit(board, scope, LEADERBOARD_RESULT_UNSUPPORTED);
        } else {
            leaderboard_backend_complete_fetch(board, scope, NULL, LEADERBOARD_RESULT_UNSUPPORTED);
        }
        return true;
    }
    return false;
}

static bool portal_submit(leaderboard_board_t board, leaderboard_scope_t scope, uint32_t value,
                          const char *extra, void *ud) {
    (void)ud;
    const char *portal_id = portal_id_of(board);
    if (portal_id == NULL || scope != LEADERBOARD_SCOPE_ALL_TIME) {
        return false;
    }
    return start(board, scope, true, platform_sdk_leaderboard_submit(portal_id, (int32_t)scope, value, extra));
}

static bool portal_fetch(leaderboard_board_t board, leaderboard_scope_t scope, void *ud) {
    (void)ud;
    const char *portal_id = portal_id_of(board);
    if (portal_id == NULL || scope != LEADERBOARD_SCOPE_ALL_TIME) {
        return false;
    }
    return start(board, scope, false, platform_sdk_leaderboard_fetch(portal_id, (int32_t)scope));
}

static bool portal_open_native(leaderboard_board_t board, void *ud) {
    (void)ud;
    const char *portal_id = portal_id_of(board);
    return portal_id != NULL && platform_sdk_leaderboard_open(portal_id) == PLATFORM_SDK_RESULT_OK;
}

/* The facade owns no clock and re-sends only on a trigger; the portal coming
 * up, or the player logging in, is such a trigger and the facade cannot see
 * either, so this pump reports both as an auth change. */
static void portal_update(void *ud) {
    (void)ud;
    const bool ready = platform_sdk_status() == PLATFORM_SDK_BOOT_READY;
    const bool authorized = platform_sdk_authorized();
    const bool changed = (ready && !g_portal.was_ready) || (authorized != g_portal.was_authorized);
    g_portal.was_ready = ready;
    g_portal.was_authorized = authorized;
    if (changed) {
        leaderboard_backend_auth_changed();
    }
}

static void portal_destroy(void *ud) {
    (void)ud;
    if (g_portal.listening) {
        platform_sdk_leaderboard_set_listener(NULL);
    }
    memset(&g_portal, 0, sizeof g_portal);
}

const leaderboard_backend_t *leaderboard_portal_backend(void) {
    static const leaderboard_backend_t backend = {
        .caps = portal_caps,
        .init = portal_init,
        .submit = portal_submit,
        .fetch = portal_fetch,
        .open_native = portal_open_native,
        .update = portal_update,
        .destroy = portal_destroy,
    };
    return &backend;
}
