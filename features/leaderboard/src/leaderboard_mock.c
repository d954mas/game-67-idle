#include "features/leaderboard/leaderboard_mock.h"

#include <stdio.h>
#include <string.h>

/* Rows a fresh mock answers with, so a screen has something to draw before
 * any real backend exists. Their count and values are fixture, not design. */
#define MOCK_SYNTH_ROWS 10

void leaderboard_mock_defaults(leaderboard_mock_t *mock) {
    memset(mock, 0, sizeof *mock);
    mock->caps.can_read = true;
    mock->caps.can_write = true;
    mock->caps.scopes = (1u << LEADERBOARD_SCOPE_ALL_TIME) | (1u << LEADERBOARD_SCOPE_UTC_DAY);
}

static void synthesise_rows(leaderboard_mock_t *mock) {
    for (int i = 0; i < MOCK_SYNTH_ROWS; i++) {
        leaderboard_row_t *row = &mock->rows[i];
        memset(row, 0, sizeof *row);
        row->value = (uint32_t)(1000 - 100 * i);
        row->place = i + 1;
        snprintf(row->name, sizeof row->name, "player %d", i + 1);
    }
    mock->row_count = MOCK_SYNTH_ROWS;
}

static void complete(leaderboard_mock_t *mock, bool is_submit, leaderboard_board_t board,
                     leaderboard_scope_t scope) {
    if (is_submit) {
        leaderboard_backend_complete_submit(board, scope, mock->submit_result);
        return;
    }
    if (mock->row_count == 0) {
        synthesise_rows(mock);
    }
    const leaderboard_page_t page = {
        .top = mock->rows,
        .top_count = mock->row_count,
        .around = NULL,
        .around_count = 0,
        .has_player = mock->has_player,
        .player_place = mock->player_place,
        .player_value = mock->player_value,
        .day = scope == LEADERBOARD_SCOPE_UTC_DAY && mock->day[0] != '\0' ? mock->day : NULL,
    };
    leaderboard_backend_complete_fetch(board, scope, &page, mock->fetch_result);
}

static bool answer(leaderboard_mock_t *mock, bool is_submit, leaderboard_board_t board,
                   leaderboard_scope_t scope) {
    if (!mock->defer) {
        complete(mock, is_submit, board, scope);
        return true;
    }
    if (mock->pending_count >= (int)(sizeof mock->pending / sizeof mock->pending[0])) {
        return false;
    }
    mock->pending[mock->pending_count++] = (leaderboard_mock_pending_t){is_submit, board, scope};
    return true;
}

static leaderboard_caps_t mock_caps(leaderboard_board_t board, void *ud) {
    (void)board;
    return ((leaderboard_mock_t *)ud)->caps;
}

static bool mock_init(void *ud) {
    leaderboard_mock_t *mock = ud;
    mock->init_calls++;
    return !mock->init_fails;
}

static bool mock_submit(leaderboard_board_t board, leaderboard_scope_t scope, uint32_t value,
                        const char *extra, void *ud) {
    leaderboard_mock_t *mock = ud;
    mock->submit_calls++;
    mock->last_submit_board = board;
    mock->last_submit_scope = scope;
    mock->last_submit_value = value;
    snprintf(mock->last_submit_extra, sizeof mock->last_submit_extra, "%s", extra != NULL ? extra : "");
    return answer(mock, true, board, scope);
}

static bool mock_fetch(leaderboard_board_t board, leaderboard_scope_t scope, void *ud) {
    leaderboard_mock_t *mock = ud;
    mock->fetch_calls++;
    return answer(mock, false, board, scope);
}

static bool mock_open_native(leaderboard_board_t board, void *ud) {
    (void)board;
    leaderboard_mock_t *mock = ud;
    mock->open_calls++;
    return !mock->open_fails;
}

static void mock_update(void *ud) {
    leaderboard_mock_t *mock = ud;
    mock->update_calls++;
    /* A completion may queue a follow-up request; it answers on the next pump. */
    leaderboard_mock_pending_t batch[sizeof mock->pending / sizeof mock->pending[0]];
    const int count = mock->pending_count;
    memcpy(batch, mock->pending, sizeof batch);
    mock->pending_count = 0;
    for (int i = 0; i < count; i++) {
        complete(mock, batch[i].is_submit, batch[i].board, batch[i].scope);
    }
}

static void mock_destroy(void *ud) {
    ((leaderboard_mock_t *)ud)->destroy_calls++;
}

const leaderboard_backend_t *leaderboard_mock_backend(void) {
    static const leaderboard_backend_t backend = {
        .caps = mock_caps,
        .init = mock_init,
        .submit = mock_submit,
        .fetch = mock_fetch,
        .open_native = mock_open_native,
        .update = mock_update,
        .destroy = mock_destroy,
    };
    return &backend;
}
