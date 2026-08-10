#ifndef GAME_EVENT_DESC_H
#define GAME_EVENT_DESC_H

#include <stdint.h>

/* Shared descriptor contract for generic event rendering.
   The generator emits one descriptor per event; DevAPI/log/analytics (E3/E4) walk it to
   render ANY event with no per-feature code. Positional-independent: every offset is
   relative to the payload base. */

/* Staging cap for emit helpers that pack inline strings/bytes. Oversized payload:
   debug NT_ASSERT / release nt_log_warn + drop (emit helper returns NULL). The staging
   union lives on the STACK per emit (default 4 KiB); raise per-game WITH CARE -- web/wasm
   stacks are small (~64 KiB) and react cascades nest emits. */
#ifndef GAME_EVENT_EMIT_MAX
#define GAME_EVENT_EMIT_MAX 4096u
#endif

typedef enum {
    GAME_EVENT_FT_BOOL = 0,
    GAME_EVENT_FT_INT,     /* int32_t */
    GAME_EVENT_FT_I64,     /* int64_t */
    GAME_EVENT_FT_FLOAT,   /* double (f64) */
    GAME_EVENT_FT_STRING,  /* uint32 byte-offset -> inline NUL-terminated string */
    GAME_EVENT_FT_HASH,    /* nt_hash64_t; render via nt_hash64_label */
    GAME_EVENT_FT_BYTES    /* uint32 byte-offset + uint32 length; recorders ignore, DevAPI size+hex */
} game_event_field_type_t;

typedef struct {
    const char             *name;
    game_event_field_type_t type;
    uint32_t                offset;     /* offsetof within the payload struct */
    uint32_t                len_offset; /* BYTES: offsetof of the paired uint32 length; else 0 */
} game_event_field_t;

/* A repeated section: N fixed-size records packed inline after the payload struct.
   `offset`/`count_offset` locate the uint32 pair inside the struct; member offsets in
   `fields` are relative to ONE RECORD, but a member's own stored value keeps the
   payload base as its origin (a STRING member holds a payload-relative offset exactly
   like a top-level one), so a walker shifts the member offset and leaves the value
   alone. Records carry scalars and strings only -- never another repeated section. */
typedef struct {
    const char               *name;
    uint32_t                  offset;       /* offsetof the uint32 payload-relative array offset */
    uint32_t                  count_offset; /* offsetof the paired uint32 record count */
    uint32_t                  size;         /* sizeof one record */
    const game_event_field_t *fields;
    int                       field_count;
} game_event_record_t;

typedef struct {
    const char                *name;         /* "<fragment>.<event>" */
    uint32_t                   payload_size; /* sizeof the payload struct */
    const game_event_field_t  *fields;
    int                        field_count;
    const game_event_record_t *records;
    int                        record_count;
} game_event_desc_t;

#endif /* GAME_EVENT_DESC_H */
