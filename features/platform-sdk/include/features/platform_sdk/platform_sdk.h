#ifndef FEATURES_PLATFORM_SDK_PLATFORM_SDK_H
#define FEATURES_PLATFORM_SDK_PLATFORM_SDK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum platform_target_t {
    PLATFORM_TARGET_LOCAL = 0,
    PLATFORM_TARGET_ITCH = 1,
    PLATFORM_TARGET_POKI = 2,
    PLATFORM_TARGET_YANDEX = 3,
    PLATFORM_TARGET_PLAYGAMA = 4,
    PLATFORM_TARGET_CRAZYGAMES = 5,
    PLATFORM_TARGET_WAVEDASH = 6,
} platform_target_t;

typedef enum platform_sdk_t {
    PLATFORM_SDK_MOCK = 0,
    PLATFORM_SDK_POKI = 1,
    PLATFORM_SDK_YANDEX = 2,
    PLATFORM_SDK_PLAYGAMA = 3,
    PLATFORM_SDK_CRAZYGAMES = 4,
    PLATFORM_SDK_WAVEDASH = 5,
} platform_sdk_t;

typedef struct platform_sdk_capabilities_t {
    bool external_links_allowed;
    bool ads_supported;
    bool rewarded_supported;
    bool storage_supported;
    /* The portal has a login dialog of its own. Only Yandex ties anything the
       game cares about (leaderboard writes, the player's own row) to it. */
    bool auth_supported;
} platform_sdk_capabilities_t;

typedef enum platform_sdk_boot_status_t {
    PLATFORM_SDK_BOOT_NOT_STARTED = 0,
    PLATFORM_SDK_BOOT_INITIALIZING = 1,
    PLATFORM_SDK_BOOT_READY = 2,
    PLATFORM_SDK_BOOT_FAILED = 3,
    PLATFORM_SDK_BOOT_DESTROYED = 4,
} platform_sdk_boot_status_t;

typedef enum platform_sdk_result_t {
    PLATFORM_SDK_RESULT_OK = 0,
    PLATFORM_SDK_RESULT_NOT_READY = 1,
    PLATFORM_SDK_RESULT_DESTROYED = 2,
    PLATFORM_SDK_RESULT_WAITING_FOR_INPUT = 3,
    PLATFORM_SDK_RESULT_ALREADY_ACTIVE = 4,
    PLATFORM_SDK_RESULT_NOT_ACTIVE = 5,
    PLATFORM_SDK_RESULT_UNSUPPORTED = 6,
    PLATFORM_SDK_RESULT_BUSY = 7,
    PLATFORM_SDK_RESULT_FAILED = 8,
} platform_sdk_result_t;

typedef enum platform_sdk_ad_reason_t {
    PLATFORM_SDK_AD_REASON_NONE = 0,
    PLATFORM_SDK_AD_REASON_UNSUPPORTED = 1,
    PLATFORM_SDK_AD_REASON_NOT_READY = 2,
    PLATFORM_SDK_AD_REASON_RATE_LIMITED = 3,
    PLATFORM_SDK_AD_REASON_FAILED = 4,
    PLATFORM_SDK_AD_REASON_SKIPPED = 5,
    PLATFORM_SDK_AD_REASON_DECLINED = 6,
    PLATFORM_SDK_AD_REASON_COMPLETED = 7,
    PLATFORM_SDK_AD_REASON_TIMEOUT = 8,
} platform_sdk_ad_reason_t;

typedef struct platform_sdk_ad_result_t {
    bool supported;
    bool shown;
    platform_sdk_ad_reason_t reason;
} platform_sdk_ad_result_t;

typedef struct platform_sdk_rewarded_result_t {
    bool supported;
    bool shown;
    bool rewarded;
    platform_sdk_ad_reason_t reason;
} platform_sdk_rewarded_result_t;

typedef enum platform_sdk_auth_reason_t {
    PLATFORM_SDK_AUTH_REASON_NONE = 0,
    PLATFORM_SDK_AUTH_REASON_UNSUPPORTED = 1,
    PLATFORM_SDK_AUTH_REASON_NOT_READY = 2,
    PLATFORM_SDK_AUTH_REASON_FAILED = 3,
    /* The player closed the dialog. A normal outcome: nothing to show, nothing
       to log above debug level. */
    PLATFORM_SDK_AUTH_REASON_DECLINED = 4,
    PLATFORM_SDK_AUTH_REASON_ACCEPTED = 5,
} platform_sdk_auth_reason_t;

/* Name and avatar are what the portal supplied, or "" -- an anonymous Yandex
   player has an id and nothing else. Strings are copied by the facade. */
typedef struct platform_sdk_auth_result_t {
    bool supported;
    bool authorized;
    platform_sdk_auth_reason_t reason;
    const char *name;
    const char *avatar_url;
} platform_sdk_auth_result_t;

/* Leaderboards. A board is named by the id the portal console knows it by;
   nothing here knows what the score measures. The portal answers are the
   four refusal kinds the leaderboard pack acts on, so no caller translates
   portal error codes. */
typedef enum platform_sdk_leaderboard_status_t {
    PLATFORM_SDK_LEADERBOARD_OK = 0,
    PLATFORM_SDK_LEADERBOARD_UNSUPPORTED = 1,  /* terminal for the session: no such board here */
    PLATFORM_SDK_LEADERBOARD_NEEDS_LOGIN = 2,  /* the player can fix it */
    PLATFORM_SDK_LEADERBOARD_RATE_LIMITED = 3, /* the portal's own quota; try later */
    PLATFORM_SDK_LEADERBOARD_FAILED = 4,       /* transport or parse */
} platform_sdk_leaderboard_status_t;

/* Answered live, never cached: a Playgama host decides its board type at run
   time, and a Yandex write needs a login the player may complete mid-session. */
typedef struct platform_sdk_leaderboard_caps_t {
    bool can_read;     /* entries can be listed inside the game */
    bool can_write;    /* scores can be submitted */
    bool needs_login;  /* writing, and the player's own row, require portal auth */
    bool native_popup; /* the portal owns the UI; platform_sdk_leaderboard_open() shows it */
} platform_sdk_leaderboard_caps_t;

/* A neighbourhood is a centered window including the player. */
#define PLATFORM_SDK_LEADERBOARD_TOP_MAX 20
#define PLATFORM_SDK_LEADERBOARD_AROUND_MAX 10

/* Strings are borrowed for the duration of the completion call. */
typedef struct platform_sdk_leaderboard_entry_t {
    uint32_t value;
    int rank; /* 1-based; 0 when the portal gives none */
    bool you;
    const char *name;       /* "" on an anonymous board */
    const char *avatar_url; /* "" when the portal gives none */
    const char *extra;      /* the game's own payload as the portal stored it; "" when none */
} platform_sdk_leaderboard_entry_t;

typedef struct platform_sdk_leaderboard_page_t {
    const platform_sdk_leaderboard_entry_t *top;
    int top_count;
    const platform_sdk_leaderboard_entry_t *around; /* NULL when the portal cannot rank neighbours */
    int around_count;
    bool has_player;
    int player_rank;
    uint32_t player_value;
} platform_sdk_leaderboard_page_t;

/* One listener for every board: the consumer keys completions by board id and
   scope, which is why the entry points carry no per-call callback. */
typedef struct platform_sdk_leaderboard_listener_t {
    void (*submit_done)(const char *board_id, int32_t scope,
                        platform_sdk_leaderboard_status_t status, void *userdata);
    void (*fetch_done)(const char *board_id, int32_t scope,
                       platform_sdk_leaderboard_status_t status,
                       const platform_sdk_leaderboard_page_t *page, void *userdata);
    void *userdata;
} platform_sdk_leaderboard_listener_t;

typedef struct platform_sdk_gameplay_start_result_t {
    bool started;
    platform_sdk_result_t reason;
} platform_sdk_gameplay_start_result_t;

typedef struct platform_sdk_gameplay_stop_result_t {
    bool stopped;
    platform_sdk_result_t reason;
} platform_sdk_gameplay_stop_result_t;

typedef void (*platform_sdk_lifecycle_callback_t)(void *userdata);
typedef void (*platform_sdk_ad_callback_t)(platform_sdk_ad_result_t result, void *userdata);
typedef void (*platform_sdk_rewarded_callback_t)(platform_sdk_rewarded_result_t result, void *userdata);

typedef unsigned int platform_sdk_listener_id_t;
typedef uint32_t platform_sdk_ad_request_id_t;

typedef struct platform_sdk_backend_t {
    bool (*init)(void *userdata);
    /* Writes the portal's own language tag and returns whether it had one. The
       portal is asked, never the browser: a console that watches for the SDK
       read refuses a game that answers from navigator.language. */
    bool (*locale)(char *out, size_t out_size, void *userdata);
    void (*game_loading_progress)(float progress01, void *userdata);
    void (*game_loading_finished)(void *userdata);
    void (*game_ready)(void *userdata);
    void (*gameplay_start)(void *userdata);
    void (*gameplay_stop)(void *userdata);
    void (*measure)(const char *category, const char *what, const char *action, void *userdata);
    void (*show_banner)(void *userdata);
    void (*hide_banner)(void *userdata);
    platform_sdk_result_t (*show_interstitial)(const char *placement, void *userdata);
    platform_sdk_result_t (*show_rewarded)(const char *placement, void *userdata);
    /* Opens the portal's login dialog and later settles through
       platform_sdk_backend_complete_login(). NULL means the portal has none. */
    platform_sdk_result_t (*login)(void *userdata);
    /* Leaderboard hooks; NULL means the portal has no board API at all. A hook
       that answers OK has started a request that settles through the matching
       platform_sdk_backend_complete_leaderboard_*(); any other answer means
       nothing started and nothing will follow. */
    platform_sdk_leaderboard_caps_t (*leaderboard_caps)(const char *board_id, void *userdata);
    platform_sdk_result_t (*leaderboard_submit)(const char *board_id, int32_t scope, uint32_t value,
                                                const char *extra, void *userdata);
    platform_sdk_result_t (*leaderboard_fetch)(const char *board_id, int32_t scope, void *userdata);
    platform_sdk_result_t (*leaderboard_open)(const char *board_id, void *userdata);
    void (*destroy)(void *userdata);
} platform_sdk_backend_t;

platform_target_t platform_sdk_target(void);
platform_sdk_t platform_sdk_current(void);
const char *platform_sdk_target_name(void);
const char *platform_sdk_current_name(void);
platform_sdk_capabilities_t platform_sdk_capabilities(void);
bool platform_sdk_external_links_allowed(void);
bool platform_sdk_ads_supported(void);
bool platform_sdk_rewarded_supported(void);
/* What the build promises, minus what this portal has already refused. A portal
   can withdraw rewarded for a whole session -- a launch window with monetization
   off is the usual reason -- and only a request that came back "unsupported"
   reveals it. Anything that decides whether to OFFER a video asks this; the
   capability above answers what the target can do at all. */
bool platform_sdk_rewarded_available(void);
bool platform_sdk_storage_supported(void);
/* What the build promises, minus a portal that answered a login request with
   "unsupported": that answer latches for the session, so a login button asks
   this before it is drawn. */
bool platform_sdk_auth_supported(void);

void platform_sdk_set_backend(const platform_sdk_backend_t *backend, void *userdata);
platform_sdk_boot_status_t platform_sdk_status(void);
platform_sdk_result_t platform_sdk_init(void);
platform_sdk_result_t platform_sdk_game_loading_progress(float progress01);
platform_sdk_result_t platform_sdk_game_loading_finished(void);
platform_sdk_result_t platform_sdk_game_ready(void);

void platform_sdk_mark_input(void);
bool platform_sdk_has_input(void);
bool platform_sdk_has_gameplay_started(void);
bool platform_sdk_gameplay_active(void);
platform_sdk_gameplay_start_result_t platform_sdk_gameplay_start(void);
platform_sdk_gameplay_stop_result_t platform_sdk_gameplay_stop(void);

platform_sdk_listener_id_t platform_sdk_on_pause(platform_sdk_lifecycle_callback_t callback, void *userdata);
platform_sdk_listener_id_t platform_sdk_on_resume(platform_sdk_lifecycle_callback_t callback, void *userdata);
void platform_sdk_remove_listener(platform_sdk_listener_id_t listener_id);

platform_sdk_result_t platform_sdk_show_interstitial(
    const char *placement,
    platform_sdk_ad_callback_t callback,
    void *userdata);
platform_sdk_result_t platform_sdk_show_rewarded(
    const char *placement,
    platform_sdk_rewarded_callback_t callback,
    void *userdata);
/* The sticky banner is the one extra block a portal allows beside the
   fullscreen ad, and the portal owns its placement and its refresh. Asking for
   it is fire and forget: a portal whose console option is off simply keeps it
   hidden. */
void platform_sdk_show_banner(void);
void platform_sdk_hide_banner(void);

/* Player identity. There is no logout: a portal account outlives the game
   session, and Yandex offers no call to drop it, so the only transition is
   anonymous -> authorized, announced once through the auth.changed event.
   platform_sdk_login() is valid only from a player gesture; before the first
   input it is refused with WAITING_FOR_INPUT, like gameplay_start. One dialog
   at a time: a second call while one is open answers BUSY. */
bool platform_sdk_authorized(void);
bool platform_sdk_login_pending(void);
platform_sdk_result_t platform_sdk_login(void);
const char *platform_sdk_player_name(void);       /* "" until the portal supplies one */
const char *platform_sdk_player_avatar_url(void); /* "" until the portal supplies one */

/* Leaderboards. Every call names the board by its portal id and passes the
   scope through untouched (the portals serve one all-time board; a consumer
   that declares other scopes keeps them off the portal). `submit` and `fetch`
   answer OK when a request started and will settle through the listener;
   NOT_READY before the SDK is up, UNSUPPORTED when this portal has no board
   API, DESTROYED after teardown. `open` is fire and forget. The adapters
   enforce the portal's own quotas, so a caller never schedules around them. */
/* Replacing or clearing the listener invalidates outstanding web requests. */
void platform_sdk_leaderboard_set_listener(const platform_sdk_leaderboard_listener_t *listener);
platform_sdk_leaderboard_caps_t platform_sdk_leaderboard_caps(const char *board_id);
platform_sdk_result_t platform_sdk_leaderboard_submit(const char *board_id, int32_t scope,
                                                      uint32_t value, const char *extra);
platform_sdk_result_t platform_sdk_leaderboard_fetch(const char *board_id, int32_t scope);
platform_sdk_result_t platform_sdk_leaderboard_open(const char *board_id);

/* The portal, not the game, decides these: a tab the player left, a portal
   overlay, a phone call. The facade stops gameplay for the portal and restores
   it on resume, so a game only has to listen. */
void platform_sdk_backend_portal_pause(void);
void platform_sdk_backend_ad_visible(platform_sdk_ad_request_id_t request_id, bool visible);
void platform_sdk_backend_portal_resume(void);
bool platform_sdk_portal_paused(void);

/* Some portals own a mute switch of their own, outside the game's settings. It
   is not a pause: the game keeps running with no sound. */
void platform_sdk_backend_portal_audio(bool enabled);
bool platform_sdk_portal_audio_enabled(void);

/* True while an ad is pending or visible. Portal page pause is excluded. */
bool platform_sdk_ad_active(void);

/* True while the game must not advance: an ad is on screen or being fetched,
   or the portal has paused the page. One flag so the simulation gate and the
   gameplay-activity gate can never disagree. */
bool platform_sdk_break_active(void);

/* The portal language as the SDK answered it, or NULL when the target has no
   portal or the SDK never resolved one. */
const char *platform_sdk_locale(void);

/* Compatibility for a completion issued synchronously inside a backend start
   hook. An asynchronous backend must retain and return the request id. */
void platform_sdk_backend_complete_interstitial(platform_sdk_ad_result_t result);
void platform_sdk_backend_complete_rewarded(platform_sdk_rewarded_result_t result);
/* Web and native backends use the id captured when a request starts. A late
   completion for an earlier ad is ignored after another ad has started. */
platform_sdk_ad_request_id_t platform_sdk_active_interstitial_request_id(void);
platform_sdk_ad_request_id_t platform_sdk_active_rewarded_request_id(void);
void platform_sdk_backend_complete_interstitial_request(
    platform_sdk_ad_request_id_t request_id, platform_sdk_ad_result_t result);
void platform_sdk_backend_complete_rewarded_request(
    platform_sdk_ad_request_id_t request_id, platform_sdk_rewarded_result_t result);
void platform_sdk_backend_complete_init(bool ready);
/* Settles the dialog opened by the backend's login hook; an accepted result
   carries the identity the portal now reports. */
void platform_sdk_backend_complete_login(platform_sdk_auth_result_t result);
/* Identity known without a dialog: a player who arrives already signed in to
   the portal. Safe to call any time; only a real change is announced. */
void platform_sdk_backend_set_player(bool authorized, const char *name, const char *avatar_url);
/* Capture when starting an async request; discard its completion if this
   changes before dispatch. Listener replacement and teardown invalidate it. */
uint32_t platform_sdk_backend_leaderboard_generation(void);

/* Settle a leaderboard request the backend started. The page and its strings
   are borrowed for the call; a NULL page with OK counts as an empty board. */
void platform_sdk_backend_complete_leaderboard_submit(const char *board_id, int32_t scope,
                                                      platform_sdk_leaderboard_status_t status);
void platform_sdk_backend_complete_leaderboard_fetch(const char *board_id, int32_t scope,
                                                     platform_sdk_leaderboard_status_t status,
                                                     const platform_sdk_leaderboard_page_t *page);

void platform_sdk_destroy(void);

#if defined(PLATFORM_SDK_TESTING)
void platform_sdk_reset_for_tests(void);
#endif

#endif /* FEATURES_PLATFORM_SDK_PLATFORM_SDK_H */
