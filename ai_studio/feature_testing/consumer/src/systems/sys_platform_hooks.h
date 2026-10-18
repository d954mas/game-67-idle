#ifndef GAME_SYS_PLATFORM_HOOKS_H
#define GAME_SYS_PLATFORM_HOOKS_H

#include <stdbool.h>

/* What a portal asks of every game, kept in one place above the SDK facade:
   the clock stops and the mixer goes quiet when attention is gone or an ad is
   on screen, an ad is asked for at the game's own seams and paid out only when
   the portal says so. Placements are the strings the portal's console and the
   game's ad config spell; the game decides which ones exist. */

typedef void (*platform_reward_fn)(const char *placement, void *userdata);

void platform_hooks_init(void);
void platform_hooks_shutdown(void);

/* Attention, split in two because the browser reports it twice and either half
   can be the one that moves: the tab went to the background, or the window
   holding the frame lost focus. Both default to present, so a host that never
   sends the event (a native run, a headless page) plays. */
void platform_hooks_set_page_visible(bool visible);
void platform_hooks_set_window_focused(bool focused);
bool platform_hooks_attention(void);
/* The portal's own pause (Yandex `game_api_pause`, an overlay) and its mute
   switch are read from the SDK facade, which owns both; the hooks only fold
   them into the two answers below. */
bool platform_hooks_audible(void);

/* Freezes the app clock and mutes the mixer while attention is gone or an ad is
   on screen, and thaws on the way back. Wall time is not replayed: the loop's
   dt is clamped, so a background minute costs one clamped frame, not a minute
   of simulation. Call once a frame, before the lifecycle reports gameplay. */
void platform_hooks_update(bool runtime_ready);

/* False while the sim is frozen. Two readers: the lifecycle, which turns it into
   gameplayStop, and any drag, because nothing must move under a video. */
bool platform_hooks_gameplay_allowed(void);
bool platform_hooks_ad_active(void);

/* The ad points, as API only: no button, no timer of the game's own. The
   portal's cooldown is the schedule, so a break is asked for at every seam and
   the answer is whichever the portal gives. Both return false when the request
   never reached the portal. A placement is at most 63 bytes. */
bool platform_hooks_commercial_break(const char *placement);
bool platform_hooks_rewarded(const char *placement, platform_reward_fn on_reward, void *userdata);
/* The portal can show a video at all: the UI hides a rewarded button on false
   rather than offering a tap that leads nowhere. False for the rest of the
   session once the portal answered a request with unsupported. */
bool platform_hooks_rewarded_supported(void);

/* Counters a test or a bot reads instead of watching the portal. */
unsigned int platform_hooks_break_count(void);
unsigned int platform_hooks_reward_grant_count(void);

void platform_hooks_register_devapi(void);

#endif /* GAME_SYS_PLATFORM_HOOKS_H */
