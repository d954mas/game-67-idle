#include "lap_time_example.h"

#include "features/leaderboard/leaderboard_mock.h"

#include <string.h>

static bool save_load(const char *key, char *out, size_t out_size, void *userdata) {
    (void)userdata;
    const char *id = "12345678-1234-4234-8234-123456789abc";
    if (strcmp(key, "lb.id") != 0 || out_size <= strlen(id)) {
        return false;
    }
    memcpy(out, id, strlen(id) + 1);
    return true;
}

static void save_store(const char *key, const char *value, void *userdata) {
    (void)key;
    (void)value;
    (void)userdata;
}

int main(void) {
    leaderboard_mock_t mock;
    leaderboard_mock_defaults(&mock);
    mock.row_count = 2;
    mock.rows[0] = (leaderboard_row_t){.value = 41000, .place = 1};
    mock.rows[1] = (leaderboard_row_t){.value = 45000, .place = 2};
    const leaderboard_host_t host = {.load = save_load, .store = save_store};
    lap_time_example_init(host, leaderboard_mock_backend(), &mock);
    if (lap_time_example_submit_ms(0) || mock.submit_calls != 0) {
        return 1;
    }
    if (!lap_time_example_submit_ms(45000) || !lap_time_example_submit_ms(42000) ||
        !lap_time_example_submit_ms(46000) || mock.last_submit_value != 42000) {
        return 2;
    }
    lap_time_example_snapshot_t snapshot;
    lap_time_example_snapshot(&snapshot);
    if (snapshot.view.loaded || !snapshot.ui.show_launcher || snapshot.ui.tab_count != 1) {
        return 3;
    }
    lap_time_example_open();
    lap_time_example_update();
    lap_time_example_snapshot(&snapshot);
    if (!snapshot.view.loaded || snapshot.ui.loading || snapshot.view.top_count != 2 ||
        snapshot.view.top[0].value >= snapshot.view.top[1].value) {
        return 4;
    }
    lap_time_example_shutdown();
    return mock.destroy_calls == 1 ? 0 : 5;
}
