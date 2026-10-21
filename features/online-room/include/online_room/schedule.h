#ifndef ONLINE_ROOM_SCHEDULE_H
#define ONLINE_ROOM_SCHEDULE_H

#include <stdint.h>

// The room's clock: tick n is due at start + n / hz, in seconds on the
// caller's monotonic clock.
typedef struct online_tick_schedule {
    double start;
    uint32_t hz;
    uint64_t next;    // the next tick to run
    uint64_t late;    // ticks run a tick or more after their time
    uint64_t dropped; // ticks given up after falling behind
} online_tick_schedule;

void online_schedule_init(online_tick_schedule *schedule, double now, uint32_t hz);
double online_schedule_time(const online_tick_schedule *schedule, uint64_t tick);
// The ticks due at `now`, at most `catch_up` of them from `*first` on; a
// schedule further behind gives up the rest and starts over from now.
uint32_t online_schedule_take(online_tick_schedule *schedule, double now, uint32_t catch_up, uint64_t *first);
// Seconds until the next tick is due.
double online_schedule_delay(const online_tick_schedule *schedule, double now);

#endif
