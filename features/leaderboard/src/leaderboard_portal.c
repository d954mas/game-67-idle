#include "features/leaderboard/leaderboard_portal.h"

/* Placeholder until the platform-sdk leaderboard entry points exist: every
 * board is unsupported, so a game wired to this backend simply shows no board. */

static leaderboard_caps_t portal_caps(leaderboard_board_t board, void *ud) {
    (void)board;
    (void)ud;
    return (leaderboard_caps_t){0};
}

static bool portal_init(void *ud) {
    (void)ud;
    return true;
}

static bool portal_submit(leaderboard_board_t board, leaderboard_scope_t scope, uint32_t value,
                          const char *extra, void *ud) {
    (void)value;
    (void)extra;
    (void)ud;
    leaderboard_backend_complete_submit(board, scope, LEADERBOARD_RESULT_UNSUPPORTED);
    return true;
}

static bool portal_fetch(leaderboard_board_t board, leaderboard_scope_t scope, void *ud) {
    (void)ud;
    leaderboard_backend_complete_fetch(board, scope, NULL, LEADERBOARD_RESULT_UNSUPPORTED);
    return true;
}

static bool portal_open_native(leaderboard_board_t board, void *ud) {
    (void)board;
    (void)ud;
    return false;
}

static void portal_update(void *ud) {
    (void)ud;
}

static void portal_destroy(void *ud) {
    (void)ud;
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
