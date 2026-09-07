#include "features/leaderboard/leaderboard_http.h"

/* Placeholder until the anonymous HTTP client moves in: every board is
 * unsupported, so a game wired to this backend simply shows no board. */

static leaderboard_caps_t http_caps(leaderboard_board_t board, void *ud) {
    (void)board;
    (void)ud;
    return (leaderboard_caps_t){0};
}

static bool http_init(void *ud) {
    (void)ud;
    return true;
}

static bool http_submit(leaderboard_board_t board, leaderboard_scope_t scope, uint32_t value,
                        const char *extra, void *ud) {
    (void)value;
    (void)extra;
    (void)ud;
    leaderboard_backend_complete_submit(board, scope, LEADERBOARD_RESULT_UNSUPPORTED);
    return true;
}

static bool http_fetch(leaderboard_board_t board, leaderboard_scope_t scope, void *ud) {
    (void)ud;
    leaderboard_backend_complete_fetch(board, scope, NULL, LEADERBOARD_RESULT_UNSUPPORTED);
    return true;
}

static bool http_open_native(leaderboard_board_t board, void *ud) {
    (void)board;
    (void)ud;
    return false;
}

static void http_update(void *ud) {
    (void)ud;
}

static void http_destroy(void *ud) {
    (void)ud;
}

const leaderboard_backend_t *leaderboard_http_backend(void) {
    static const leaderboard_backend_t backend = {
        .caps = http_caps,
        .init = http_init,
        .submit = http_submit,
        .fetch = http_fetch,
        .open_native = http_open_native,
        .update = http_update,
        .destroy = http_destroy,
    };
    return &backend;
}
