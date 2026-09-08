#ifndef LEADERBOARD_LAP_TIME_EXAMPLE_H
#define LEADERBOARD_LAP_TIME_EXAMPLE_H

#include "features/leaderboard/leaderboard.h"

typedef struct {
    leaderboard_view_t view;
    leaderboard_ui_state_t ui;
} lap_time_example_snapshot_t;

void lap_time_example_init(leaderboard_host_t host, const leaderboard_backend_t *backend, void *backend_userdata);
void lap_time_example_update(void);
void lap_time_example_shutdown(void);
bool lap_time_example_submit_ms(uint32_t milliseconds);
void lap_time_example_open(void);
void lap_time_example_snapshot(lap_time_example_snapshot_t *out);

#endif
