#ifndef FEATURES_LEADERBOARD_H
#define FEATURES_LEADERBOARD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Capability facade over one leaderboard backend. The game asks what a board
 * can do and renders what the data carries; nothing here names a portal or a
 * game concept. SPEC.md next to this pack is the contract this header follows. */

#define LEADERBOARD_MAX_BOARDS 4
#define LEADERBOARD_TOP_MAX 20 /* the portal ceiling; the screen draws fewer */
#define LEADERBOARD_AROUND_MAX 10
#define LEADERBOARD_ID_MAX 32
#define LEADERBOARD_EXTRA_MAX 100 /* the game's opaque payload; the smallest portal budget */
#define LEADERBOARD_NAME_MAX 64
#define LEADERBOARD_URL_MAX 192
/* A uuid v4 in text form plus its terminator. */
#define LEADERBOARD_PLAYER_ID_MAX 40
#define LEADERBOARD_INVALID_BOARD 0xFFu

typedef enum {
    LEADERBOARD_SCOPE_ALL_TIME = 0,
    LEADERBOARD_SCOPE_UTC_DAY = 1,
    LEADERBOARD_SCOPE_COUNT = 2,
} leaderboard_scope_t;

typedef enum {
    LEADERBOARD_RESULT_OK = 0,
    LEADERBOARD_RESULT_UNSUPPORTED = 1,  /* terminal: this portal has no such board */
    LEADERBOARD_RESULT_NEEDS_LOGIN = 2,  /* recoverable: the player can fix it */
    LEADERBOARD_RESULT_RATE_LIMITED = 3, /* recoverable: wait */
    LEADERBOARD_RESULT_FAILED = 4,       /* recoverable: transport or parse */
} leaderboard_result_t;

/* Which value is "better" when two submissions for one (board, scope) coalesce. */
typedef enum {
    LEADERBOARD_SORT_DESC = 0, /* higher wins: counters, points */
    LEADERBOARD_SORT_ASC = 1,  /* lower wins: lap times */
} leaderboard_sort_t;

typedef struct {
    uint8_t index; /* LEADERBOARD_INVALID_BOARD when the id is unknown */
} leaderboard_board_t;

typedef struct {
    uint32_t value;
    int place; /* 0 when the source gives no rank */
    bool you;
    char extra[LEADERBOARD_EXTRA_MAX]; /* opaque to the pack; the game's own k=v payload */
    char name[LEADERBOARD_NAME_MAX];   /* "" on an anonymous board */
    char avatar[LEADERBOARD_URL_MAX];  /* "" when the portal gives none */
} leaderboard_row_t;

/* Borrowed for the duration of the completion call; the facade copies what it
 * keeps before returning. A backend may free its buffers immediately after. */
typedef struct {
    const leaderboard_row_t *top;
    int top_count;
    const leaderboard_row_t *around; /* neighbours of the player; NULL when the source has none */
    int around_count;
    bool has_player;
    int player_place;
    uint32_t player_value;
    const char *day; /* "YYYYMMDD" for a day scope, NULL otherwise */
} leaderboard_page_t;

typedef struct {
    bool can_read;     /* entries can be listed inside the game */
    bool can_write;    /* scores can be submitted */
    bool needs_login;  /* writing, and the player's own row, require portal auth */
    bool native_popup; /* the portal owns the UI; we can only open it */
    uint32_t scopes;   /* bitmask of (1u << leaderboard_scope_t) */
} leaderboard_caps_t;

/* One board of the game's manifest; the generated table hands these to init. */
typedef struct {
    const char *id;         /* manifest id, at most LEADERBOARD_ID_MAX - 1 chars; also the host key suffix */
    leaderboard_sort_t sort;
    uint32_t scopes;        /* bitmask of (1u << leaderboard_scope_t) the manifest declares */
    const char *portal_id;  /* the board's name at the portal of this build; NULL or "" when none */
} leaderboard_board_def_t;

/* The game's save. The pack owns no persistence: the anonymous id, the day
 * string and the per-scope "already sent" value live wherever the game keeps
 * them. `load` answers false when the key is absent. */
typedef struct {
    bool (*load)(const char *key, char *out, size_t out_size, void *userdata);
    void (*store)(const char *key, const char *value, void *userdata);
    void *userdata;
} leaderboard_host_t;

typedef struct leaderboard_backend_t leaderboard_backend_t;

typedef struct {
    leaderboard_host_t host;
    const leaderboard_backend_t *backend;
    void *backend_userdata;
    const leaderboard_board_def_t *boards;
    int board_count; /* at most LEADERBOARD_MAX_BOARDS; the rest are ignored */
} leaderboard_config_t;

typedef struct {
    int place; /* -1 until known */
    uint32_t value;
    int top_count;
    leaderboard_row_t top[LEADERBOARD_TOP_MAX];
    int around_count; /* 0 on sources that cannot rank neighbours */
    leaderboard_row_t around[LEADERBOARD_AROUND_MAX];
    bool loaded;
    bool error;       /* retries are spent; offer a manual retry */
    bool needs_login; /* the top is real, the player is simply not on it yet */
} leaderboard_view_t;

/* What a screen may show. The only thing a game's screen has to obey. */
typedef struct {
    bool show_launcher; /* can_read || native_popup */
    int tab_count;      /* popcount(caps.scopes) */
    bool show_login;
    bool show_retry;
    bool loading;
} leaderboard_ui_state_t;

/* ---- game-facing API ---- */

void leaderboard_init(const leaderboard_config_t *config);
void leaderboard_update(void); /* pump; once per frame */
void leaderboard_shutdown(void);

leaderboard_board_t leaderboard_board(const char *id);
int leaderboard_board_count(void);

leaderboard_caps_t leaderboard_caps(leaderboard_board_t board);
/* Some capability survives: reading, writing or the portal's own popup. */
bool leaderboard_available(leaderboard_board_t board);

/* Never blocks and never fails at the call site. Per (board, scope): the
 * better value per the board's sort is kept, a value the portal already
 * accepted is not sent again, and a portal that cannot take it withdraws or
 * waits per the refusal rules.
 *
 * `extra` is recorded even when the value is not an improvement, because a
 * backend that carries the payload on every request would otherwise blank the
 * row after a relaunch. NULL leaves the stored payload alone; "" clears it. */
void leaderboard_submit(leaderboard_board_t board, leaderboard_scope_t scope,
                        uint32_t value, const char *extra);
void leaderboard_view_get(leaderboard_board_t board, leaderboard_scope_t scope,
                          leaderboard_view_t *out);
/* Call on screen open or manual retry: clears errors, re-sends pending scores,
 * and fetches supported scopes, queuing while the backend initializes. */
void leaderboard_refresh_now(leaderboard_board_t board);
bool leaderboard_open_native(leaderboard_board_t board);

leaderboard_ui_state_t leaderboard_ui_state(leaderboard_board_t board);

/* Day of the day-scope boards on the server-anchored clock; "" before init. */
const char *leaderboard_day_label(void);
int64_t leaderboard_seconds_to_day_reset(void);

/* ---- the `extra` codec: "k=v;" pairs in a fixed buffer ---- */

/* Sets or replaces one pair. False, with the buffer untouched, when the pair
 * does not fit or key/value carry '=' or ';'. */
bool leaderboard_extra_set(char *extra, size_t cap, const char *key, const char *value);
/* False when the key is absent, the input is garbage or `out` is too small. */
bool leaderboard_extra_get(const char *extra, const char *key, char *out, size_t out_size);

/* ---- backend contract ---- */

/* One vtable, chosen once at init. Every call is per board; `submit` and
 * `fetch` answer through the completion entry points below, synchronously or
 * later. A false return from `submit` or `fetch` means the request never
 * started and counts as FAILED. `init` answering false makes the pack inert
 * for the session. A NULL entry answers UNSUPPORTED. */
struct leaderboard_backend_t {
    /* Polling backends own their cadence; otherwise the facade retries failures. */
    bool owns_retry_cadence;
    /* Optional startup signal; a missing reader is otherwise unavailable. */
    bool (*initializing)(void *ud);
    leaderboard_caps_t (*caps)(leaderboard_board_t board, void *ud);
    bool (*init)(void *ud);
    bool (*submit)(leaderboard_board_t board, leaderboard_scope_t scope,
                   uint32_t value, const char *extra, void *ud);
    bool (*fetch)(leaderboard_board_t board, leaderboard_scope_t scope, void *ud);
    bool (*open_native)(leaderboard_board_t board, void *ud);
    void (*update)(void *ud);
    void (*destroy)(void *ud);
};

/* Refusal rules: UNSUPPORTED latches for the session and only for the
 * capability that refused (a write refusal leaves reading alone); NEEDS_LOGIN
 * raises the board's login flag and never latches; RATE_LIMITED retries after
 * a delay. FAILED uses independent read/write budgets and exposes an error
 * when either is spent. Polling backends own their retry cadence instead. */
void leaderboard_backend_complete_fetch(leaderboard_board_t board, leaderboard_scope_t scope,
                                        const leaderboard_page_t *page,
                                        leaderboard_result_t result);
void leaderboard_backend_complete_submit(leaderboard_board_t board, leaderboard_scope_t scope,
                                         leaderboard_result_t result);
/* The portal's auth state changed either way: pending scores are re-sent, so a
 * value refused with NEEDS_LOGIN reaches the board once the player logged in. */
void leaderboard_backend_auth_changed(void);

/* The anonymous id the host stores under "lb.id"; "" before init. Backends
 * without portal identity key their rows by it. */
const char *leaderboard_player_id(void);
/* Latest non-NULL payload submitted for this board, even when the score was
 * already accepted. Borrowed until the next submit or shutdown. */
const char *leaderboard_extra(leaderboard_board_t board);
/* The board definition init received; NULL for an invalid board. */
const leaderboard_board_def_t *leaderboard_board_def(leaderboard_board_t board);

#if defined(LEADERBOARD_TESTING)
void leaderboard_reset_for_tests(void);
/* 0 restores the wall clock. */
void leaderboard_set_now_for_tests(int64_t unix_seconds);
#endif

#endif /* FEATURES_LEADERBOARD_H */
