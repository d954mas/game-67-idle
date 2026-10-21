#ifndef ONLINE_ROOM_PERF_H
#define ONLINE_ROOM_PERF_H

// The room's performance window: what one status period cost, for the
// metaserver's admin page and for load runs (features/metaserver/testdata/room_perf_v1.json
// is the shape). Recording is plain arithmetic on fixed arrays: no
// allocation, formatting or lock on the tick path, and no clock read beyond
// the monotonic ones that time each stage. The host takes the window when its
// status is handed to the metaserver transport, and only then formats
// anything; a status that then fails loses its window.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "net_ws_server.h"

// Log2 buckets of microseconds: bucket 0 holds under 16 us, bucket i (1..16)
// holds [2^(3+i), 2^(4+i)) us, the last one 2^20 us (~1 s) and more.
#define ONLINE_PERF_BUCKETS 18U

typedef struct online_perf_hist {
    uint32_t counts[ONLINE_PERF_BUCKETS];
    uint32_t max_us;
    uint64_t sum_us;
} online_perf_hist;

uint32_t online_perf_bucket(uint32_t us);
void online_perf_hist_add(online_perf_hist *hist, uint32_t us);

typedef enum online_perf_stage {
    ONLINE_PERF_LATE,       // a tick's start after its scheduled time
    ONLINE_PERF_STEP,       // the host tick: inputs, simulation, profiles
    ONLINE_PERF_REPLICATE,  // building every seat's packet and writing it to its socket
    ONLINE_PERF_META,       // metaserver HTTP pump and status
    ONLINE_PERF_INPUT_WAIT, // how long each move taken this tick sat in the room
    ONLINE_PERF_STAGES
} online_perf_stage;

// Close reasons as net_ws_close_reason_t, and one for a reason this build does not know.
#define ONLINE_PERF_CLOSES 6U

// The machine's counters from /proc: jiffies by CPU state, and every
// interface but loopback. Absent where the platform has no /proc. The window
// sends their growth, and the interfaces' byte counters whole as well, from
// which the metaserver keeps the machine's traffic by day.
typedef struct online_perf_machine {
    uint64_t user, nice, system, idle, iowait, irq, softirq, steal;
    uint64_t rx_bytes, tx_bytes, rx_packets, tx_packets;
} online_perf_machine;

typedef struct online_perf_process {
    uint64_t user_us, system_us;
    uint64_t rss_kb;
} online_perf_process;

// Cumulative counters the host reads when it takes the window; the window
// sends their growth since the previous take.
typedef struct online_perf_totals {
    net_ws_server_stats_t net;
    uint64_t ticks, late, dropped;
    uint64_t closes[ONLINE_PERF_CLOSES];
    uint64_t malformed; // commands that did not decode, every seat
    uint64_t underruns, overruns, trimmed;
    uint64_t lost_edges, malformed_edges; // a game whose moves carry no edges leaves them 0
    online_perf_process process;
    bool has_process;
    online_perf_machine machine;
    bool has_machine;
    // The machine's configured rooms, ids plan_room_base + 1 .. + plan_rooms,
    // so the metaserver tells a room that should answer from one switched off;
    // 0 rooms: the room runs outside a machine's plan.
    uint32_t plan_rooms, plan_room_base;
} online_perf_totals;

// The clients' network reports the window heard; the game fills it from its
// own report message.
typedef struct online_perf_clients {
    uint32_t reports;
    online_perf_hist rtt; // the reported round trips, microseconds; reports without one left out
    uint32_t rtt_max_ms;
    uint64_t corrections, snaps, stalls;
    uint64_t fps_sum;
    uint32_t fps_low; // reports under 30 frames a second
    uint32_t reconnects;
    uint32_t web, touch, bench; // reports by what the client runs on
} online_perf_clients;

typedef struct online_perf_window {
    online_perf_hist stages[ONLINE_PERF_STAGES];
    uint32_t pumps;        // loop passes: each one a wake of the room
    uint64_t seated_ticks; // seated players summed over ticks: player-seconds times the tick rate
    // Replication.
    uint64_t packets, bytes, row_bytes;
    uint64_t reliable, cosmetic, cosmetic_dropped;
    uint64_t rows_sent, rows_deferred;
    uint32_t rows_waited_most;
    uint64_t skipped, refused;
    uint32_t outbox_peak;
    int32_t credit_min, credit_max;
    bool credit_seen;
    uint32_t input_queue_peak;
    uint32_t net_queue_peak; // peaks taken from the server during the window
    online_perf_clients clients;
} online_perf_window;

void online_perf_window_reset(online_perf_window *window);
void online_perf_credit(online_perf_window *window, int32_t credit);

// The window as the status's "perf" object: `now` and `before` are the
// cumulative counters at this take and at the previous one, `window_ms` the
// time between them. Returns the bytes written, 0 when it does not fit.
// The busiest window the tests build, the machine plan included, is 2061 bytes.
#define ONLINE_PERF_JSON_MAX 2112U
size_t online_perf_json(const online_perf_window *window, const online_perf_totals *now, const online_perf_totals *before,
    uint32_t window_ms, char *out, size_t cap);

#endif
