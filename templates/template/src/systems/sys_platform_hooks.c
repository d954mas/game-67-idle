#include "systems/sys_platform_hooks.h"

#include "app/nt_app.h"
#include "features/platform_sdk/platform_sdk.h"
#include "game_audio.h"
#include "game_log.h"
#include "time/nt_time.h"

#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#if !defined(__EMSCRIPTEN__) && NT_DEVAPI_ENABLED
/* A native dev build has no portal behind the facade, so no video can ever
   complete and the reward path would be unreachable for a bot. A web build
   compiles this to false and only a real success pays out. */
#define PLATFORM_HOOKS_MOCK_REWARD true
#else
#define PLATFORM_HOOKS_MOCK_REWARD false
#endif

/* An ad request that never answers would leave the session muted and frozen
   forever, so the game gives up on it. On the portal the wait must outlast any
   real unit, and only a wedged request trips it; a native dev build has no
   portal at all, so a pending ad there is always a stuck one. */
#ifdef __EMSCRIPTEN__
#define PLATFORM_HOOKS_AD_TIMEOUT_SECONDS 45.0
#else
#define PLATFORM_HOOKS_AD_TIMEOUT_SECONDS 2.0
#endif

#define PLATFORM_HOOKS_PLACEMENT_MAX 64

static bool s_initialized;
static bool s_page_visible = true;
static bool s_window_focused = true;
static bool s_audio_muted = false;
static bool s_ad_active;
static double s_ad_started_seconds;
static bool s_sim_frozen;
static unsigned int s_break_count;
static unsigned int s_reward_grant_count;

static platform_reward_fn s_reward_callback;
static void *s_reward_userdata;
static char s_reward_placement[PLATFORM_HOOKS_PLACEMENT_MAX];

static platform_sdk_listener_id_t s_pause_listener;
static platform_sdk_listener_id_t s_resume_listener;

/* The build says a portal can show videos; the portal itself can still refuse
   at runtime (a launch window with monetization off). Its first refusal is
   remembered, so an offer that can never pay stops being drawn. */
static bool s_reward_refused;

bool platform_hooks_rewarded_supported(void) {
    return s_initialized && !s_reward_refused && platform_sdk_rewarded_supported();
}

/* The facade raises the same pause listeners for an ad and for the portal's own
   pause. Only an ad is on a timer: a tab the player left for an hour is not a
   wedged request, and its resume must not be answered with a fake completion. */
static bool ad_pending(void) {
    return platform_sdk_break_active() && !platform_sdk_portal_paused();
}

/* Raised around every ad the facade actually starts, including the ones it
   fails to start, so the mixer comes back on both paths. */
static void on_sdk_pause(void *userdata) {
    (void)userdata;
    if (!ad_pending()) {
        return;
    }
    s_ad_active = true;
    s_ad_started_seconds = nt_time_now();
}

static void on_sdk_resume(void *userdata) {
    (void)userdata;
    s_ad_active = ad_pending();
}

#ifdef __EMSCRIPTEN__
static EM_BOOL on_web_visibility(
    int event_type, const EmscriptenVisibilityChangeEvent *event, void *userdata) {
    (void)event_type;
    (void)userdata;
    platform_hooks_set_page_visible(!event->hidden);
    return EM_FALSE; /* the page still owns the event */
}

static EM_BOOL on_web_focus(int event_type, const EmscriptenFocusEvent *event, void *userdata) {
    (void)event;
    (void)userdata;
    platform_hooks_set_window_focused(event_type == EMSCRIPTEN_EVENT_FOCUS);
    return EM_FALSE;
}

static void install_web_attention_events(void) {
    (void)emscripten_set_visibilitychange_callback(NULL, EM_FALSE, on_web_visibility);
    (void)emscripten_set_blur_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_FALSE, on_web_focus);
    (void)emscripten_set_focus_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_FALSE, on_web_focus);
}
#endif

void platform_hooks_init(void) {
    if (s_initialized) {
        return;
    }
    s_initialized = true;
    s_page_visible = true;
    s_window_focused = true;
    s_ad_active = false;
    s_sim_frozen = false;
    s_audio_muted = false;
    s_pause_listener = platform_sdk_on_pause(on_sdk_pause, NULL);
    s_resume_listener = platform_sdk_on_resume(on_sdk_resume, NULL);
#ifdef __EMSCRIPTEN__
    install_web_attention_events();
#endif
}

void platform_hooks_set_page_visible(bool visible) {
    s_page_visible = visible;
}

void platform_hooks_set_window_focused(bool focused) {
    s_window_focused = focused;
}

/* Sound follows FOCUS, the clock follows VISIBILITY. A portal requires silence
   from a game that lost focus (Yandex requirement 1.3), and a player who
   clicked another window does not want the game frozen mid-move. A portal's
   own mute switch is a third input: it silences without freezing. */
bool platform_hooks_audible(void) {
    return s_page_visible && s_window_focused && !s_ad_active &&
           !platform_sdk_portal_paused() && platform_sdk_portal_audio_enabled();
}

bool platform_hooks_attention(void) {
    /* VISIBILITY, not focus. A hidden tab must freeze -- nobody is watching and
       the portal expects it. A blurred canvas must NOT: the game is still on
       screen, and a player who clicked a bookmark bar or another window comes
       back to a game that kept going. Focus is still tracked for input, it just
       no longer stops the clock. */
    return s_page_visible;
}

bool platform_hooks_ad_active(void) {
    return s_ad_active;
}

bool platform_hooks_gameplay_allowed(void) {
    return platform_hooks_attention() && !s_ad_active && !platform_sdk_portal_paused();
}

/* Hands the wedged request an outcome instead of cancelling it: the facade has
   no cancel, and completing it is what releases the mixer and the clock. */
static void abandon_stuck_ad(void) {
    if (s_reward_callback != NULL) {
        platform_sdk_backend_complete_rewarded((platform_sdk_rewarded_result_t){
            .supported = true, .shown = false, .rewarded = false,
            .reason = PLATFORM_SDK_AD_REASON_FAILED});
    } else {
        platform_sdk_backend_complete_interstitial((platform_sdk_ad_result_t){
            .supported = true, .shown = false, .reason = PLATFORM_SDK_AD_REASON_FAILED});
    }
    if (s_ad_active) { /* no backend answered the completion: release it here */
        s_ad_active = false;
    }
    (void)game_log_emit("platform.ad timeout");
}

void platform_hooks_update(bool runtime_ready) {
    bool frozen;
    if (!s_initialized) {
        return;
    }
    if (s_ad_active &&
        nt_time_now() - s_ad_started_seconds >= PLATFORM_HOOKS_AD_TIMEOUT_SECONDS) {
        abandon_stuck_ad();
    }
    /* The mixer answers FOCUS and the clock answers VISIBILITY, so they are two
       edges, not one -- but each is still an edge: the backend is told when the
       answer changes and never once a frame. */
    {
        const bool muted = runtime_ready && !platform_hooks_audible();
        if (muted != s_audio_muted) {
            s_audio_muted = muted;
            game_audio_set_paused(muted);
        }
    }
    frozen = runtime_ready && !platform_hooks_gameplay_allowed();
    if (frozen == s_sim_frozen) {
        return;
    }
    s_sim_frozen = frozen;
    if (frozen) {
        nt_app_pause();
    } else {
        nt_app_resume();
    }
    (void)game_log_emit(frozen ? "platform.sim frozen" : "platform.sim running");
}

static bool placement_ok(const char *placement) {
    return placement != NULL && placement[0] != '\0' &&
           strlen(placement) < PLATFORM_HOOKS_PLACEMENT_MAX;
}

bool platform_hooks_commercial_break(const char *placement) {
    char message[96];
    platform_sdk_result_t result;
    if (!s_initialized || !placement_ok(placement)) {
        return false;
    }
    (void)snprintf(message, sizeof message, "platform.break %s", placement);
    (void)game_log_emit(message);
    result = platform_sdk_show_interstitial(placement, NULL, NULL);
    if (result == PLATFORM_SDK_RESULT_OK) {
        s_break_count += 1u;
        return true;
    }
    return false;
}

static void on_rewarded(platform_sdk_rewarded_result_t result, void *userdata) {
    const platform_reward_fn callback = s_reward_callback;
    void *reward_userdata = s_reward_userdata;
    char placement[PLATFORM_HOOKS_PLACEMENT_MAX];
    const bool granted = result.rewarded || PLATFORM_HOOKS_MOCK_REWARD;
    (void)userdata;

    memcpy(placement, s_reward_placement, sizeof placement);
    s_reward_callback = NULL;
    s_reward_userdata = NULL;
    if (!result.supported) {
        s_reward_refused = true;
    }

    if (!granted || callback == NULL) {
        (void)game_log_emit("platform.reward denied");
        return;
    }
    s_reward_grant_count += 1u;
    (void)game_log_emit("platform.reward granted");
    callback(placement, reward_userdata);
}

bool platform_hooks_rewarded(const char *placement, platform_reward_fn on_reward, void *userdata) {
    char message[96];
    platform_sdk_result_t result;
    if (!platform_hooks_rewarded_supported() || s_reward_callback != NULL || !placement_ok(placement)) {
        return false; /* one video, one reward: no chaining, no double payout */
    }
    (void)snprintf(message, sizeof message, "platform.reward %s", placement);
    (void)game_log_emit(message);

    (void)snprintf(s_reward_placement, sizeof s_reward_placement, "%s", placement);
    s_reward_callback = on_reward;
    s_reward_userdata = userdata;
    result = platform_sdk_show_rewarded(placement, on_rewarded, NULL);
    if (result != PLATFORM_SDK_RESULT_OK && s_reward_callback != NULL) {
        s_reward_callback = NULL;
        s_reward_userdata = NULL;
        return false;
    }
    return true;
}

unsigned int platform_hooks_break_count(void) {
    return s_break_count;
}

unsigned int platform_hooks_reward_grant_count(void) {
    return s_reward_grant_count;
}

void platform_hooks_shutdown(void) {
    if (!s_initialized) {
        return;
    }
    platform_sdk_remove_listener(s_pause_listener);
    platform_sdk_remove_listener(s_resume_listener);
    if (s_sim_frozen) {
        nt_app_resume();
        game_audio_set_paused(false);
    }
    s_initialized = false;
    s_page_visible = true;
    s_window_focused = true;
    s_ad_active = false;
    s_sim_frozen = false;
    s_audio_muted = false;
    s_break_count = 0u;
    s_reward_grant_count = 0u;
    s_reward_callback = NULL;
    s_reward_userdata = NULL;
    s_reward_refused = false;
}
