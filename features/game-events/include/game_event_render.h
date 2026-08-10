#ifndef GAME_EVENT_RENDER_H
#define GAME_EVENT_RENDER_H

#include "game_event_desc.h" /* game_event_desc_t */
#include "game_events.h"     /* game_event_t */

/* Max payload bytes hex-dumped (bytes/unknown fields) so a render fits the ring slot. */
#ifndef GAME_EVENT_RENDER_HEX_MAX
#define GAME_EVENT_RENDER_HEX_MAX 48
#endif

/* The line budget every consumer of game_event_render must give it. Sized against
   GAME_EVENT_EMIT_MAX rather than against today's longest event: a repeated section
   makes the rendered length content-driven, so a budget tuned to one schema would be
   a landmine for the next. Not fitting is an assert, so this number has to hold. */
#ifndef GAME_EVENT_RENDER_LINE_MAX
#define GAME_EVENT_RENDER_LINE_MAX 4096
#endif

/* Renders ONE event to a compact JSON object string in out[0..cap) (NUL-terminated).
   desc==NULL => unregistered/raw event: { seq, tick, type, size, unknown:true, hex }.
   Positional-independent: string/bytes read via payload-relative offsets from the
   descriptor. EVERY read is bounds-checked against e->size BEFORE dereferencing: a
   scalar/offset/len word is read only if offset+width <= e->size (BYTES also needs
   len_offset+4 <= e->size); a STRING's inline bytes are scanned with a BOUNDED memchr
   within [soff, e->size) and, if no NUL is found in range, the field is emitted as
   { "size":N, "truncated":true } (NEVER AddStringToObject on an unterminated span --
   no over-read). Any out-of-range field is skipped / marked, never dereferenced.
   Numbers vs strings: i64 fields ride as a JSON STRING (gsj_i64_to_string) -- NEVER a
   double (bit-for-bit parity with game.state.get; envelope seq/tick stay numbers).
   Type name: desc->name if desc, else nt_hash64_label(e->type) if non-NULL, else
   "0x%016<PRIx64>" hex. hash-FIELD values: nt_hash64_label else hex. bytes fields:
   { "size":N, "hex":"..." } (DevAPI shows size+hex; hex truncated to fit).
   `cap` must be GAME_EVENT_RENDER_LINE_MAX. A render that does not fit asserts:
   there is no degraded line, because a marker nobody can read the payment out of
   is worse than a stop, and an event too big to render is one the emit path
   should have refused. Returns the written length (< cap). */
int game_event_render(const game_event_t *e, const game_event_desc_t *desc, char *out, int cap);

#endif /* GAME_EVENT_RENDER_H */
