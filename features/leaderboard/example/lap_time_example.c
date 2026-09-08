#include "lap_time_example.h"

#include "game_leaderboards.h"

static leaderboard_board_t lap_board(void) {
    return leaderboard_board(GAME_LEADERBOARD_LAP_TIME);
}

void lap_time_example_init(leaderboard_host_t host, const leaderboard_backend_t *backend, void *backend_userdata) {
    const leaderboard_config_t config = {
        .host = host,
        .backend = backend,
        .backend_userdata = backend_userdata,
        .boards = GAME_LEADERBOARD_BOARDS,
        .board_count = GAME_LEADERBOARD_BOARD_COUNT,
    };
    leaderboard_init(&config);
}

void lap_time_example_update(void) {
    leaderboard_update();
}

void lap_time_example_shutdown(void) {
    leaderboard_shutdown();
}

bool lap_time_example_submit_ms(uint32_t milliseconds) {
    /* An unfinished lap must not become the unbeatable zero-time record. */
    if (milliseconds == 0) {
        return false;
    }
    leaderboard_submit(lap_board(), LEADERBOARD_SCOPE_ALL_TIME, milliseconds, NULL);
    return true;
}

void lap_time_example_open(void) {
    leaderboard_refresh_now(lap_board());
}

void lap_time_example_snapshot(lap_time_example_snapshot_t *out) {
    const leaderboard_board_t board = lap_board();
    leaderboard_view_get(board, LEADERBOARD_SCOPE_ALL_TIME, &out->view);
    out->ui = leaderboard_ui_state(board);
}
