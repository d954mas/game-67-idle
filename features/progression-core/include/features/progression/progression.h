#ifndef FEATURES_PROGRESSION_H
#define FEATURES_PROGRESSION_H
// feature-layer: L2

#include "features/items/items.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum progression_mode_t {
    PROGRESSION_MODE_MANUAL = 0,
    PROGRESSION_MODE_AUTO,
    PROGRESSION_MODE_THRESHOLD,
} progression_mode_t;

typedef struct progression_emit_t {
    const char *def_id;
    const char *to_track;
    int64_t amount;
} progression_emit_t;

typedef struct progression_track_def_t {
    const char *id;
    progression_mode_t mode;
    const char *currency_def;
    int max_level;
    const int64_t *cost;
    int cost_count;
    const progression_emit_t *on_level_up;
    int on_level_up_count;
} progression_track_def_t;

const progression_track_def_t *progression_track_def(const char *track);

/* The game owns the resource container and binds it after state load/reset. */
void progression_bind_resource_container(items_container_ref_t container);

int progression_level(const char *track);
int progression_max_level(const char *track);
int64_t progression_xp_current(const char *track);
int64_t progression_xp_needed(const char *track);
bool progression_can_level_up(const char *track);

bool progression_level_up(const char *track, const char *reason);
void progression_add_xp(const char *track, int64_t n, const char *reason);
void progression_set_level(const char *track, int level, const char *reason);
void progression_reset(const char *track, const char *reason);

void progression_update(void);

#endif /* FEATURES_PROGRESSION_H */
