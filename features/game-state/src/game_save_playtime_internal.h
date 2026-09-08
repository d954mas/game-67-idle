#ifndef GAME_SAVE_PLAYTIME_INTERNAL_H
#define GAME_SAVE_PLAYTIME_INTERNAL_H

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>

typedef struct game_save_playtime {
    int64_t milliseconds;
    int64_t last_mono_ms;
    bool active;
} game_save_playtime_t;

static inline void game_save_playtime_set(
    game_save_playtime_t *counter, int64_t milliseconds, int64_t mono_ms) {
    counter->milliseconds = milliseconds;
    counter->last_mono_ms = mono_ms;
    counter->active = false;
}

static inline bool game_save_playtime_update(
    game_save_playtime_t *counter, int64_t mono_ms, bool active) {
    bool changed = false;
    if (counter->active && mono_ms > counter->last_mono_ms) {
        const uint64_t elapsed = (uint64_t)mono_ms - (uint64_t)counter->last_mono_ms;
        const uint64_t capacity = (uint64_t)(INT64_MAX - counter->milliseconds);
        if (elapsed >= capacity) {
            if (capacity != 0U) {
                counter->milliseconds = INT64_MAX;
                changed = true;
            }
        } else {
            counter->milliseconds += (int64_t)elapsed;
            changed = true;
        }
        counter->last_mono_ms = mono_ms;
    } else if (!counter->active) {
        counter->last_mono_ms = mono_ms;
    }
    counter->active = active;
    return changed;
}

#endif
