/* Opens an engine window, asks Steam for its overlay once the overlay has hooked the window,
   prints what happened and closes itself. Exit 0: the overlay hooked the window and its
   OVERLAY event came; 2: either did not happen; 77: Steam did not start; 1: the engine did not
   start. */

#include "app/nt_app.h"
#include "core/nt_core.h"
#include "features/steam_platform/steam_platform.h"
#include "graphics/nt_gfx.h"
#include "input/nt_input.h"
#include "window/nt_window.h"

#include <stdio.h>

/* Seconds. The window never outlives HARD_STOP, whatever Steam does. */
#define READY_WAIT 6.0
#define EVENT_WAIT 4.0
#define SHOWN_HOLD 1.5
#define HARD_STOP 14.0

typedef enum { WAIT_READY, WAIT_EVENT } phase_t;

static phase_t s_phase = WAIT_READY;
static double s_asked_at;
static bool s_ready_seen;
static double s_ready_at;
static int s_events;
static double s_shown_at = -1.0;

static void frame(void) {
    nt_window_poll();
    nt_input_poll();
    steam_platform_run_frame();
    const double now = g_nt_app.time;

    steam_platform_event_t event;
    while (steam_platform_next_event(&event)) {
        if (event.type == STEAM_PLATFORM_EVENT_OVERLAY) {
            s_events++;
            printf("overlay event: active=%d at %.2f s\n", event.active ? 1 : 0, now);
            if (event.active && s_shown_at < 0.0) {
                s_shown_at = now;
            }
        }
    }

    if (s_phase == WAIT_READY) {
        const bool ready = steam_platform_overlay_ready();
        if (ready || now > READY_WAIT) {
            s_ready_seen = ready;
            s_ready_at = now;
            printf("overlay_ready=%d at %.2f s; asking for \"friends\"\n", ready ? 1 : 0, now);
            steam_platform_open_overlay("friends");
            s_asked_at = now;
            s_phase = WAIT_EVENT;
        }
    } else {
        const bool shown_long_enough = s_shown_at >= 0.0 && now - s_shown_at > SHOWN_HOLD;
        if (shown_long_enough || now - s_asked_at > EVENT_WAIT) {
            nt_app_quit();
        }
    }
    if (now > HARD_STOP) {
        nt_app_quit();
    }

    nt_gfx_begin_frame();
    nt_gfx_begin_pass(&(nt_pass_desc_t){.clear_color = {0.10F, 0.16F, 0.24F, 1.0F}, .clear_depth = 1.0F});
    nt_gfx_end_pass();
    nt_gfx_end_frame();
    nt_window_swap_buffers();
}

int main(void) {
    /* Unbuffered, so the log shows how far a run got even when it dies. */
    setvbuf(stdout, NULL, _IONBF, 0);
    /* Before the window: Steam's overlay hooks the graphics context the window creates. */
    char message[256];
    const steam_platform_config_t config = {.app_id = 480, .restart_through_steam = false};
    const steam_platform_status_t status = steam_platform_start(&config, message, sizeof message);
    printf("steam_platform_start: %d (%s)\n", (int)status, message);
    if (status != STEAM_PLATFORM_READY) {
        printf("RESULT: no Steam, nothing to show\n");
        return 77;
    }

    nt_engine_config_t engine = {0};
    engine.app_name = "steam_platform_overlay_example";
    if (nt_engine_init(&engine) != NT_OK) {
        steam_platform_stop();
        return 1;
    }
    g_nt_window.width = 640;
    g_nt_window.height = 360;
    nt_window_init();
    nt_input_init();
    nt_gfx_desc_t gfx = nt_gfx_desc_defaults();
    nt_gfx_init(&gfx);
    g_nt_app.target_dt = 1.0F / 60.0F;

    nt_app_run(frame);

    printf("RESULT: overlay_ready=%d (at %.2f s), overlay events=%d, shown=%d\n", s_ready_seen ? 1 : 0, s_ready_at,
           s_events, s_shown_at >= 0.0 ? 1 : 0);
    nt_gfx_shutdown();
    nt_input_shutdown();
    nt_window_shutdown();
    nt_engine_shutdown();
    steam_platform_stop();
    return s_ready_seen && s_shown_at >= 0.0 ? 0 : 2;
}
