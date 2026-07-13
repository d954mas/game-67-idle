#ifndef GAME_ANALYTICS_H
#define GAME_ANALYTICS_H

#if FEATURE_GAME_ANALYTICS

#include <stddef.h>
#include <stdint.h>

#include "game_event_desc.h" /* game_event_desc_t */

/* Local analytics writer over the event bus. RECORD-phase
   recorder: renders EVERY frame event via game_event_render (E3, reused) into an NDJSON
   stream. native -> build/analytics/session-<wall>-<pid>.ndjson (append); web -> in-memory
   ring; test -> injected sink. Buffered (never a blocking write per frame). Not a save slot. */

/* Register a fragment's generated descriptor table (<frag>_ev_descs / _count) into the
   writer's hash->desc lookup (typed rendering). Call once after nt_hash_init, same site as
   the DevAPI tail registration. Duplicate type hashes trigger a debug assert. */
void game_analytics_register_descs(const game_event_desc_t *const *descs, int count);

/* Open the stream: write the session header line. Call once after descriptors registered
   (before the frame loop). No-op if already open. */
void game_analytics_init(void);

/* RECORD-phase recorder. Walk game_event_log(), render each event by descriptor into an
   NDJSON line, append to the buffer; flushes to the sink on threshold. Call ONCE per frame
   from game_features_record (arena alive). No-op until init. */
void game_analytics_record(void);

/* Force-flush the buffer to the sink. Call on shutdown; native only needs it (web ring is
   already live in memory). */
void game_analytics_flush(void);

/* Final flush + close (native: fclose). Resets all statics (registry too) for a clean
   next init -- the ctest runs one init/shutdown cycle per case. */
void game_analytics_shutdown(void);

/* Cumulative events dropped by a REAL failure (rendered line > buffer, or write error) --
   health metric, mirror of game_events_dropped(); 0 on a healthy run. The session byte cap
   is a routine bound, not a failure: it sets the internal capped flag + one-time warn. */
uint64_t game_analytics_dropped(void);

/* Cumulative ROUTINE web-ring evictions (oldest whole lines rolled off the in-memory ring).
   Kept SEPARATE from dropped() so "dropped()==0 on a healthy run" stays true even as the
   bounded web ring rolls. Always 0 on native (append-file). Mirror of the E3 tail evicted. */
uint64_t game_analytics_evicted(void);

#if defined(__EMSCRIPTEN__)
/* Web export: the in-memory NDJSON ring as a malloc'd NUL string (caller frees), for a game
   "export analytics" affordance / future DevAPI command. NULL if empty. Currently INERT --
   no client invokes it yet (a future consumer must land on BOTH clients at once, tool
   parity: a site affordance AND a DevAPI command). */
char *game_analytics_export(void);
#endif

/* Sink function type: the platform default (native file / web ring) and the test capture
   share it. Declared unconditionally so the writer TU compiles with or without the test
   seam (the test-only injectors below reuse it). */
typedef void (*game_analytics_sink_fn)(const char *bytes, size_t len);

#ifdef GAME_ANALYTICS_TESTING
/* Test seams (declared only under GAME_ANALYTICS_TESTING): inject the sink (capture instead
   of file/ring) and the wall clock. Call BEFORE game_analytics_init. */
void game_analytics__set_sink_for_test(game_analytics_sink_fn sink);
void game_analytics__set_clock_for_test(int64_t (*wall)(void));
#endif

#endif /* FEATURE_GAME_ANALYTICS */
#endif /* GAME_ANALYTICS_H */
