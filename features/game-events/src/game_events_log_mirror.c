#include "game_events_log_mirror.h"

#if GAME_EVENTS_LOG_MIRROR

#include "core/nt_assert.h"
#include "game_event_render.h"
#include "game_events.h"
#include "hash/nt_hash.h"
#include "log/nt_log.h"

#ifndef GAME_EVENTS_LOG_MIRROR_ENTRY_MAX
#define GAME_EVENTS_LOG_MIRROR_ENTRY_MAX 512
#endif
#ifndef GAME_EVENTS_LOG_MIRROR_DESC_REG_CAP
#define GAME_EVENTS_LOG_MIRROR_DESC_REG_CAP 64
#endif

typedef struct {
    uint64_t hash;
    const game_event_desc_t *desc;
} mirror_reg_entry_t;

static mirror_reg_entry_t s_reg[GAME_EVENTS_LOG_MIRROR_DESC_REG_CAP];
static int s_reg_count;

void game_events_log_mirror_register_descs(const game_event_desc_t *const *descs, int count) {
    for (int i = 0; i < count; ++i) {
        const game_event_desc_t *d = descs[i];
        if (!d || !d->name) {
            continue;
        }
        const uint64_t h = nt_hash64_str(d->name).value;
        for (int j = 0; j < s_reg_count; ++j) {
            NT_ASSERT(s_reg[j].hash != h && "duplicate event type hash in log mirror registry");
        }
        if (s_reg_count >= GAME_EVENTS_LOG_MIRROR_DESC_REG_CAP) {
            nt_log_warn("game_events_log_mirror: descriptor registry full (%d) -> dropping %s",
                        GAME_EVENTS_LOG_MIRROR_DESC_REG_CAP, d->name);
            return;
        }
        s_reg[s_reg_count] = (mirror_reg_entry_t){.hash = h, .desc = d};
        s_reg_count++;
    }
}

static const game_event_desc_t *reg_find(nt_hash64_t type) {
    for (int i = 0; i < s_reg_count; ++i) {
        if (s_reg[i].hash == type.value) {
            return s_reg[i].desc;
        }
    }
    return NULL;
}

void game_events_log_mirror_record(void) {
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    for (int i = 0; i < n; ++i) {
        const game_event_t *e = &log[i];
        char line[GAME_EVENTS_LOG_MIRROR_ENTRY_MAX];
        const game_event_desc_t *d = reg_find(e->type);
        (void)game_event_render(e, d, line, (int)sizeof line);
        nt_log_info("[ev] %s", line);
    }
}

#endif /* GAME_EVENTS_LOG_MIRROR */
