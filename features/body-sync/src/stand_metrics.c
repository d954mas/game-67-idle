#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "stand_logic.h"

/* ---- histogram ---- */

enum { BINS_PER_DECADE = 60, LOWEST_DECADE = -5 };

void stand_hist_add(stand_hist_t *hist, double value) {
    if (!isfinite(value) || value < 0.0) {
        return;
    }
    uint32_t bin = 0;
    if (value > 1e-5) {
        double index = floor((log10(value) - (double)LOWEST_DECADE) * (double)BINS_PER_DECADE) + 1.0;
        bin = index >= (double)(STAND_HIST_BINS - 1) ? STAND_HIST_BINS - 1 : (uint32_t)index;
    }
    hist->bins[bin]++;
    hist->count++;
    hist->sum += value;
    hist->max = value > hist->max ? value : hist->max;
}

/* The upper edge of the bin holding the q-th sample, capped by the largest sample seen. */
double stand_hist_quantile(const stand_hist_t *hist, double q) {
    if (hist->count == 0) {
        return 0.0;
    }
    uint64_t rank = (uint64_t)ceil(q * (double)hist->count);
    rank = rank == 0 ? 1 : rank;
    uint64_t seen = 0;
    for (uint32_t i = 0; i < STAND_HIST_BINS; i++) {
        seen += hist->bins[i];
        if (seen >= rank) {
            if (i == 0) {
                return 0.0;
            }
            double edge = pow(10.0, (double)i / (double)BINS_PER_DECADE + (double)LOWEST_DECADE);
            return edge < hist->max ? edge : hist->max;
        }
    }
    return hist->max;
}

double stand_hist_mean(const stand_hist_t *hist) { return hist->count ? hist->sum / (double)hist->count : 0.0; }

/* ---- sampling ---- */

void stand_metrics_reset(stand_metrics_t *metrics, uint32_t guests) {
    memset(metrics, 0, sizeof(*metrics));
    metrics->guests = guests > BODY_SYNC_GUESTS_MAX ? BODY_SYNC_GUESTS_MAX : guests;
}

void stand_metrics_begin_tick(stand_metrics_t *metrics) {
    for (uint32_t g = 0; g < BODY_SYNC_GUESTS_MAX; g++) {
        metrics->tick_div_max[g] = 0.0;
        metrics->tick_div_sum[g] = 0.0;
        metrics->tick_jerk_max[g] = 0.0;
        metrics->tick_div_count[g] = 0;
    }
}

void stand_metrics_divergence(stand_metrics_t *metrics, uint32_t guest, double distance_m, double rotation_deg) {
    if (guest >= metrics->guests || !isfinite(distance_m)) {
        return;
    }
    stand_hist_add(&metrics->divergence[guest], distance_m);
    metrics->tick_div_max[guest] = distance_m > metrics->tick_div_max[guest] ? distance_m : metrics->tick_div_max[guest];
    metrics->tick_div_sum[guest] += distance_m;
    metrics->tick_div_count[guest]++;
    if (isfinite(rotation_deg) && rotation_deg > metrics->rotation_max[guest]) {
        metrics->rotation_max[guest] = rotation_deg;
    }
}

void stand_metrics_jerk(stand_metrics_t *metrics, uint32_t guest, double jerk_m) {
    if (guest >= metrics->guests || !isfinite(jerk_m)) {
        return;
    }
    stand_hist_add(&metrics->jerk[guest], jerk_m);
    metrics->tick_jerk_max[guest] = jerk_m > metrics->tick_jerk_max[guest] ? jerk_m : metrics->tick_jerk_max[guest];
    if (jerk_m > (double)BODY_SYNC_JERK_OVER_M) {
        metrics->jerk_over[guest]++;
    }
}

void stand_metrics_playback(stand_metrics_t *metrics, uint32_t guest, double lag_m, double path_m, double behind_ticks, bool moved,
                            double drawn_move_m, double owner_move_m, bool offset_active) {
    if (guest >= metrics->guests || !isfinite(lag_m) || !isfinite(path_m) || !isfinite(behind_ticks)) {
        return;
    }
    stand_hist_add(&metrics->lag[guest], lag_m);
    stand_hist_add(&metrics->path[guest], path_m);
    stand_hist_add(&metrics->behind[guest], behind_ticks > 0.0 ? behind_ticks : 0.0);
    if (!moved || !isfinite(drawn_move_m) || !isfinite(owner_move_m)) {
        return;
    }
    if (drawn_move_m > 1.25 * owner_move_m + 0.01) {
        /* The render offset giving a move back is a slide by design, counted apart. */
        if (offset_active) {
            metrics->offset_steps[guest]++;
        } else {
            metrics->steps[guest]++;
        }
    }
    if (drawn_move_m > 2.0) {
        metrics->snaps[guest]++;
        return;
    }
    double jump = drawn_move_m - owner_move_m;
    metrics->jump_max[guest] = jump > metrics->jump_max[guest] ? jump : metrics->jump_max[guest];
}

void stand_metrics_correction(stand_metrics_t *metrics, uint32_t guest, double correction_m) {
    if (guest >= metrics->guests || !isfinite(correction_m)) {
        return;
    }
    if (correction_m > 2.0) {
        metrics->snaps[guest]++;
        return;
    }
    metrics->jump_max[guest] = correction_m > metrics->jump_max[guest] ? correction_m : metrics->jump_max[guest];
}

void stand_metrics_response(stand_metrics_t *metrics, uint32_t guest, double ticks) {
    if (guest < metrics->guests && isfinite(ticks)) {
        stand_hist_add(&metrics->response[guest], ticks);
    }
}

void stand_metrics_response_miss(stand_metrics_t *metrics, uint32_t guest) {
    if (guest < metrics->guests) {
        metrics->response_misses[guest]++;
    }
}

void stand_metrics_rest(stand_metrics_t *metrics, uint32_t guest, double jitter_m, bool wake) {
    if (guest >= metrics->guests) {
        return;
    }
    metrics->rest_jitter[guest] = jitter_m > metrics->rest_jitter[guest] ? jitter_m : metrics->rest_jitter[guest];
    metrics->rest_wakes[guest] += wake ? 1u : 0u;
    metrics->rest_ticks[guest]++;
}

void stand_metrics_rest_lag(stand_metrics_t *metrics, uint32_t guest, uint32_t ticks) {
    if (guest < metrics->guests && ticks > metrics->rest_lag[guest]) {
        metrics->rest_lag[guest] = ticks;
    }
}

void stand_metrics_soft_place(stand_metrics_t *metrics, uint32_t guest, uint32_t places) {
    if (guest < metrics->guests && places > metrics->soft_places[guest]) {
        metrics->soft_places[guest] = places;
    }
}

void stand_metrics_layer_hit(stand_metrics_t *metrics, uint32_t guest, uint32_t tick, double pause_divergence) {
    if (guest >= metrics->guests) {
        return;
    }
    if (metrics->first_hit[guest] == 0u) {
        metrics->first_hit[guest] = tick + 1u;
    }
    if (isfinite(pause_divergence) && pause_divergence > metrics->pause_divergence[guest]) {
        metrics->pause_divergence[guest] = pause_divergence;
    }
}

void stand_metrics_copy_speed(stand_metrics_t *metrics, uint32_t guest, double speed) {
    if (guest < metrics->guests && isfinite(speed) && speed > metrics->copy_speed[guest]) {
        metrics->copy_speed[guest] = speed;
    }
}

void stand_metrics_end_tick(stand_metrics_t *metrics) {
    if (metrics->ticks >= BODY_SYNC_STAND_TICKS_MAX) {
        return;
    }
    uint32_t t = metrics->ticks++;
    for (uint32_t g = 0; g < metrics->guests; g++) {
        metrics->series_div_max[g][t] = (float)metrics->tick_div_max[g];
        metrics->series_div_mean[g][t] =
            metrics->tick_div_count[g] ? (float)(metrics->tick_div_sum[g] / (double)metrics->tick_div_count[g]) : 0.0f;
        metrics->series_jerk_max[g][t] = (float)metrics->tick_jerk_max[g];
    }
}

void stand_metrics_bytes(stand_metrics_t *metrics, uint32_t guest, int direction, uint32_t kind, uint64_t bytes, uint32_t tick) {
    if (guest >= metrics->guests || (direction != STAND_DIR_TO_GUEST && direction != STAND_DIR_TO_HOST) || kind >= BODY_SYNC_KINDS_ALL ||
        bytes == 0) {
        return;
    }
    uint32_t second = tick / BODY_SYNC_TICK_HZ;
    second = second >= BODY_SYNC_TIME_MAX_S ? BODY_SYNC_TIME_MAX_S - 1 : second;
    uint64_t sum = metrics->bytes_second[guest][direction][kind][second] + bytes;
    metrics->bytes_second[guest][direction][kind][second] = sum > UINT32_MAX ? UINT32_MAX : (uint32_t)sum;
    metrics->bytes_total[guest][direction][kind] += bytes;
}

void stand_metrics_link(stand_metrics_t *metrics, uint32_t guest, int direction, double bytes_per_s) {
    if (guest >= metrics->guests || (direction != STAND_DIR_TO_GUEST && direction != STAND_DIR_TO_HOST) || !isfinite(bytes_per_s) ||
        bytes_per_s < 0.0) {
        return;
    }
    metrics->link_rate_sum[guest][direction] += bytes_per_s;
    metrics->link_rate_count[guest][direction]++;
}

void stand_metrics_closed(stand_metrics_t *metrics, uint8_t seat, uint32_t tick, uint32_t reason, const char *reason_name) {
    /* Both ends report one close; the first report names it. */
    for (uint32_t i = 0; i < metrics->closed_count; i++) {
        if (metrics->closed[i].seat == seat) {
            return;
        }
    }
    if (metrics->closed_count < STAND_CLOSED_MAX) {
        metrics->closed[metrics->closed_count++] = (stand_closed_t){.seat = seat, .tick = tick, .reason = reason, .reason_name = reason_name};
    }
}

static uint32_t seconds_run(const stand_metrics_t *metrics) {
    uint32_t seconds = (metrics->ticks + BODY_SYNC_TICK_HZ - 1) / BODY_SYNC_TICK_HZ;
    seconds = seconds == 0 ? 1 : seconds;
    return seconds > BODY_SYNC_TIME_MAX_S ? BODY_SYNC_TIME_MAX_S : seconds;
}

static void summarize_bytes(const stand_metrics_t *m, uint32_t g, int dir, body_sync_bytes_t *out) {
    memset(out, 0, sizeof(*out));
    double duration = (double)(m->ticks ? m->ticks : 1) / (double)BODY_SYNC_TICK_HZ;
    uint32_t seconds = seconds_run(m);
    for (uint32_t k = 0; k < BODY_SYNC_KINDS_ALL; k++) {
        out->total_bytes[k] = m->bytes_total[g][dir][k];
        out->mean_bytes_per_s[k] = (double)m->bytes_total[g][dir][k] / duration;
        uint32_t peak = 0;
        for (uint32_t s = 0; s < seconds; s++) {
            peak = m->bytes_second[g][dir][k][s] > peak ? m->bytes_second[g][dir][k][s] : peak;
        }
        out->peak_bytes_per_s[k] = (double)peak;
    }
    out->link_estimate_bytes_per_s = m->link_rate_count[g][dir] ? m->link_rate_sum[g][dir] / (double)m->link_rate_count[g][dir] : -1.0;
}

void stand_metrics_summarize(const stand_metrics_t *metrics, body_sync_report_t *report) {
    report->guest_count = metrics->guests;
    for (uint32_t g = 0; g < metrics->guests; g++) {
        body_sync_guest_metrics_t *out = &report->guests[g];
        out->seat = (uint8_t)(g + 1);
        out->divergence_max_m = metrics->divergence[g].max;
        out->divergence_p95_m = stand_hist_quantile(&metrics->divergence[g], 0.95);
        out->divergence_mean_m = stand_hist_mean(&metrics->divergence[g]);
        out->rotation_max_deg = metrics->rotation_max[g];
        out->jerk_max_m = metrics->jerk[g].max;
        out->jerk_p95_m = stand_hist_quantile(&metrics->jerk[g], 0.95);
        out->jerk_frames_over = metrics->jerk_over[g];
        out->lag_divergence_max_m = metrics->lag[g].max;
        out->lag_divergence_p95_m = stand_hist_quantile(&metrics->lag[g], 0.95);
        out->path_error_max_m = metrics->path[g].max;
        out->path_error_p95_m = stand_hist_quantile(&metrics->path[g], 0.95);
        out->step_count = metrics->steps[g];
        out->offset_step_count = metrics->offset_steps[g];
        out->behind_max_ticks = metrics->behind[g].max;
        out->behind_p95_ticks = stand_hist_quantile(&metrics->behind[g], 0.95);
        out->playback_samples = metrics->lag[g].count;
        out->response_events = (uint32_t)metrics->response[g].count;
        out->response_misses = metrics->response_misses[g];
        out->response_max_ticks = metrics->response[g].max;
        out->response_p95_ticks = stand_hist_quantile(&metrics->response[g], 0.95);
        out->rest_jitter_max_m = metrics->rest_jitter[g];
        out->rest_wakes = metrics->rest_wakes[g];
        out->rest_ticks = metrics->rest_ticks[g];
        out->rest_lag_max_ticks = metrics->rest_lag[g];
        out->soft_places_max = metrics->soft_places[g];
        out->copy_speed_max = metrics->copy_speed[g];
        out->first_hit_tick = metrics->first_hit[g] ? (double)(metrics->first_hit[g] - 1u) : -1.0;
        out->pause_divergence_max_m = metrics->pause_divergence[g];
        out->snap_count = metrics->snaps[g];
        out->jump_max_m = metrics->jump_max[g];
        summarize_bytes(metrics, g, STAND_DIR_TO_GUEST, &out->to_guest);
        summarize_bytes(metrics, g, STAND_DIR_TO_HOST, &out->to_host);
    }
}

/* ---- metric names ---- */

static const struct {
    const char *name;
    stand_metric_unit_t unit;
} metric_names[] = {
    {"divergence.max", STAND_UNIT_LENGTH},       {"divergence.p95", STAND_UNIT_LENGTH},        {"divergence.mean", STAND_UNIT_LENGTH},
    {"rotation.max", STAND_UNIT_ANGLE},          {"jerk.max", STAND_UNIT_LENGTH},              {"jerk.p95", STAND_UNIT_LENGTH},
    {"jerk.frames_over", STAND_UNIT_COUNT},      {"host_ticks.max", STAND_UNIT_TICKS},         {"echo_ticks.max", STAND_UNIT_TICKS},
    {"seen_ticks.max", STAND_UNIT_TICKS},        {"bytes_per_s.to_guest", STAND_UNIT_RATE},    {"bytes_per_s.to_host", STAND_UNIT_RATE},
    {"peak_bytes_per_s.to_guest", STAND_UNIT_RATE}, {"peak_bytes_per_s.to_host", STAND_UNIT_RATE},
    {"stale_ticks.max", STAND_UNIT_TICKS},       {"deferred.count", STAND_UNIT_COUNT},
    {"lag_divergence.max", STAND_UNIT_LENGTH},   {"lag_divergence.p95", STAND_UNIT_LENGTH},    {"path_error.max", STAND_UNIT_LENGTH},
    {"path_error.p95", STAND_UNIT_LENGTH},       {"jump.max", STAND_UNIT_LENGTH},              {"step.count", STAND_UNIT_COUNT},
    {"snap.count", STAND_UNIT_COUNT},         {"offset_step.count", STAND_UNIT_COUNT},     {"behind_ticks.max", STAND_UNIT_TICKS},
    {"behind_ticks.p95", STAND_UNIT_TICKS},      {"response_ticks.max", STAND_UNIT_TICKS},    {"response_ticks.p95", STAND_UNIT_TICKS},
    {"rest_jitter.max", STAND_UNIT_LENGTH},      {"rest_wakes.count", STAND_UNIT_COUNT},      {"rest_lag_ticks.max", STAND_UNIT_TICKS},
    {"soft_places.max", STAND_UNIT_COUNT},       {"response_misses.count", STAND_UNIT_COUNT},
};

bool stand_metric_unit(const char *name, stand_metric_unit_t *unit) {
    for (size_t i = 0; i < sizeof(metric_names) / sizeof(metric_names[0]); i++) {
        if (strcmp(name, metric_names[i].name) == 0) {
            *unit = metric_names[i].unit;
            return true;
        }
    }
    return false;
}

static double peak_total(const stand_metrics_t *m, uint32_t g, int dir) {
    uint32_t seconds = seconds_run(m);
    double peak = 0.0;
    for (uint32_t s = 0; s < seconds; s++) {
        double sum = 0.0;
        for (uint32_t k = 0; k < BODY_SYNC_KINDS_ALL; k++) {
            sum += (double)m->bytes_second[g][dir][k][s];
        }
        peak = sum > peak ? sum : peak;
    }
    return peak;
}

bool stand_metric_value(const body_sync_report_t *report, const stand_metrics_t *metrics, const char *name, double *value, char *detail,
                        size_t detail_size) {
    if (detail && detail_size) {
        detail[0] = '\0';
    }
    stand_metric_unit_t unit;
    if (!stand_metric_unit(name, &unit)) {
        return false;
    }
    double worst = 0.0;
    if (strncmp(name, "host_ticks", 10) == 0 || strncmp(name, "echo_ticks", 10) == 0 || strncmp(name, "seen_ticks", 10) == 0) {
        for (uint32_t i = 0; i < report->action_count; i++) {
            const body_sync_action_metrics_t *a = &report->actions[i];
            bool counts = a->start_tick != UINT32_MAX && (name[0] == 'e' ? a->answered : a->shows);
            if (!counts) {
                continue;
            }
            int32_t v = name[0] == 'h' ? a->host_ticks : name[0] == 'e' ? a->echo_ticks : a->seen_ticks;
            if (v < 0) {
                /* Never seen is no delay of zero: the bound cannot hold. */
                if (detail && detail_size) {
                    snprintf(detail, detail_size, "%s on line %u was never seen", a->label[0] ? a->label : stand_verb_name(a->verb), a->line);
                }
                *value = (double)INFINITY;
                return true;
            }
            worst = (double)v > worst ? (double)v : worst;
        }
        *value = worst;
        return true;
    }
    for (uint32_t g = 0; g < report->guest_count; g++) {
        const body_sync_guest_metrics_t *m = &report->guests[g];
        double v = 0.0;
        if (strcmp(name, "divergence.max") == 0) {
            v = m->divergence_max_m;
        } else if (strcmp(name, "divergence.p95") == 0) {
            v = m->divergence_p95_m;
        } else if (strcmp(name, "divergence.mean") == 0) {
            v = m->divergence_mean_m;
        } else if (strcmp(name, "rotation.max") == 0) {
            v = m->rotation_max_deg;
        } else if (strcmp(name, "jerk.max") == 0) {
            v = m->jerk_max_m;
        } else if (strcmp(name, "jerk.p95") == 0) {
            v = m->jerk_p95_m;
        } else if (strcmp(name, "jerk.frames_over") == 0) {
            v = (double)m->jerk_frames_over;
        } else if (strcmp(name, "stale_ticks.max") == 0) {
            v = (double)m->stale_max_ticks;
        } else if (strcmp(name, "deferred.count") == 0) {
            v = (double)m->deferred;
        } else if (m->playback_samples == 0 && (strncmp(name, "lag_divergence", 14) == 0 || strncmp(name, "path_error", 10) == 0 ||
                                                strcmp(name, "jump.max") == 0 || strcmp(name, "step.count") == 0 || strcmp(name, "snap.count") == 0 ||
                                                strcmp(name, "offset_step.count") == 0 || strncmp(name, "behind_ticks", 12) == 0)) {
            /* No copy measured is no evidence: the bound cannot hold. */
            if (detail && detail_size) {
                snprintf(detail, detail_size, "g%u: no copy was measured", (unsigned)m->seat);
            }
            *value = (double)INFINITY;
            return true;
        } else if (m->rest_ticks == 0 && (strcmp(name, "rest_jitter.max") == 0 || strcmp(name, "rest_wakes.count") == 0)) {
            if (detail && detail_size) {
                snprintf(detail, detail_size, "g%u: no rest was measured", (unsigned)m->seat);
            }
            *value = (double)INFINITY;
            return true;
        } else if (m->response_events == 0 && strncmp(name, "response_ticks", 14) == 0) {
            if (detail && detail_size) {
                snprintf(detail, detail_size, "g%u: no hit was measured", (unsigned)m->seat);
            }
            *value = (double)INFINITY;
            return true;
        } else if (strcmp(name, "response_ticks.max") == 0) {
            v = m->response_max_ticks;
        } else if (strcmp(name, "response_ticks.p95") == 0) {
            v = m->response_p95_ticks;
        } else if (strcmp(name, "rest_jitter.max") == 0) {
            v = m->rest_jitter_max_m;
        } else if (strcmp(name, "rest_wakes.count") == 0) {
            v = (double)m->rest_wakes;
        } else if (strcmp(name, "rest_lag_ticks.max") == 0) {
            v = (double)m->rest_lag_max_ticks;
        } else if (strcmp(name, "soft_places.max") == 0) {
            v = (double)m->soft_places_max;
        } else if (strcmp(name, "response_misses.count") == 0) {
            v = (double)m->response_misses;
        } else if (strcmp(name, "behind_ticks.max") == 0) {
            v = m->behind_max_ticks;
        } else if (strcmp(name, "behind_ticks.p95") == 0) {
            v = m->behind_p95_ticks;
        } else if (strcmp(name, "offset_step.count") == 0) {
            v = (double)m->offset_step_count;
        } else if (strcmp(name, "lag_divergence.max") == 0) {
            v = m->lag_divergence_max_m;
        } else if (strcmp(name, "lag_divergence.p95") == 0) {
            v = m->lag_divergence_p95_m;
        } else if (strcmp(name, "path_error.max") == 0) {
            v = m->path_error_max_m;
        } else if (strcmp(name, "path_error.p95") == 0) {
            v = m->path_error_p95_m;
        } else if (strcmp(name, "jump.max") == 0) {
            v = m->jump_max_m;
        } else if (strcmp(name, "step.count") == 0) {
            v = (double)m->step_count;
        } else if (strcmp(name, "snap.count") == 0) {
            v = (double)m->snap_count;
        } else if (strncmp(name, "bytes_per_s.", 12) == 0) {
            const body_sync_bytes_t *b = strcmp(name + 12, "to_guest") == 0 ? &m->to_guest : &m->to_host;
            for (uint32_t k = 0; k < BODY_SYNC_KINDS_ALL; k++) {
                v += b->mean_bytes_per_s[k];
            }
        } else if (metrics) {
            v = peak_total(metrics, g, strcmp(name + 17, "to_guest") == 0 ? STAND_DIR_TO_GUEST : STAND_DIR_TO_HOST);
        }
        worst = v > worst ? v : worst;
    }
    *value = worst;
    return true;
}

/* ---- JSON ---- */

typedef struct json {
    char *buffer;
    size_t capacity;
    size_t length; /* counts past the capacity, so a short buffer learns the size it needs */
    bool broken;   /* a formatted piece did not fit its scratch: the text would not be JSON */
} json_t;

static void raw(json_t *j, const char *text, size_t n) {
    if (j->length + n < j->capacity) {
        memcpy(j->buffer + j->length, text, n);
    }
    j->length += n;
}

static void put(json_t *j, const char *text) { raw(j, text, strlen(text)); }

__attribute__((format(printf, 2, 3))) static void putf(json_t *j, const char *format, ...) {
    char temp[256];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);
    if (n < 0 || (size_t)n >= sizeof(temp)) {
        j->broken = true;
        return;
    }
    raw(j, temp, (size_t)n);
}

static void number(json_t *j, double v) {
    if (!isfinite(v)) {
        put(j, "null");
    } else {
        putf(j, "%.6g", v);
    }
}

static void string(json_t *j, const char *s) {
    put(j, "\"");
    for (; s && *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            char e[2] = {'\\', (char)c};
            raw(j, e, 2);
        } else if (c < 0x20 || c > 0x7E) {
            putf(j, "\\u%04x", (unsigned)c);
        } else {
            raw(j, (const char *)&c, 1);
        }
    }
    put(j, "\"");
}

static void key(json_t *j, const char *k) {
    string(j, k);
    put(j, ":");
}

static const char *kind_name(uint32_t kind) {
    static const char *seam[] = {"header", "copy", "sim", "held", "authority"};
    return kind < BODY_SYNC_SEAM_KINDS ? seam[kind] : body_sync_stand_kind_name((body_sync_stand_kind_t)(kind - BODY_SYNC_SEAM_KINDS));
}

static void bytes_object(json_t *j, const body_sync_bytes_t *b) {
    put(j, "{");
    key(j, "link_estimate_bytes_per_s");
    number(j, b->link_estimate_bytes_per_s);
    put(j, ",");
    key(j, "kinds");
    put(j, "{");
    for (uint32_t k = 0; k < BODY_SYNC_KINDS_ALL; k++) {
        if (k) {
            put(j, ",");
        }
        key(j, kind_name(k));
        put(j, "{");
        key(j, "mean");
        number(j, b->mean_bytes_per_s[k]);
        put(j, ",");
        key(j, "peak");
        number(j, b->peak_bytes_per_s[k]);
        put(j, ",");
        key(j, "total");
        putf(j, "%llu", (unsigned long long)b->total_bytes[k]);
        put(j, "}");
    }
    put(j, "}}");
}

static void float_series(json_t *j, const float *values, uint32_t count) {
    put(j, "[");
    for (uint32_t i = 0; i < count; i++) {
        if (i) {
            put(j, ",");
        }
        if (!isfinite(values[i])) {
            put(j, "null");
        } else {
            putf(j, "%.4g", (double)values[i]);
        }
    }
    put(j, "]");
}

static void series(json_t *j, const stand_metrics_t *m, uint32_t g) {
    put(j, ",");
    key(j, "series");
    put(j, "{");
    key(j, "divergence_max");
    float_series(j, m->series_div_max[g], m->ticks);
    put(j, ",");
    key(j, "divergence_mean");
    float_series(j, m->series_div_mean[g], m->ticks);
    put(j, ",");
    key(j, "jerk_max");
    float_series(j, m->series_jerk_max[g], m->ticks);
    put(j, ",");
    key(j, "bytes_per_s");
    put(j, "{");
    uint32_t seconds = seconds_run(m);
    for (int dir = 0; dir < 2; dir++) {
        if (dir) {
            put(j, ",");
        }
        key(j, dir == STAND_DIR_TO_GUEST ? "to_guest" : "to_host");
        put(j, "{");
        for (uint32_t k = 0; k < BODY_SYNC_KINDS_ALL; k++) {
            if (k) {
                put(j, ",");
            }
            key(j, kind_name(k));
            put(j, "[");
            for (uint32_t s = 0; s < seconds; s++) {
                putf(j, s ? ",%u" : "%u", m->bytes_second[g][dir][k][s]);
            }
            put(j, "]");
        }
        put(j, "}");
    }
    put(j, "}}");
}

bool stand_json_write(const body_sync_report_t *r, const stand_metrics_t *m, bool with_series, char *buffer, size_t capacity, size_t *size) {
    json_t j = {.buffer = buffer, .capacity = buffer ? capacity : 0, .length = 0};
    put(&j, "{");
    key(&j, "schema");
    string(&j, BODY_SYNC_METRICS_SCHEMA);
    put(&j, ",");
    key(&j, "scenario");
    string(&j, r->scenario);
    put(&j, ",");
    key(&j, "scene");
    string(&j, r->scene);
    put(&j, ",");
    key(&j, "implementation");
    string(&j, r->implementation);
    put(&j, ",");
    key(&j, "seed");
    putf(&j, "%llu", (unsigned long long)r->seed);
    put(&j, ",");
    key(&j, "tick_hz");
    putf(&j, "%u", r->tick_hz);
    put(&j, ",");
    key(&j, "light_mass_max");
    number(&j, (double)r->light_mass_max);
    put(&j, ",");
    key(&j, "ticks");
    putf(&j, "%u", r->ticks);
    put(&j, ",");
    key(&j, "rest_tick");
    if (r->rest_tick == UINT32_MAX) {
        put(&j, "null");
    } else {
        putf(&j, "%u", r->rest_tick);
    }
    put(&j, ",");
    key(&j, "passed");
    put(&j, r->passed ? "true" : "false");
    put(&j, ",");
    key(&j, "guests");
    put(&j, "[");
    for (uint32_t g = 0; g < r->guest_count; g++) {
        const body_sync_guest_metrics_t *gm = &r->guests[g];
        put(&j, g ? ",{" : "{");
        key(&j, "seat");
        putf(&j, "%u", (unsigned)gm->seat);
        put(&j, ",");
        key(&j, "divergence");
        put(&j, "{\"unit\":\"m\",\"includes_copy_latency\":true,");
        key(&j, "max");
        number(&j, gm->divergence_max_m);
        put(&j, ",");
        key(&j, "p95");
        number(&j, gm->divergence_p95_m);
        put(&j, ",");
        key(&j, "mean");
        number(&j, gm->divergence_mean_m);
        put(&j, ",");
        key(&j, "rotation_max_deg");
        number(&j, gm->rotation_max_deg);
        put(&j, "},");
        key(&j, "jerk");
        put(&j, "{\"unit\":\"m\",");
        key(&j, "max");
        number(&j, gm->jerk_max_m);
        put(&j, ",");
        key(&j, "p95");
        number(&j, gm->jerk_p95_m);
        put(&j, ",");
        key(&j, "frames_over");
        putf(&j, "%u", gm->jerk_frames_over);
        put(&j, ",");
        key(&j, "over_m");
        number(&j, (double)BODY_SYNC_JERK_OVER_M);
        put(&j, "},");
        key(&j, "playback");
        put(&j, "{\"unit\":\"m\",");
        key(&j, "lag_divergence_max");
        number(&j, gm->lag_divergence_max_m);
        put(&j, ",");
        key(&j, "lag_divergence_p95");
        number(&j, gm->lag_divergence_p95_m);
        put(&j, ",");
        key(&j, "path_error_max");
        number(&j, gm->path_error_max_m);
        put(&j, ",");
        key(&j, "path_error_p95");
        number(&j, gm->path_error_p95_m);
        put(&j, ",");
        key(&j, "jump_max");
        number(&j, gm->jump_max_m);
        put(&j, ",");
        key(&j, "steps");
        putf(&j, "%u", gm->step_count);
        put(&j, ",");
        key(&j, "snaps");
        putf(&j, "%u", gm->snap_count);
        put(&j, ",");
        key(&j, "offset_steps");
        putf(&j, "%u", gm->offset_step_count);
        put(&j, ",");
        key(&j, "behind_ticks_max");
        number(&j, gm->behind_max_ticks);
        put(&j, ",");
        key(&j, "behind_ticks_p95");
        number(&j, gm->behind_p95_ticks);
        put(&j, ",");
        key(&j, "samples");
        putf(&j, "%llu", (unsigned long long)gm->playback_samples);
        put(&j, "},");
        key(&j, "layer");
        put(&j, "{");
        key(&j, "response_events");
        putf(&j, "%u", gm->response_events);
        put(&j, ",");
        key(&j, "response_misses");
        putf(&j, "%u", gm->response_misses);
        put(&j, ",");
        key(&j, "response_ticks_max");
        number(&j, gm->response_max_ticks);
        put(&j, ",");
        key(&j, "response_ticks_p95");
        number(&j, gm->response_p95_ticks);
        put(&j, ",");
        key(&j, "rest_jitter_max");
        number(&j, gm->rest_jitter_max_m);
        put(&j, ",");
        key(&j, "rest_wakes");
        putf(&j, "%u", gm->rest_wakes);
        put(&j, ",");
        key(&j, "rest_ticks");
        putf(&j, "%u", gm->rest_ticks);
        put(&j, ",");
        key(&j, "rest_lag_ticks_max");
        putf(&j, "%u", gm->rest_lag_max_ticks);
        put(&j, ",");
        key(&j, "soft_places_max");
        putf(&j, "%u", gm->soft_places_max);
        put(&j, ",");
        key(&j, "copy_speed_max");
        number(&j, gm->copy_speed_max);
        put(&j, ",");
        key(&j, "first_hit_tick");
        number(&j, gm->first_hit_tick);
        put(&j, ",");
        key(&j, "pause_divergence_max");
        number(&j, gm->pause_divergence_max_m);
        put(&j, "},");
        key(&j, "to_guest");
        bytes_object(&j, &gm->to_guest);
        put(&j, ",");
        key(&j, "to_host");
        bytes_object(&j, &gm->to_host);
        put(&j, ",");
        key(&j, "link");
        put(&j, "{");
        key(&j, "rtt_ms");
        number(&j, gm->rtt_ms);
        put(&j, ",");
        key(&j, "loss");
        number(&j, gm->loss);
        put(&j, "},");
        key(&j, "seam");
        putf(&j, "{\"refused\":%u,\"stale\":%u,\"rejected\":%u,\"deferred\":%llu,\"stale_max_ticks\":%u,\"flush_over_limit\":%u,\"packets_to_guest\":%u}",
             gm->refused, gm->stale, gm->rejected, (unsigned long long)gm->deferred, gm->stale_max_ticks, gm->flush_over_limit, gm->packets_to_guest);
        if (with_series && m) {
            series(&j, m, g);
        }
        put(&j, "}");
    }
    put(&j, "],");
    key(&j, "actions");
    put(&j, "[");
    for (uint32_t i = 0; i < r->action_count; i++) {
        const body_sync_action_metrics_t *a = &r->actions[i];
        put(&j, i ? ",{" : "{");
        putf(&j, "\"line\":%u,", a->line);
        key(&j, "label");
        string(&j, a->label);
        putf(&j, ",\"player\":%u,", (unsigned)a->player);
        key(&j, "verb");
        string(&j, stand_verb_name(a->verb));
        put(&j, ",");
        key(&j, "start_tick");
        if (a->start_tick == UINT32_MAX) {
            put(&j, "null");
        } else {
            putf(&j, "%u", a->start_tick);
        }
        put(&j, ",");
        key(&j, "outcome");
        string(&j, body_sync_outcome_name(a->outcome));
        putf(&j, ",\"host_ticks\":%d,\"echo_ticks\":%d,\"seen_ticks\":%d,\"shows\":%s,\"answered\":%s}", a->host_ticks, a->echo_ticks, a->seen_ticks,
             a->shows ? "true" : "false", a->answered ? "true" : "false");
    }
    put(&j, "],");
    key(&j, "checks");
    put(&j, "[");
    for (uint32_t i = 0; i < r->check_count; i++) {
        const body_sync_check_metrics_t *c = &r->checks[i];
        put(&j, i ? ",{" : "{");
        putf(&j, "\"line\":%u,", c->line);
        key(&j, "kind");
        string(&j, stand_expect_name(c->kind));
        put(&j, ",");
        key(&j, "passed");
        put(&j, c->passed ? "true" : "false");
        put(&j, ",");
        key(&j, "value");
        number(&j, c->value);
        put(&j, ",");
        key(&j, "limit");
        number(&j, c->limit);
        put(&j, ",");
        key(&j, "detail");
        string(&j, c->detail);
        put(&j, "}");
    }
    put(&j, "],");
    key(&j, "sessions_closed");
    put(&j, "[");
    for (uint32_t i = 0; m && i < m->closed_count; i++) {
        putf(&j, "%s{\"seat\":%u,\"tick\":%u,\"reason\":%u,", i ? "," : "", (unsigned)m->closed[i].seat, m->closed[i].tick, m->closed[i].reason);
        key(&j, "reason_name");
        string(&j, m->closed[i].reason_name ? m->closed[i].reason_name : "");
        put(&j, "}");
    }
    put(&j, "]}\n");
    if (size) {
        *size = j.length;
    }
    if (buffer && j.length < capacity && !j.broken) {
        buffer[j.length] = '\0';
        return true;
    }
    return false;
}
