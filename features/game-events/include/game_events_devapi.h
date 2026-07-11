#ifndef GAME_EVENTS_DEVAPI_H
#define GAME_EVENTS_DEVAPI_H

#if NT_DEVAPI_ENABLED

#include "game_event_desc.h" /* game_event_desc_t */

/* Register a fragment's generated descriptor table (<frag>_ev_descs / _count) into the
   tail's hash->desc lookup. Call once after nt_hash_init (type hashes computed from
   desc->name). Conductor wiring (main.c) -- the generator stays frozen. Duplicate
   type hash across descriptors triggers a debug assert. */
void game_events_devapi_register_descs(const game_event_desc_t *const *descs, int count);

/* RECORD-phase recorder (render-at-copy). Walks game_event_log(), renders each event by
   descriptor into the fixed ring. Call ONCE per frame from game_features_record (arena
   alive). No-op until game_events_register_devapi() has enabled it. */
void game_events_devapi_record(void);

/* Registers game.events.tail. Call once from devapi_start() after nt_devapi_init(). */
void game_events_register_devapi(void);

#endif /* NT_DEVAPI_ENABLED */
#endif /* GAME_EVENTS_DEVAPI_H */
