#include "game_log.h"

#include <stddef.h> /* offsetof, max_align_t */
#include <stdint.h>
#include <string.h> /* memcpy, memset, strlen */

#include "core/nt_assert.h"
#include "game_events.h" /* game_event_emit, game_event_register_type_name */
#include "log/nt_log.h"  /* nt_log_warn on staging overflow (release-visible) */

_Static_assert(_Alignof(GameLog) <= _Alignof(max_align_t),
               "GameLog over-aligned for game_event_emit");

nt_hash64_t game_log_type(void) {
    static nt_hash64_t h;
    if (!h.value) {
        h = nt_hash64_str("log");
    }
    return h;
}

const void *game_log_emit(const char *msg) {
    union {
        GameLog ev;
        uint8_t bytes[GAME_EVENT_EMIT_MAX];
    } u;
    memset(&u, 0, sizeof(u.ev)); /* deterministic struct bytes; the string is written below */

    uint32_t off = (uint32_t)sizeof(u.ev);
    const char *msg_s = msg ? msg : "";
    size_t msg_n = strlen(msg_s) + 1u; /* incl. NUL */
    if ((size_t)off + msg_n > sizeof(u.bytes)) {
        NT_ASSERT(0 && "game_log_emit payload exceeds GAME_EVENT_EMIT_MAX");
        nt_log_warn("game_log_emit: payload exceeds GAME_EVENT_EMIT_MAX (%u B) -> dropped",
                    (unsigned)GAME_EVENT_EMIT_MAX);
        return NULL; /* release: warned drop */
    }
    u.ev.msg = off;
    memcpy(u.bytes + off, msg_s, msg_n);
    off += (uint32_t)msg_n;
    return game_event_emit(game_log_type(), &u, off, _Alignof(GameLog));
}

static const game_event_field_t game_log_fields[] = {
    {"msg", GAME_EVENT_FT_STRING, (uint32_t)offsetof(GameLog, msg), 0u},
};
const game_event_desc_t game_log_desc = {
    "log",
    (uint32_t)sizeof(GameLog),
    game_log_fields,
    (int)(sizeof(game_log_fields) / sizeof(game_log_fields[0])),
};

const game_event_desc_t *const game_log_descs[] = {
    &game_log_desc,
};
const int game_log_desc_count = 1;

void game_log_register(void) {
    game_event_register_type_name(game_log_type(), "log");
}
