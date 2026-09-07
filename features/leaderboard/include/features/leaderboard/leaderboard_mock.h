#ifndef FEATURES_LEADERBOARD_MOCK_H
#define FEATURES_LEADERBOARD_MOCK_H

#include "features/leaderboard/leaderboard.h"

/* Deterministic in-process backend for tests and local development. The
 * state struct is the backend userdata; tests script its answers and read
 * its counters. */

typedef struct {
    bool is_submit;
    leaderboard_board_t board;
    leaderboard_scope_t scope;
} leaderboard_mock_pending_t;

typedef struct {
    leaderboard_caps_t caps; /* answered for every board */
    leaderboard_result_t submit_result;
    leaderboard_result_t fetch_result;
    bool defer; /* answer on update() instead of inside the call */
    bool init_fails;
    bool open_fails;
    int init_calls;
    int submit_calls;
    int fetch_calls;
    int open_calls;
    int update_calls;
    int destroy_calls;
    leaderboard_board_t last_submit_board;
    leaderboard_scope_t last_submit_scope;
    uint32_t last_submit_value;
    char last_submit_extra[LEADERBOARD_EXTRA_MAX];
    /* The page every fetch answers; with row_count 0 the mock synthesises rows. */
    leaderboard_row_t rows[LEADERBOARD_TOP_MAX];
    int row_count;
    bool has_player;
    int player_place;
    uint32_t player_value;
    char day[9]; /* "YYYYMMDD" answered for the day scope; "" for none */
    leaderboard_mock_pending_t pending[LEADERBOARD_MAX_BOARDS * LEADERBOARD_SCOPE_COUNT * 2];
    int pending_count;
} leaderboard_mock_t;

/* Full capabilities, OK answers, synthesised rows, immediate completion. */
void leaderboard_mock_defaults(leaderboard_mock_t *mock);
const leaderboard_backend_t *leaderboard_mock_backend(void);

#endif /* FEATURES_LEADERBOARD_MOCK_H */
