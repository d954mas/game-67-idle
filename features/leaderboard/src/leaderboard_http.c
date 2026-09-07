#include "features/leaderboard/leaderboard_http.h"
#include "features/leaderboard/leaderboard_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(LEADERBOARD_TESTING)
#include "http/nt_http.h"
#include "time/nt_time.h"
#include <time.h>
#endif

/* One request per board serves both scopes and doubles as the submit, so the
 * state machine is per board: idle -> loading -> loaded | error, with the
 * facade's submits riding whichever request starts next. */

/* Body before obfuscation; the encoder's own ceiling is the same. */
#define HTTP_BODY_MAX 512
#define HTTP_PAYLOAD_MAX 1024
#define HTTP_URL_MAX 1536
#define HTTP_METRIC_MAX 64

typedef enum {
    HTTP_IDLE = 0,
    HTTP_LOADING,
    HTTP_LOADED,
    HTTP_ERROR,
} http_status_t;

typedef enum {
    SUBMIT_NONE = 0,
    SUBMIT_QUEUED,    /* arrived while a request was in flight; rides the next one */
    SUBMIT_IN_FLIGHT, /* the request in flight carries it; completes with it */
} http_submit_t;

typedef struct {
    http_status_t status;
    uint32_t request;
    double last_response;
    int errors_sequence;
    http_submit_t submit[LEADERBOARD_SCOPE_COUNT];
    /* The last payload the game submitted; the poll re-sends it because the
     * server row holds only what the latest request carried. */
    char extra[LEADERBOARD_EXTRA_MAX];
    lb_response_t response;
} http_board_t;

typedef struct {
    http_board_t boards[LEADERBOARD_MAX_BOARDS];
    /* Seconds to add to the local clock so its UTC day matches the server's
     * (day-granular): midnight keeps arriving at the server's midnight even
     * fully offline. 0 until the first response. */
    int64_t day_offset;
} http_state_t;

/* ---- engine transport ---- */

#if !defined(LEADERBOARD_TESTING)
static uint32_t engine_request(const char *url, void *ud) {
    (void)ud;
    return nt_http_request(url).id;
}

static leaderboard_http_state_t engine_state(uint32_t request, void *ud) {
    (void)ud;
    switch (nt_http_state((nt_http_request_t){request})) {
    case NT_HTTP_STATE_DONE:
        return LEADERBOARD_HTTP_DONE;
    case NT_HTTP_STATE_FAILED:
        return LEADERBOARD_HTTP_FAILED;
    default:
        return LEADERBOARD_HTTP_PENDING;
    }
}

static uint8_t *engine_take_body(uint32_t request, uint32_t *size, void *ud) {
    (void)ud;
    return nt_http_take_data((nt_http_request_t){request}, size);
}

static void engine_release(uint32_t request, void *ud) {
    (void)ud;
    nt_http_free((nt_http_request_t){request});
}

static double engine_monotonic_now(void *ud) {
    (void)ud;
    return nt_time_now();
}

static int64_t engine_unix_now(void *ud) {
    (void)ud;
    return (int64_t)time(NULL);
}

static const leaderboard_http_transport_t k_engine_transport = {
    .request = engine_request,
    .state = engine_state,
    .take_body = engine_take_body,
    .release = engine_release,
    .monotonic_now = engine_monotonic_now,
    .unix_now = engine_unix_now,
    .userdata = NULL,
};
#endif

/* ---- helpers ---- */

static leaderboard_http_t *self_of(void *ud) {
    return (leaderboard_http_t *)ud;
}

static http_state_t *state_of(leaderboard_http_t *self) {
    return self != NULL ? (http_state_t *)self->state : NULL;
}

static const leaderboard_http_transport_t *transport_of(const leaderboard_http_t *self) {
    return self->config.transport;
}

static bool dormant(const leaderboard_http_t *self) {
    return self == NULL || self->config.url == NULL || self->config.url[0] == '\0' ||
           self->config.key == NULL || self->config.key[0] == '\0';
}

static bool ready(leaderboard_http_t *self, leaderboard_board_t board, http_board_t **out) {
    http_state_t *st = state_of(self);
    if (st == NULL || dormant(self) || board.index >= LEADERBOARD_MAX_BOARDS ||
        leaderboard_board_def(board) == NULL) {
        return false;
    }
    *out = &st->boards[board.index];
    return true;
}

static bool scope_declared(const leaderboard_board_def_t *def, leaderboard_scope_t scope) {
    return (def->scopes & (1u << (unsigned)scope)) != 0;
}

static void metric_name(const leaderboard_board_def_t *def, char out[HTTP_METRIC_MAX]) {
    const char *name = def->portal_id != NULL && def->portal_id[0] != '\0' ? def->portal_id : def->id;
    snprintf(out, HTTP_METRIC_MAX, "%s", name != NULL ? name : "");
}

static void current_day(const leaderboard_http_t *self, char out[LB_DAY_STR_MAX]) {
    const http_state_t *st = (const http_state_t *)self->state;
    lb_utc_day((time_t)(transport_of(self)->unix_now(transport_of(self)->userdata) + st->day_offset), out);
}

/* ---- request body ---- */

static bool is_plain_integer(const char *s, size_t len) {
    size_t i = (len > 0 && s[0] == '-') ? 1 : 0;
    if (i == len || len > 18) {
        return false;
    }
    if (s[i] == '0' && len - i > 1) {
        return false;
    }
    for (; i < len; i++) {
        if (s[i] < '0' || s[i] > '9') {
            return false;
        }
    }
    return true;
}

/* Appends `src` as a JSON string body (no quotes); false when it does not fit. */
static bool append_json_escaped(char *out, size_t cap, size_t *len, const char *src, size_t src_len) {
    for (size_t i = 0; i < src_len; i++) {
        const unsigned char c = (unsigned char)src[i];
        char piece[8];
        int n;
        if (c == '"' || c == '\\') {
            n = snprintf(piece, sizeof piece, "\\%c", c);
        } else if (c < 0x20) {
            n = snprintf(piece, sizeof piece, "\\u%04x", c);
        } else {
            piece[0] = (char)c;
            piece[1] = '\0';
            n = 1;
        }
        if (*len + (size_t)n + 1 > cap) {
            return false;
        }
        memcpy(out + *len, piece, (size_t)n);
        *len += (size_t)n;
        out[*len] = '\0';
    }
    return true;
}

static bool append_text(char *out, size_t cap, size_t *len, const char *text) {
    const size_t n = strlen(text);
    if (*len + n + 1 > cap) {
        return false;
    }
    memcpy(out + *len, text, n + 1);
    *len += n;
    return true;
}

static bool reserved_key(const char *key, size_t key_len, const char *metric, const char *day_field) {
    return (key_len == 7 && memcmp(key, "user_id", 7) == 0) || (key_len == 3 && memcmp(key, "day", 3) == 0) ||
           (key_len == strlen(metric) && memcmp(key, metric, key_len) == 0) ||
           (key_len == strlen(day_field) && memcmp(key, day_field, key_len) == 0);
}

/* The game's payload rides as body fields; a pair that collides with the
 * wire's own fields is dropped rather than letting it forge the score. */
static bool append_extra_fields(char *out, size_t cap, size_t *len, const char *extra, const char *metric,
                                const char *day_field) {
    const char *p = extra;
    while (*p != '\0') {
        const char *end = strchr(p, ';');
        const size_t pair_len = end != NULL ? (size_t)(end - p) : strlen(p);
        const char *eq = memchr(p, '=', pair_len);
        if (eq == NULL) {
            break; /* garbage past here per the codec */
        }
        const size_t key_len = (size_t)(eq - p);
        const char *value = eq + 1;
        const size_t value_len = pair_len - key_len - 1;
        if (key_len > 0 && !reserved_key(p, key_len, metric, day_field)) {
            if (!append_text(out, cap, len, ",\"") || !append_json_escaped(out, cap, len, p, key_len) ||
                !append_text(out, cap, len, "\":")) {
                return false;
            }
            if (is_plain_integer(value, value_len)) {
                if (!append_json_escaped(out, cap, len, value, value_len)) {
                    return false;
                }
            } else if (!append_text(out, cap, len, "\"") || !append_json_escaped(out, cap, len, value, value_len) ||
                       !append_text(out, cap, len, "\"")) {
                return false;
            }
        }
        p += pair_len + (end != NULL ? 1u : 0u);
    }
    return true;
}

static bool build_body(const leaderboard_http_t *self, leaderboard_board_t board, const http_board_t *b,
                       char out[HTTP_BODY_MAX]) {
    const leaderboard_board_def_t *def = leaderboard_board_def(board);
    char metric[HTTP_METRIC_MAX];
    char day_field[HTTP_METRIC_MAX + 4];
    char day[LB_DAY_STR_MAX];
    metric_name(def, metric);
    snprintf(day_field, sizeof day_field, "%s_day", metric);
    current_day(self, day);
    size_t len = 0;
    out[0] = '\0';
    if (!append_text(out, HTTP_BODY_MAX, &len, "{\"user_id\":\"") ||
        !append_json_escaped(out, HTTP_BODY_MAX, &len, leaderboard_player_id(), strlen(leaderboard_player_id())) ||
        !append_text(out, HTTP_BODY_MAX, &len, "\"")) {
        return false;
    }
    /* The facade's current value per scope: best, else what the server already
     * accepted. A body missing a scope would zero that counter on the server. */
    for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
        if (!scope_declared(def, (leaderboard_scope_t)s)) {
            continue;
        }
        leaderboard_view_t view;
        leaderboard_view_get(board, (leaderboard_scope_t)s, &view);
        char field[HTTP_METRIC_MAX + 32];
        snprintf(field, sizeof field, ",\"%s\":%u", s == LEADERBOARD_SCOPE_UTC_DAY ? day_field : metric,
                 (unsigned)view.value);
        if (!append_text(out, HTTP_BODY_MAX, &len, field)) {
            return false;
        }
    }
    char day_pair[LB_DAY_STR_MAX + 16];
    snprintf(day_pair, sizeof day_pair, ",\"day\":\"%s\"", day);
    if (!append_text(out, HTTP_BODY_MAX, &len, day_pair) ||
        !append_extra_fields(out, HTTP_BODY_MAX, &len, b->extra, metric, day_field) ||
        !append_text(out, HTTP_BODY_MAX, &len, "}")) {
        return false;
    }
    return true;
}

/* ---- pages ---- */

/* Rows the facade borrows during a completion; one board at a time, and the
 * facade copies before returning. */
static leaderboard_row_t s_rows[LEADERBOARD_TOP_MAX];

static void complete_scope_page(leaderboard_board_t board, http_board_t *b, leaderboard_scope_t scope,
                                uint32_t value) {
    const char *player = leaderboard_player_id();
    lb_board_t ranked = scope == LEADERBOARD_SCOPE_UTC_DAY ? b->response.day : b->response.all;
    const int place = lb_recalc_place(&ranked, player, b->extra, value);
    int count = 0;
    for (int i = 0; i < ranked.top_count && count < LEADERBOARD_TOP_MAX; i++) {
        const lb_top_entry_t *e = &ranked.top[i];
        const bool you = strcmp(e->user_id, player) == 0;
        /* zero rows exist only when the board held nobody but the player */
        if (e->value == 0 && !you) {
            break;
        }
        leaderboard_row_t *row = &s_rows[count++];
        memset(row, 0, sizeof *row);
        row->value = e->value;
        row->place = i + 1;
        row->you = you;
        snprintf(row->extra, sizeof row->extra, "%s", e->extra);
    }
    const leaderboard_page_t page = {
        .top = s_rows,
        .top_count = count,
        .around = NULL,
        .around_count = 0,
        .has_player = true,
        .player_place = place,
        .player_value = value,
        .day = scope == LEADERBOARD_SCOPE_UTC_DAY ? b->response.day_str : NULL,
    };
    leaderboard_backend_complete_fetch(board, scope, &page, LEADERBOARD_RESULT_OK);
}

static void complete_pages(leaderboard_board_t board, http_board_t *b) {
    const leaderboard_board_def_t *def = leaderboard_board_def(board);
    for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
        if (!scope_declared(def, (leaderboard_scope_t)s)) {
            continue;
        }
        leaderboard_view_t view;
        leaderboard_view_get(board, (leaderboard_scope_t)s, &view);
        complete_scope_page(board, b, (leaderboard_scope_t)s, view.value);
    }
}

static void complete_fetches_failed(leaderboard_board_t board) {
    const leaderboard_board_def_t *def = leaderboard_board_def(board);
    for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
        if (scope_declared(def, (leaderboard_scope_t)s)) {
            leaderboard_backend_complete_fetch(board, (leaderboard_scope_t)s, NULL, LEADERBOARD_RESULT_FAILED);
        }
    }
}

static void complete_submits(leaderboard_board_t board, http_board_t *b, leaderboard_result_t result) {
    for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
        if (b->submit[s] == SUBMIT_IN_FLIGHT) {
            b->submit[s] = SUBMIT_NONE;
            leaderboard_backend_complete_submit(board, (leaderboard_scope_t)s, result);
        }
    }
}

static bool any_submit_queued(const http_board_t *b) {
    for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
        if (b->submit[s] == SUBMIT_QUEUED) {
            return true;
        }
    }
    return false;
}

/* ---- request lifecycle ---- */

static void start_request(leaderboard_http_t *self, leaderboard_board_t board, http_board_t *b) {
    const leaderboard_http_transport_t *t = transport_of(self);
    for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
        if (b->submit[s] == SUBMIT_QUEUED) {
            b->submit[s] = SUBMIT_IN_FLIGHT;
        }
    }
    char body[HTTP_BODY_MAX];
    char payload[HTTP_PAYLOAD_MAX];
    char url[HTTP_URL_MAX];
    bool started = build_body(self, board, b, body) &&
                   lb_encode_payload(body, self->config.key, payload, sizeof payload) != 0;
    if (started) {
        const int n = snprintf(url, sizeof url, "%s?d=%s", self->config.url, payload);
        started = n > 0 && (size_t)n < sizeof url;
    }
    if (started) {
        b->request = t->request(url, t->userdata);
        started = b->request != 0;
    }
    if (!started) {
        /* Never left the client: the facade learns now, the poll retries on the
         * error cadence. */
        b->status = HTTP_ERROR;
        b->last_response = t->monotonic_now(t->userdata);
        complete_submits(board, b, LEADERBOARD_RESULT_FAILED);
        complete_fetches_failed(board);
        return;
    }
    b->status = HTTP_LOADING;
}

static void anchor_day(leaderboard_http_t *self, const http_board_t *b) {
    http_state_t *st = state_of(self);
    const leaderboard_http_transport_t *t = transport_of(self);
    char local[LB_DAY_STR_MAX];
    lb_utc_day((time_t)t->unix_now(t->userdata), local);
    const int64_t server_days = lb_day_number(b->response.day_str);
    const int64_t local_days = lb_day_number(local);
    if (server_days >= 0 && local_days >= 0) {
        st->day_offset = (server_days - local_days) * 86400;
    }
}

static void finish_request(leaderboard_http_t *self, leaderboard_board_t board, http_board_t *b, bool ok,
                           const uint8_t *data, uint32_t size) {
    const leaderboard_http_transport_t *t = transport_of(self);
    const leaderboard_board_def_t *def = leaderboard_board_def(board);
    char metric[HTTP_METRIC_MAX];
    metric_name(def, metric);
    b->last_response = t->monotonic_now(t->userdata);
    lb_response_t parsed;
    /* A body the parser refuses is a failed request, never an empty board. */
    const bool loaded = ok && data != NULL && lb_parse_response((const char *)data, size, metric, &parsed);
    if (loaded) {
        b->response = parsed;
        b->status = HTTP_LOADED;
        b->errors_sequence = 0;
        anchor_day(self, b);
        /* Submits first: a page that rolls the day must not leave the value
         * of the old day recorded as sent on the new one. */
        complete_submits(board, b, LEADERBOARD_RESULT_OK);
        complete_pages(board, b);
    } else {
        b->status = HTTP_ERROR;
        b->errors_sequence += 1;
        complete_fetches_failed(board);
        complete_submits(board, b, LEADERBOARD_RESULT_FAILED);
    }
    /* A score that arrived mid-flight is the newer truth: it goes out now, not
     * on the repeat timer, which a short session never reaches. Completions
     * above may already have started that request. */
    if (b->status != HTTP_LOADING && any_submit_queued(b)) {
        start_request(self, board, b);
    }
}

static void poll_board(leaderboard_http_t *self, leaderboard_board_t board, http_board_t *b) {
    const leaderboard_http_transport_t *t = transport_of(self);
    const double now = t->monotonic_now(t->userdata);
    switch (b->status) {
    case HTTP_IDLE:
        start_request(self, board, b);
        break;
    case HTTP_LOADING: {
        const leaderboard_http_state_t st = t->state(b->request, t->userdata);
        if (st == LEADERBOARD_HTTP_DONE || st == LEADERBOARD_HTTP_FAILED) {
            uint32_t size = 0;
            uint8_t *data = st == LEADERBOARD_HTTP_DONE ? t->take_body(b->request, &size, t->userdata) : NULL;
            t->release(b->request, t->userdata);
            b->request = 0;
            finish_request(self, board, b, st == LEADERBOARD_HTTP_DONE, data, size);
            free(data);
        }
        break;
    }
    case HTTP_LOADED:
        if (now - b->last_response > self->config.repeat_delay_s) {
            start_request(self, board, b);
        }
        break;
    case HTTP_ERROR: {
        const double delay = b->errors_sequence < self->config.error_fast_tries ? self->config.error_delay_s
                                                                                 : self->config.repeat_delay_s;
        if (now - b->last_response > delay) {
            start_request(self, board, b);
        }
        break;
    }
    }
}

/* ---- backend vtable ---- */

static leaderboard_caps_t http_caps(leaderboard_board_t board, void *ud) {
    leaderboard_caps_t caps = {0};
    leaderboard_http_t *self = self_of(ud);
    http_board_t *b;
    if (!ready(self, board, &b)) {
        return caps;
    }
    caps.can_read = true;
    caps.can_write = true;
    caps.scopes = (1u << LEADERBOARD_SCOPE_ALL_TIME) | (1u << LEADERBOARD_SCOPE_UTC_DAY);
    return caps;
}

static bool http_init(void *ud) {
    leaderboard_http_t *self = self_of(ud);
    if (self == NULL) {
        return false;
    }
#if !defined(LEADERBOARD_TESTING)
    if (self->config.transport == NULL) {
        self->config.transport = &k_engine_transport;
    }
#endif
    if (self->config.transport == NULL) {
        return false;
    }
    free(self->state);
    self->state = calloc(1, sizeof(http_state_t));
    return self->state != NULL;
}

static bool http_submit(leaderboard_board_t board, leaderboard_scope_t scope, uint32_t value, const char *extra,
                        void *ud) {
    leaderboard_http_t *self = self_of(ud);
    http_board_t *b;
    if (!ready(self, board, &b) || (int)scope < 0 || (int)scope >= LEADERBOARD_SCOPE_COUNT) {
        return false;
    }
    snprintf(b->extra, sizeof b->extra, "%s", extra != NULL ? extra : "");
    /* The place moves with the score before the server confirms it, exactly
     * as the counter does: the last top is re-ranked around the new value. */
    if (b->response.valid) {
        complete_scope_page(board, b, scope, value);
    }
    b->submit[scope] = SUBMIT_QUEUED;
    if (b->status != HTTP_LOADING) {
        start_request(self, board, b);
    }
    return true;
}

static bool http_fetch(leaderboard_board_t board, leaderboard_scope_t scope, void *ud) {
    (void)scope;
    leaderboard_http_t *self = self_of(ud);
    http_board_t *b;
    if (!ready(self, board, &b)) {
        return false;
    }
    /* One response serves both scopes; a request in flight already answers. */
    if (b->status != HTTP_LOADING) {
        start_request(self, board, b);
    }
    return true;
}

static bool http_open_native(leaderboard_board_t board, void *ud) {
    (void)board;
    (void)ud;
    return false;
}

static void http_update(void *ud) {
    leaderboard_http_t *self = self_of(ud);
    http_state_t *st = state_of(self);
    if (st == NULL || dormant(self)) {
        return;
    }
    for (int i = 0; i < LEADERBOARD_MAX_BOARDS; i++) {
        const leaderboard_board_t board = {(uint8_t)i};
        if (leaderboard_board_def(board) == NULL) {
            break;
        }
        poll_board(self, board, &st->boards[i]);
    }
}

static void http_destroy(void *ud) {
    leaderboard_http_t *self = self_of(ud);
    http_state_t *st = state_of(self);
    if (st == NULL) {
        return;
    }
    const leaderboard_http_transport_t *t = transport_of(self);
    for (int i = 0; i < LEADERBOARD_MAX_BOARDS; i++) {
        if (st->boards[i].request != 0 && t != NULL) {
            t->release(st->boards[i].request, t->userdata);
        }
    }
    free(st);
    self->state = NULL;
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
