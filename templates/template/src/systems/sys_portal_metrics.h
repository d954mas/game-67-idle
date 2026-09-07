#ifndef GAME_SYS_PORTAL_METRICS_H
#define GAME_SYS_PORTAL_METRICS_H

#include "game_events.h"

/* The portal's funnel, fed from the frame event log. The drain is the same for
   every game; what a game's events mean to a dashboard is the game's own, so
   the mapping is handed in as a callback that names the (category, what,
   action) triples through sys_portal_metrics_measure(). Only portals with a
   funnel API listen (Poki); elsewhere measure() is a no-op. */

typedef void (*portal_metrics_map_fn)(const game_event_t *ev, void *userdata);

/* `map` may be NULL: nothing is read from the log, but measure() still works
   for the events a screen reports directly (an offer seen, a button tapped). */
void sys_portal_metrics_init(portal_metrics_map_fn map, void *userdata);

/* Runs the mapper over the events recorded this frame, once per frame, after
   every other reader of the log. */
void sys_portal_metrics_record(void);

void sys_portal_metrics_measure(const char *category, const char *what, const char *action);

#endif /* GAME_SYS_PORTAL_METRICS_H */
