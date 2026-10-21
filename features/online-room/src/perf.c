#include "online_room/perf.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

uint32_t online_perf_bucket(uint32_t us) {
    uint32_t bucket = 0U;
    for (uint32_t bound = 16U; bucket < ONLINE_PERF_BUCKETS - 1U && us >= bound; bound <<= 1U) { bucket += 1U; }
    return bucket;
}

void online_perf_hist_add(online_perf_hist *hist, uint32_t us) {
    hist->counts[online_perf_bucket(us)] += 1U;
    hist->sum_us += us;
    if (us > hist->max_us) { hist->max_us = us; }
}

void online_perf_window_reset(online_perf_window *window) { memset(window, 0, sizeof *window); }

void online_perf_credit(online_perf_window *window, int32_t credit) {
    if (!window->credit_seen || credit < window->credit_min) { window->credit_min = credit; }
    if (!window->credit_seen || credit > window->credit_max) { window->credit_max = credit; }
    window->credit_seen = true;
}

typedef struct writer {
    char *out;
    size_t cap, used;
    bool full;
} writer;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
static void put(writer *w, const char *format, ...) {
    if (w->full) { return; }
    va_list args;
    va_start(args, format);
    const int length = vsnprintf(w->out + w->used, w->cap - w->used, format, args);
    va_end(args);
    if (length < 0 || (size_t)length >= w->cap - w->used) {
        w->full = true;
        return;
    }
    w->used += (size_t)length;
}

// Growth of a counter that only grows; a counter that went back (a seat's
// telemetry restarting with a new occupant) counts from zero.
static unsigned long long grew(uint64_t now, uint64_t before) {
    return (unsigned long long)(now >= before ? now - before : now);
}

static const char *const k_stages[ONLINE_PERF_STAGES] = {"late", "step", "replicate", "meta", "input_wait"};

static void put_hist(writer *w, const char *name, const online_perf_hist *hist) {
    put(w, "\"%s\":{\"b\":[", name);
    for (uint32_t index = 0U; index < ONLINE_PERF_BUCKETS; ++index) { put(w, index > 0U ? ",%u" : "%u", (unsigned)hist->counts[index]); }
    put(w, "],\"max\":%u,\"sum\":%llu}", (unsigned)hist->max_us, (unsigned long long)hist->sum_us);
}

size_t online_perf_json(const online_perf_window *window, const online_perf_totals *now, const online_perf_totals *before, uint32_t window_ms,
    char *out, size_t cap) {
    writer w = {.out = out, .cap = cap};
    if (cap == 0U) { return 0U; }
    put(&w, "{\"window_ms\":%u,\"ticks\":%llu,\"late\":%llu,\"dropped\":%llu,\"pumps\":%u,\"seated_ticks\":%llu,\"stages\":{",
        (unsigned)window_ms, grew(now->ticks, before->ticks), grew(now->late, before->late), grew(now->dropped, before->dropped),
        (unsigned)window->pumps, (unsigned long long)window->seated_ticks);
    for (uint32_t stage = 0U; stage < ONLINE_PERF_STAGES; ++stage) {
        if (stage > 0U) { put(&w, ","); }
        put_hist(&w, k_stages[stage], &window->stages[stage]);
    }
    const net_ws_server_stats_t *n = &now->net;
    const net_ws_server_stats_t *b = &before->net;
    put(&w, "},\"net\":{\"rx_messages\":%llu,\"rx_bytes\":%llu,\"rx_frames\":%llu,\"rx_pongs\":%llu,\"tx_messages\":%llu,"
        "\"tx_bytes\":%llu,\"tx_writes\":%llu,\"tx_partial\":%llu,\"tx_refused\":%llu,\"writable\":%llu,\"services\":%llu,"
        "\"service_passes\":%llu,\"queue_peak\":%u}",
        grew(n->rx_messages, b->rx_messages), grew(n->rx_bytes, b->rx_bytes), grew(n->rx_frames, b->rx_frames),
        grew(n->rx_pongs, b->rx_pongs), grew(n->tx_messages, b->tx_messages), grew(n->tx_bytes, b->tx_bytes),
        grew(n->tx_writes, b->tx_writes), grew(n->tx_partial, b->tx_partial), grew(n->tx_refused, b->tx_refused),
        grew(n->writable, b->writable), grew(n->services, b->services), grew(n->service_passes, b->service_passes),
        (unsigned)window->net_queue_peak);
    put(&w, ",\"repl\":{\"packets\":%llu,\"bytes\":%llu,\"row_bytes\":%llu,\"reliable\":%llu,\"cosmetic\":%llu,"
        "\"cosmetic_dropped\":%llu,\"rows_sent\":%llu,\"rows_deferred\":%llu,\"rows_waited_most\":%u,\"skipped\":%llu,"
        "\"refused\":%llu,\"outbox_peak\":%u,\"credit_min\":%d,\"credit_max\":%d}",
        (unsigned long long)window->packets, (unsigned long long)window->bytes, (unsigned long long)window->row_bytes,
        (unsigned long long)window->reliable, (unsigned long long)window->cosmetic, (unsigned long long)window->cosmetic_dropped,
        (unsigned long long)window->rows_sent, (unsigned long long)window->rows_deferred, (unsigned)window->rows_waited_most,
        (unsigned long long)window->skipped, (unsigned long long)window->refused, (unsigned)window->outbox_peak,
        (int)window->credit_min, (int)window->credit_max);
    put(&w, ",\"input\":{\"malformed\":%llu,\"underruns\":%llu,\"overruns\":%llu,\"trimmed\":%llu,\"lost_edges\":%llu,"
        "\"malformed_edges\":%llu,\"queue_peak\":%u}",
        grew(now->malformed, before->malformed), grew(now->underruns, before->underruns), grew(now->overruns, before->overruns),
        grew(now->trimmed, before->trimmed), grew(now->lost_edges, before->lost_edges),
        grew(now->malformed_edges, before->malformed_edges), (unsigned)window->input_queue_peak);
    const online_perf_clients *c = &window->clients;
    put(&w, ",\"clients\":{\"reports\":%u,", (unsigned)c->reports);
    put_hist(&w, "rtt", &c->rtt);
    put(&w, ",\"rtt_max_ms\":%u,\"corrections\":%llu,\"snaps\":%llu,\"stalls\":%llu,\"fps_sum\":%llu,\"fps_low\":%u,"
        "\"reconnects\":%u,\"web\":%u,\"touch\":%u,\"bench\":%u}",
        (unsigned)c->rtt_max_ms, (unsigned long long)c->corrections, (unsigned long long)c->snaps, (unsigned long long)c->stalls,
        (unsigned long long)c->fps_sum, (unsigned)c->fps_low, (unsigned)c->reconnects, (unsigned)c->web, (unsigned)c->touch,
        (unsigned)c->bench);
    put(&w, ",\"closes\":[");
    for (uint32_t index = 0U; index < ONLINE_PERF_CLOSES; ++index) {
        put(&w, index > 0U ? ",%llu" : "%llu", grew(now->closes[index], before->closes[index]));
    }
    put(&w, "]");
    if (now->has_process && before->has_process) {
        put(&w, ",\"process\":{\"user_us\":%llu,\"system_us\":%llu,\"rss_kb\":%llu}",
            grew(now->process.user_us, before->process.user_us), grew(now->process.system_us, before->process.system_us),
            (unsigned long long)now->process.rss_kb);
    }
    if (now->has_machine && before->has_machine) {
        const online_perf_machine *m = &now->machine;
        const online_perf_machine *p = &before->machine;
        put(&w, ",\"machine\":{\"user\":%llu,\"nice\":%llu,\"system\":%llu,\"idle\":%llu,\"iowait\":%llu,\"irq\":%llu,"
            "\"softirq\":%llu,\"steal\":%llu,\"rx_bytes\":%llu,\"tx_bytes\":%llu,\"rx_packets\":%llu,\"tx_packets\":%llu,"
            "\"rx_total\":%llu,\"tx_total\":%llu}",
            grew(m->user, p->user), grew(m->nice, p->nice), grew(m->system, p->system), grew(m->idle, p->idle),
            grew(m->iowait, p->iowait), grew(m->irq, p->irq), grew(m->softirq, p->softirq), grew(m->steal, p->steal),
            grew(m->rx_bytes, p->rx_bytes), grew(m->tx_bytes, p->tx_bytes), grew(m->rx_packets, p->rx_packets),
            grew(m->tx_packets, p->tx_packets), (unsigned long long)m->rx_bytes, (unsigned long long)m->tx_bytes);
    }
    if (now->plan_rooms > 0U) {
        put(&w, ",\"plan\":{\"rooms\":%u,\"room_id_base\":%u}", (unsigned)now->plan_rooms, (unsigned)now->plan_room_base);
    }
    put(&w, "}");
    return w.full ? 0U : w.used;
}
