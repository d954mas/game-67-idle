#ifndef GAME_LOG_H
#define GAME_LOG_H

#include <stdbool.h>
#include <stdint.h>

#include "game_event_desc.h" /* game_event_desc_t */
#include "hash/nt_hash.h"    /* nt_hash64_t */

/* Built-in generic "log" event type (event_system_design §3/§6). A fragment-less,
   hand-written leaf: an ad-hoc debug message for game glue that needs no schema. Type =
   nt_hash64_str("log") (NOT "game.log" -- expressing it in a fragment schema would need a
   generator edit, and the generator is frozen). One inline STRING field `msg`. The E3 tail
   and the E4 analytics writer render it generically once its descriptor is registered.

   Unconditional leaf: needs only game_events.h + game_event_desc.h + nt_hash.h (no cJSON,
   no devapi) -> compiles in the game target ALWAYS, like game_events.c; in release an emit
   is harmless (the event lives one frame and dies). */

typedef struct GameLog {
    uint32_t msg; /* byte offset within the payload -> inline NUL-terminated string */
} GameLog;

nt_hash64_t game_log_type(void); /* nt_hash64_str("log"), cached */

/* Pack `msg` (NULL -> "") as an inline string and emit a "log" event. Returns the arena
   copy (positional-independent) or NULL on staging overflow (> GAME_EVENT_EMIT_MAX). The
   emit-helper name is game_log_emit -- game_event_log(int*) is already the log walk. */
const void *game_log_emit(const char *msg);

static inline const char *game_log_msg(const GameLog *e) {
    return (const char *)e + e->msg;
}

extern const game_event_desc_t game_log_desc;           /* {"log", sizeof(GameLog), {STRING msg}} */
extern const game_event_desc_t *const game_log_descs[]; /* 1-elem table for *_register_descs */
extern const int game_log_desc_count;                   /* 1 */

/* Register the debug label "log" (effect only under NT_HASH_LABELS). Call once after
   nt_hash_init. Descriptor registration into the tail/analytics registries is a separate
   *_register_descs(game_log_descs, ...) call at the conductor (main.c). */
void game_log_register(void);

#endif /* GAME_LOG_H */
