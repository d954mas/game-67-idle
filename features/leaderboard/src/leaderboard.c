#if defined(_WIN32)
#define _CRT_RAND_S
#endif

#include "features/leaderboard/leaderboard.h"
#include "features/leaderboard/leaderboard_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if !defined(_WIN32)
#if defined(__EMSCRIPTEN__) || defined(__linux__)
#include <sys/random.h>
#else
#include <unistd.h>
#endif
#endif

/* The facade: capability answers, refusal latches, per-(board, scope)
 * coalescing and the host keys. Every backend answers through the completion
 * entry points; nothing here does I/O beyond the host callbacks. */

/* FAILED answers before the screen is told the retries are spent. A design
 * knob for the retry feel; only its existence is asserted. */
#define LEADERBOARD_FAILED_BUDGET 3
#define LEADERBOARD_RETRY_DELAY_S 5
#define LEADERBOARD_RATE_LIMIT_DELAY_S 15

/* "lb.sent." + id + "." + scope name, with room to spare. */
#define LEADERBOARD_KEY_MAX 64

typedef struct {
    leaderboard_view_t view;
    /* The coalesced best the game handed us; the value the screen shows. */
    bool has_best;
    uint32_t best;
    char extra[LEADERBOARD_EXTRA_MAX];
    /* What the portal accepted; a relaunch must not send it again. */
    bool has_sent;
    uint32_t sent;
    bool submit_in_flight;
    uint32_t in_flight;
    char in_flight_day[LB_DAY_STR_MAX];
    bool fetch_in_flight;
    bool fetch_requested;
    bool fetch_waits_for_login;
    int64_t submit_retry_at;
    int64_t fetch_retry_at;
    /* The player's value as the last page reported it; shown until the game submits. */
    bool has_page_value;
    uint32_t page_value;
    int fetch_failures;
    int submit_failures;
} leaderboard_slot_t;

typedef struct {
    bool initialised;
    bool backend_ready;
    leaderboard_config_t config;
    leaderboard_board_def_t boards[LEADERBOARD_MAX_BOARDS];
    int board_count;
    char player_id[LEADERBOARD_PLAYER_ID_MAX];
    char day[LB_DAY_STR_MAX];
    char day_label[32];
    /* Server day minus local day, in seconds: every rollover then happens at
     * the server's midnight, online or not. */
    int64_t day_offset;
    bool read_refused[LEADERBOARD_MAX_BOARDS];
    bool write_refused[LEADERBOARD_MAX_BOARDS];
    bool needs_login[LEADERBOARD_MAX_BOARDS];
    char extra[LEADERBOARD_MAX_BOARDS][LEADERBOARD_EXTRA_MAX];
    leaderboard_slot_t slots[LEADERBOARD_MAX_BOARDS][LEADERBOARD_SCOPE_COUNT];
} leaderboard_runtime_t;

static leaderboard_runtime_t g_lb;
#if defined(LEADERBOARD_TESTING)
static int64_t g_lb_test_now;
#endif

static const char *const k_scope_names[LEADERBOARD_SCOPE_COUNT] = {"all_time", "utc_day"};

/* ---- clock ---- */

static int64_t now_seconds(void) {
#if defined(LEADERBOARD_TESTING)
    if (g_lb_test_now != 0) {
        return g_lb_test_now;
    }
#endif
    return (int64_t)time(NULL);
}

static int64_t effective_now(void) {
    return now_seconds() + g_lb.day_offset;
}

/* ---- board addressing ---- */

static bool board_valid(leaderboard_board_t board) {
    return g_lb.initialised && board.index < g_lb.board_count;
}

static bool scope_valid(leaderboard_scope_t scope) {
    return (int)scope >= 0 && (int)scope < LEADERBOARD_SCOPE_COUNT;
}

static uint32_t scope_bit(leaderboard_scope_t scope) {
    return 1u << (unsigned)scope;
}

static leaderboard_slot_t *slot_of(leaderboard_board_t board, leaderboard_scope_t scope) {
    return &g_lb.slots[board.index][scope];
}

static bool better(leaderboard_sort_t sort, uint32_t candidate, uint32_t incumbent) {
    return sort == LEADERBOARD_SORT_ASC ? candidate < incumbent : candidate > incumbent;
}

/* ---- host keys ---- */

static bool host_load(const char *key, char *out, size_t out_size) {
    out[0] = '\0';
    if (g_lb.config.host.load == NULL) {
        return false;
    }
    return g_lb.config.host.load(key, out, out_size, g_lb.config.host.userdata) && out[0] != '\0';
}

static void host_store(const char *key, const char *value) {
    if (g_lb.config.host.store != NULL) {
        g_lb.config.host.store(key, value, g_lb.config.host.userdata);
    }
}

static bool host_load_u32(const char *key, uint32_t *out) {
    char text[32];
    if (!host_load(key, text, sizeof text)) {
        return false;
    }
    char *end = NULL;
    const unsigned long v = strtoul(text, &end, 10);
    if (end == text || *end != '\0' || v > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)v;
    return true;
}

static void host_store_u32(const char *key, uint32_t value) {
    char text[16];
    snprintf(text, sizeof text, "%u", (unsigned)value);
    host_store(key, text);
}

static void sent_key(char out[LEADERBOARD_KEY_MAX], leaderboard_board_t board, leaderboard_scope_t scope) {
    snprintf(out, LEADERBOARD_KEY_MAX, "lb.sent.%s.%s", g_lb.boards[board.index].id, k_scope_names[scope]);
}

static void day_value_key(char out[LEADERBOARD_KEY_MAX], leaderboard_board_t board) {
    snprintf(out, LEADERBOARD_KEY_MAX, "lb.day.%s", g_lb.boards[board.index].id);
}

/* ---- anonymous id ---- */

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static bool platform_entropy(uint8_t out[16]) {
#if defined(_WIN32)
    for (int i = 0; i < 16; i += 4) {
        unsigned int r = 0;
        if (rand_s(&r) != 0) {
            return false;
        }
        memcpy(out + i, &r, 4);
    }
    return true;
#else
    return getentropy(out, 16) == 0; /* wasm + posix */
#endif
}

static void ensure_player_id(void) {
    if (host_load("lb.id", g_lb.player_id, sizeof g_lb.player_id) && strlen(g_lb.player_id) >= 32) {
        return;
    }
    uint8_t b[16];
    if (!platform_entropy(b)) {
        /* clocks-and-address fallback: collision-prone, but only reachable
         * when the platform CSPRNG itself fails */
        uint64_t seed = (uint64_t)time(NULL);
        seed ^= (uint64_t)clock();
        seed ^= (uint64_t)(uintptr_t)&g_lb * 0x9E3779B97F4A7C15ULL;
        for (int i = 0; i < 16; i += 8) {
            const uint64_t r = splitmix64(&seed);
            memcpy(b + i, &r, 8);
        }
    }
    b[6] = (uint8_t)((b[6] & 0x0F) | 0x40); /* uuid v4 layout */
    b[8] = (uint8_t)((b[8] & 0x3F) | 0x80);
    snprintf(g_lb.player_id, sizeof g_lb.player_id,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
             b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
    host_store("lb.id", g_lb.player_id);
}

/* ---- day ---- */

static void set_day_label(void) {
    if (strlen(g_lb.day) != 8) {
        g_lb.day_label[0] = '\0';
        return;
    }
    snprintf(g_lb.day_label, sizeof g_lb.day_label, "%.4s.%.2s.%.2s (UTC+0)",
             g_lb.day, g_lb.day + 4, g_lb.day + 6);
}

static void reset_slot_view(leaderboard_slot_t *slot) {
    memset(&slot->view, 0, sizeof slot->view);
    slot->view.place = -1;
    slot->has_page_value = false;
}

/* Yesterday's day-scope keys must not be read back as today's: an empty value
 * is what `load` reports as absent. */
static void clear_day_keys(leaderboard_board_t board) {
    char key[LEADERBOARD_KEY_MAX];
    sent_key(key, board, LEADERBOARD_SCOPE_UTC_DAY);
    host_store(key, "");
    day_value_key(key, board);
    host_store(key, "");
}

/* A later day than the counters belong to resets every day scope; an earlier
 * one (a clock that ran ahead of the server) keeps them. */
static void roll_day_if_needed(void) {
    char today[LB_DAY_STR_MAX];
    lb_utc_day((time_t)effective_now(), today);
    if (strcmp(today, g_lb.day) <= 0) {
        return;
    }
    snprintf(g_lb.day, sizeof g_lb.day, "%s", today);
    host_store("lb.day", g_lb.day);
    set_day_label();
    for (int b = 0; b < g_lb.board_count; b++) {
        leaderboard_slot_t *slot = &g_lb.slots[b][LEADERBOARD_SCOPE_UTC_DAY];
        slot->has_best = false;
        slot->has_sent = false;
        slot->extra[0] = '\0';
        slot->fetch_failures = 0;
        slot->submit_failures = 0;
        slot->submit_retry_at = 0;
        slot->fetch_retry_at = 0;
        reset_slot_view(slot);
        clear_day_keys((leaderboard_board_t){(uint8_t)b});
    }
}

/* ---- requests ---- */

static const leaderboard_backend_t *backend(void) {
    return g_lb.backend_ready ? g_lb.config.backend : NULL;
}

static void try_send(leaderboard_board_t board, leaderboard_scope_t scope) {
    leaderboard_slot_t *slot = slot_of(board, scope);
    const leaderboard_board_def_t *def = &g_lb.boards[board.index];
    if (slot->submit_in_flight || !slot->has_best || slot->submit_retry_at > now_seconds()) {
        return;
    }
    if (slot->has_sent && !better(def->sort, slot->best, slot->sent)) {
        return;
    }
    const leaderboard_caps_t caps = leaderboard_caps(board);
    if (!caps.can_write || (caps.scopes & scope_bit(scope)) == 0) {
        return;
    }
    const leaderboard_backend_t *be = backend();
    if (be == NULL || be->submit == NULL) {
        g_lb.write_refused[board.index] = true;
        return;
    }
    slot->submit_in_flight = true;
    slot->submit_retry_at = 0;
    slot->in_flight = slot->best;
    snprintf(slot->in_flight_day, sizeof slot->in_flight_day, "%s", g_lb.day);
    const bool started = be->submit(board, scope, slot->best, slot->extra, g_lb.config.backend_userdata);
    /* A backend may complete inside the call; only a request that never
     * started is still in flight here. */
    if (!started && slot->submit_in_flight) {
        leaderboard_backend_complete_submit(board, scope, LEADERBOARD_RESULT_FAILED);
    }
}

static void request_fetch(leaderboard_board_t board, leaderboard_scope_t scope) {
    leaderboard_slot_t *slot = slot_of(board, scope);
    if (slot->fetch_in_flight || slot->fetch_retry_at > now_seconds()) {
        return;
    }
    const leaderboard_backend_t *be = backend();
    if (be != NULL && be->initializing != NULL && be->initializing(g_lb.config.backend_userdata)) {
        return;
    }
    const leaderboard_caps_t caps = leaderboard_caps(board);
    if ((caps.scopes & scope_bit(scope)) == 0 || g_lb.read_refused[board.index]) {
        slot->fetch_requested = false;
        return;
    }
    if (!caps.can_read) {
        slot->fetch_requested = false;
        return;
    }
    if (be == NULL || be->fetch == NULL) {
        g_lb.read_refused[board.index] = true;
        return;
    }
    slot->fetch_in_flight = true;
    slot->fetch_requested = false;
    slot->fetch_retry_at = 0;
    const bool started = be->fetch(board, scope, g_lb.config.backend_userdata);
    if (!started && slot->fetch_in_flight) {
        leaderboard_backend_complete_fetch(board, scope, NULL, LEADERBOARD_RESULT_FAILED);
    }
}

static void update_error(leaderboard_slot_t *slot) {
    slot->view.error = slot->fetch_failures >= LEADERBOARD_FAILED_BUDGET ||
                       slot->submit_failures >= LEADERBOARD_FAILED_BUDGET;
}

static int64_t retry_at(int failures, leaderboard_result_t result) {
    const leaderboard_backend_t *be = backend();
    if (be == NULL || be->owns_retry_cadence) {
        return 0;
    }
    if (result == LEADERBOARD_RESULT_RATE_LIMITED) {
        return now_seconds() + LEADERBOARD_RATE_LIMIT_DELAY_S;
    }
    if (result == LEADERBOARD_RESULT_FAILED && failures < LEADERBOARD_FAILED_BUDGET) {
        return now_seconds() + LEADERBOARD_RETRY_DELAY_S * failures;
    }
    return 0;
}

static void apply_page(leaderboard_board_t board, leaderboard_scope_t scope, const leaderboard_page_t *page) {
    leaderboard_slot_t *slot = slot_of(board, scope);
    leaderboard_view_t *view = &slot->view;
    view->top_count = 0;
    if (page->top != NULL) {
        for (int i = 0; i < page->top_count && i < LEADERBOARD_TOP_MAX; i++) {
            view->top[view->top_count++] = page->top[i];
        }
    }
    view->around_count = 0;
    if (page->around != NULL) {
        for (int i = 0; i < page->around_count && i < LEADERBOARD_AROUND_MAX; i++) {
            view->around[view->around_count++] = page->around[i];
        }
    }
    if (page->has_player) {
        view->place = page->player_place;
        slot->has_page_value = true;
        slot->page_value = page->player_value;
    }
    view->loaded = true;
    if (scope == LEADERBOARD_SCOPE_UTC_DAY && page->day != NULL) {
        char local[LB_DAY_STR_MAX];
        lb_utc_day((time_t)now_seconds(), local);
        const int64_t server_days = lb_day_number(page->day);
        const int64_t local_days = lb_day_number(local);
        if (server_days >= 0 && local_days >= 0) {
            g_lb.day_offset = (server_days - local_days) * 86400;
            roll_day_if_needed();
        }
    }
}

/* ---- backend entry points ---- */

void leaderboard_backend_complete_fetch(leaderboard_board_t board, leaderboard_scope_t scope,
                                        const leaderboard_page_t *page,
                                        leaderboard_result_t result) {
    if (!board_valid(board) || !scope_valid(scope)) {
        return;
    }
    leaderboard_slot_t *slot = slot_of(board, scope);
    slot->fetch_in_flight = false;
    slot->fetch_requested = false;
    slot->fetch_retry_at = 0;
    slot->fetch_waits_for_login = false;
    switch (result) {
    case LEADERBOARD_RESULT_OK:
        if (page != NULL) {
            apply_page(board, scope, page);
        }
        slot->fetch_failures = 0;
        break;
    case LEADERBOARD_RESULT_UNSUPPORTED:
        g_lb.read_refused[board.index] = true;
        break;
    case LEADERBOARD_RESULT_NEEDS_LOGIN:
        /* The top is real; only the player's own row is missing. */
        g_lb.needs_login[board.index] = true;
        slot->fetch_waits_for_login = true;
        if (page != NULL) {
            apply_page(board, scope, page);
        }
        break;
    case LEADERBOARD_RESULT_RATE_LIMITED:
        break;
    case LEADERBOARD_RESULT_FAILED:
        if (slot->fetch_failures < LEADERBOARD_FAILED_BUDGET) slot->fetch_failures++;
        break;
    }
    update_error(slot);
    slot->fetch_retry_at = retry_at(slot->fetch_failures, result);
    slot->fetch_requested = slot->fetch_retry_at != 0;
}

void leaderboard_backend_complete_submit(leaderboard_board_t board, leaderboard_scope_t scope,
                                         leaderboard_result_t result) {
    if (!board_valid(board) || !scope_valid(scope)) {
        return;
    }
    leaderboard_slot_t *slot = slot_of(board, scope);
    if (!slot->submit_in_flight) {
        return;
    }
    slot->submit_in_flight = false;
    slot->submit_retry_at = 0;
    if (scope == LEADERBOARD_SCOPE_UTC_DAY && strcmp(slot->in_flight_day, g_lb.day) != 0) {
        try_send(board, scope);
        return;
    }
    switch (result) {
    case LEADERBOARD_RESULT_OK: {
        char key[LEADERBOARD_KEY_MAX];
        slot->has_sent = true;
        slot->sent = slot->in_flight;
        sent_key(key, board, scope);
        host_store_u32(key, slot->sent);
        slot->submit_failures = 0;
        g_lb.needs_login[board.index] = false;
        try_send(board, scope); /* a better value may have arrived meanwhile */
        break;
    }
    case LEADERBOARD_RESULT_UNSUPPORTED:
        g_lb.write_refused[board.index] = true;
        break;
    case LEADERBOARD_RESULT_NEEDS_LOGIN:
        g_lb.needs_login[board.index] = true;
        break;
    case LEADERBOARD_RESULT_RATE_LIMITED:
        break;
    case LEADERBOARD_RESULT_FAILED:
        if (slot->submit_failures < LEADERBOARD_FAILED_BUDGET) slot->submit_failures++;
        break;
    }
    update_error(slot);
    if (result != LEADERBOARD_RESULT_OK) {
        slot->submit_retry_at = retry_at(slot->submit_failures, result);
    }
}

void leaderboard_backend_auth_changed(void) {
    if (!g_lb.initialised) {
        return;
    }
    for (int b = 0; b < g_lb.board_count; b++) {
        g_lb.needs_login[b] = false;
        for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
            g_lb.slots[b][s].submit_retry_at = 0;
            if (g_lb.slots[b][s].fetch_waits_for_login) {
                g_lb.slots[b][s].fetch_waits_for_login = false;
                g_lb.slots[b][s].fetch_requested = true;
            }
            try_send((leaderboard_board_t){(uint8_t)b}, (leaderboard_scope_t)s);
        }
    }
}

const char *leaderboard_player_id(void) {
    return g_lb.player_id;
}

const char *leaderboard_extra(leaderboard_board_t board) {
    return board_valid(board) ? g_lb.extra[board.index] : "";
}

const leaderboard_board_def_t *leaderboard_board_def(leaderboard_board_t board) {
    return board_valid(board) ? &g_lb.boards[board.index] : NULL;
}

/* ---- lifecycle ---- */

void leaderboard_init(const leaderboard_config_t *config) {
    memset(&g_lb, 0, sizeof g_lb);
    if (config == NULL) {
        return;
    }
    g_lb.config = *config;
    g_lb.board_count = config->boards != NULL ? config->board_count : 0;
    if (g_lb.board_count > LEADERBOARD_MAX_BOARDS) {
        g_lb.board_count = LEADERBOARD_MAX_BOARDS;
    }
    if (g_lb.board_count < 0) {
        g_lb.board_count = 0;
    }
    for (int b = 0; b < g_lb.board_count; b++) {
        g_lb.boards[b] = config->boards[b];
        if (g_lb.boards[b].id == NULL) {
            g_lb.boards[b].id = "";
        }
        for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
            reset_slot_view(&g_lb.slots[b][s]);
        }
    }
    g_lb.initialised = true;
    ensure_player_id();

    /* Day-scope counters are only trusted when they belong to today. */
    char today[LB_DAY_STR_MAX];
    lb_utc_day((time_t)effective_now(), today);
    char stored_day[LB_DAY_STR_MAX];
    const bool same_day = host_load("lb.day", stored_day, sizeof stored_day) && strcmp(stored_day, today) == 0;
    snprintf(g_lb.day, sizeof g_lb.day, "%s", today);
    if (!same_day) {
        host_store("lb.day", g_lb.day);
    }
    set_day_label();

    for (int b = 0; b < g_lb.board_count; b++) {
        const leaderboard_board_t board = {(uint8_t)b};
        char key[LEADERBOARD_KEY_MAX];
        if (!same_day) {
            clear_day_keys(board);
        }
        for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
            leaderboard_slot_t *slot = &g_lb.slots[b][s];
            sent_key(key, board, (leaderboard_scope_t)s);
            slot->has_sent = host_load_u32(key, &slot->sent);
        }
        leaderboard_slot_t *day_slot = &g_lb.slots[b][LEADERBOARD_SCOPE_UTC_DAY];
        day_value_key(key, board);
        if (host_load_u32(key, &day_slot->best)) {
            day_slot->has_best = true;
        }
    }

    const leaderboard_backend_t *be = config->backend;
    g_lb.backend_ready = be != NULL && (be->init == NULL || be->init(config->backend_userdata));
    if (!g_lb.backend_ready) {
        return;
    }
    /* A score the portal never accepted (closed tab, refused login) is not
     * stranded until the next increment. */
    for (int b = 0; b < g_lb.board_count; b++) {
        for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
            try_send((leaderboard_board_t){(uint8_t)b}, (leaderboard_scope_t)s);
        }
    }
}

void leaderboard_update(void) {
    if (!g_lb.initialised) {
        return;
    }
    roll_day_if_needed();
    const leaderboard_backend_t *be = backend();
    if (be != NULL && be->update != NULL) {
        be->update(g_lb.config.backend_userdata);
    }
    for (int b = 0; b < g_lb.board_count; b++) {
        const leaderboard_board_t board = {(uint8_t)b};
        for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
            leaderboard_slot_t *slot = &g_lb.slots[b][s];
            if (slot->submit_retry_at != 0 && slot->submit_retry_at <= now_seconds()) {
                try_send(board, (leaderboard_scope_t)s);
            }
            if (slot->fetch_requested) {
                request_fetch(board, (leaderboard_scope_t)s);
            }
        }
    }
}

void leaderboard_shutdown(void) {
    const leaderboard_backend_t *be = backend();
    if (be != NULL && be->destroy != NULL) {
        be->destroy(g_lb.config.backend_userdata);
    }
    memset(&g_lb, 0, sizeof g_lb);
}

/* ---- game-facing queries ---- */

leaderboard_board_t leaderboard_board(const char *id) {
    leaderboard_board_t board = {LEADERBOARD_INVALID_BOARD};
    if (id == NULL || !g_lb.initialised) {
        return board;
    }
    for (int b = 0; b < g_lb.board_count; b++) {
        if (strcmp(g_lb.boards[b].id, id) == 0) {
            board.index = (uint8_t)b;
            return board;
        }
    }
    return board;
}

int leaderboard_board_count(void) {
    return g_lb.initialised ? g_lb.board_count : 0;
}

leaderboard_caps_t leaderboard_caps(leaderboard_board_t board) {
    leaderboard_caps_t caps = {0};
    const leaderboard_backend_t *be = backend();
    if (!board_valid(board) || be == NULL) {
        return caps;
    }
    if (be->caps != NULL) {
        caps = be->caps(board, g_lb.config.backend_userdata);
    }
    caps.scopes &= g_lb.boards[board.index].scopes;
    if (g_lb.read_refused[board.index]) {
        caps.can_read = false;
    }
    if (g_lb.write_refused[board.index] || be->submit == NULL) {
        caps.can_write = false;
    }
    if (be->fetch == NULL) {
        caps.can_read = false;
    }
    if (be->open_native == NULL) {
        caps.native_popup = false;
    }
    return caps;
}

bool leaderboard_available(leaderboard_board_t board) {
    const leaderboard_caps_t caps = leaderboard_caps(board);
    return caps.can_read || caps.can_write || caps.native_popup;
}

void leaderboard_submit(leaderboard_board_t board, leaderboard_scope_t scope,
                        uint32_t value, const char *extra) {
    if (!board_valid(board) || !scope_valid(scope)) {
        return;
    }
    const leaderboard_board_def_t *def = &g_lb.boards[board.index];
    if ((def->scopes & scope_bit(scope)) == 0) {
        return;
    }
    leaderboard_slot_t *slot = slot_of(board, scope);
    /* The payload is recorded even when the value is not an improvement: a game
       submits its standing best again at every launch, and a backend that sends
       the payload with each request would otherwise blank the row until the
       player beats their own score. NULL means "unchanged"; an empty string is
       how a caller clears it. */
    if (extra != NULL) {
        snprintf(slot->extra, sizeof slot->extra, "%s", extra);
        snprintf(g_lb.extra[board.index], sizeof g_lb.extra[board.index], "%s", extra);
    }
    if (slot->has_best && !better(def->sort, value, slot->best)) {
        return;
    }
    slot->has_best = true;
    slot->best = value;
    if (scope == LEADERBOARD_SCOPE_UTC_DAY) {
        char key[LEADERBOARD_KEY_MAX];
        day_value_key(key, board);
        host_store_u32(key, value);
    }
    try_send(board, scope);
}

void leaderboard_view_get(leaderboard_board_t board, leaderboard_scope_t scope,
                          leaderboard_view_t *out) {
    if (out == NULL) {
        return;
    }
    memset(out, 0, sizeof *out);
    out->place = -1;
    if (!board_valid(board) || !scope_valid(scope)) {
        return;
    }
    const leaderboard_slot_t *slot = slot_of(board, scope);
    *out = slot->view;
    out->needs_login = g_lb.needs_login[board.index];
    if (slot->has_best) {
        out->value = slot->best;
    } else if (slot->has_sent) {
        out->value = slot->sent;
    } else if (slot->has_page_value) {
        out->value = slot->page_value;
    }
}

void leaderboard_refresh_now(leaderboard_board_t board) {
    if (!board_valid(board)) {
        return;
    }
    for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
        leaderboard_slot_t *slot = &g_lb.slots[board.index][s];
        slot->fetch_failures = 0;
        slot->submit_failures = 0;
        slot->view.error = false;
        slot->submit_retry_at = 0;
        slot->fetch_retry_at = 0;
        slot->fetch_requested = !slot->fetch_in_flight;
        slot->fetch_waits_for_login = false;
        try_send(board, (leaderboard_scope_t)s);
        request_fetch(board, (leaderboard_scope_t)s);
    }
}

bool leaderboard_open_native(leaderboard_board_t board) {
    const leaderboard_caps_t caps = leaderboard_caps(board);
    const leaderboard_backend_t *be = backend();
    if (!caps.native_popup || be == NULL || be->open_native == NULL) {
        return false;
    }
    return be->open_native(board, g_lb.config.backend_userdata);
}

static int popcount32(uint32_t v) {
    int n = 0;
    while (v != 0) {
        n += (int)(v & 1u);
        v >>= 1;
    }
    return n;
}

leaderboard_ui_state_t leaderboard_ui_state(leaderboard_board_t board) {
    leaderboard_ui_state_t state = {0};
    if (!board_valid(board)) {
        return state;
    }
    const leaderboard_caps_t caps = leaderboard_caps(board);
    state.show_launcher = caps.can_read || caps.native_popup;
    state.tab_count = popcount32(caps.scopes);
    state.show_login = g_lb.needs_login[board.index];
    for (int s = 0; s < LEADERBOARD_SCOPE_COUNT; s++) {
        const leaderboard_slot_t *slot = &g_lb.slots[board.index][s];
        state.show_retry = state.show_retry || slot->view.error;
        state.loading = state.loading || slot->fetch_in_flight || slot->fetch_requested;
    }
    return state;
}

const char *leaderboard_day_label(void) {
    return g_lb.day_label;
}

int64_t leaderboard_seconds_to_day_reset(void) {
    return lb_seconds_until_day_end((time_t)effective_now());
}

#if defined(LEADERBOARD_TESTING)
void leaderboard_reset_for_tests(void) {
    memset(&g_lb, 0, sizeof g_lb);
    g_lb_test_now = 0;
}

void leaderboard_set_now_for_tests(int64_t unix_seconds) {
    g_lb_test_now = unix_seconds;
}
#endif
