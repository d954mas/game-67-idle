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

typedef struct progression_amount_t {
    const char *def_id;
    int64_t amount;
} progression_amount_t;

/* One step up. Index L is the step that leaves level L, so a track with
   max_level N has N of them: level 0 is the un-upgraded state nothing reaches.
   Exactly one price is populated -- items for manual/auto, xp for threshold. */
typedef struct progression_step_t {
    const progression_amount_t *cost;
    int cost_count;
    int64_t xp_cost;
    const progression_amount_t *grants;
    int grant_count;
} progression_step_t;

/* The column dictionary is generated per game: progression_tracks.gen.h defines
   PROGRESSION_VALUE_<COLUMN> indices and PROGRESSION_VALUE_COUNT. */
typedef uint32_t progression_value_t;

typedef struct progression_track_def_t {
    const char *id;
    progression_mode_t mode;
    int max_level;
    const progression_step_t *steps;   /* max_level entries */
    const int64_t *exact;              /* [max_level + 1][value_count], or NULL */
    const double *fractional;          /* [max_level + 1][value_count], or NULL */
    uint32_t value_count;
} progression_track_def_t;

/* Resolved once; a string lookup is a linear scan over the whole catalog. */
typedef struct progression_track_ref_t {
    int index;
} progression_track_ref_t;

#define PROGRESSION_TRACK_REF_NONE ((progression_track_ref_t){-1})

progression_track_ref_t progression_track(const char *id);
bool progression_track_valid(progression_track_ref_t track);
const progression_track_def_t *progression_track_def(progression_track_ref_t track);

/* The game owns the containers a track spends from and binds them after state
   load/reset. A track pays atomically across the whole scope. */
void progression_bind_payment_scope(items_payment_scope_t scope);

/* The bound scope, so a caller can ask items whether a step is affordable
   without reaching for the game's own containers. */
items_payment_scope_t progression_payment_scope(void);

int progression_level(progression_track_ref_t track);
int progression_max_level(progression_track_ref_t track);
int64_t progression_xp_current(progression_track_ref_t track);

/* The step that leaves the CURRENT level: what the next level-up will charge. */
uint32_t progression_cost_count(progression_track_ref_t track);
progression_amount_t progression_cost_at(progression_track_ref_t track, uint32_t index);
int64_t progression_xp_cost(progression_track_ref_t track);

/* A column read. The type is the column's, not the call's, so asking for the
   wrong one is a debug assert and a zero -- both take one progression_value_t. */
int64_t progression_valuei(progression_track_ref_t track, progression_value_t value);
int64_t progression_valuei_at(progression_track_ref_t track, progression_value_t value, int level);
double progression_valuef(progression_track_ref_t track, progression_value_t value);
double progression_valuef_at(progression_track_ref_t track, progression_value_t value, int level);

bool progression_level_up(progression_track_ref_t track, const char *reason);
void progression_add_xp(progression_track_ref_t track, int64_t n, const char *reason);
void progression_set_level(progression_track_ref_t track, int level, const char *reason);
void progression_reset(progression_track_ref_t track, const char *reason);

void progression_update(void);

#endif /* FEATURES_PROGRESSION_H */
