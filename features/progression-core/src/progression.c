#include "features/progression/progression.h"

#include "progression_tracks.gen.h" /* codegen: k_tracks/k_tracks_count/k_progression_value_exact */

#include "progression_state.h"             /* generated: ProgressionState + progression_state instance */
#include "progression_state_events.gen.h"  /* generated: progression_emit_levelup */

#include "core/nt_assert.h"
#include "game_save.h"  /* game_save_mark_dirty */
#include "log/nt_log.h" /* nt_log_warn on the per-frame level-up cap */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* The scope a track pays from. Its first container also receives grants: a level
   hands items back into the same purse it charged. */
static items_payment_scope_t s_scope;

void progression_bind_payment_scope(items_payment_scope_t scope) {
    s_scope = scope;
}

items_payment_scope_t progression_payment_scope(void) {
    return s_scope;
}

/* The balance a price is measured against: the whole scope, not one container. */
static int64_t scope_count(const char *def_id) {
    int64_t total = 0;
    for (uint32_t i = 0; i < s_scope.count; ++i) {
        total += items_stack_count(s_scope.containers[i], def_id);
    }
    return total;
}

/* Format "verb:subject"; debug-only. A spend forwards reason into items, where the
   full verb check runs, so the currency vocabulary is not duplicated here. */
static inline void progression_reason_check(const char *reason) {
#ifndef NDEBUG
    assert(reason != NULL);
    const char *colon = strchr(reason, ':');
    assert(colon != NULL && colon[1] != '\0');
#else
    (void)reason;
#endif
}

static const char *mode_name(progression_mode_t mode) {
    switch (mode) {
    case PROGRESSION_MODE_MANUAL:
        return "manual";
    case PROGRESSION_MODE_AUTO:
        return "auto";
    case PROGRESSION_MODE_THRESHOLD:
        return "threshold";
    }
    return "unknown";
}

bool progression_track_valid(progression_track_ref_t track) {
    return track.index >= 0 && track.index < k_tracks_count;
}

progression_track_ref_t progression_track(const char *id) {
    if (id != NULL) {
        for (int i = 0; i < k_tracks_count; ++i) {
            if (strcmp(k_tracks[i].id, id) == 0) {
                return (progression_track_ref_t){i};
            }
        }
    }
    return PROGRESSION_TRACK_REF_NONE;
}

const progression_track_def_t *progression_track_def(progression_track_ref_t track) {
    return progression_track_valid(track) ? &k_tracks[track.index] : NULL;
}

/* Reads never allocate: a track with no save record is level 0 / xp 0, so a fresh
   save keeps an empty tracks map and an idle tick creates nothing. */
static ProgressionTrackState *find_track(const char *id) {
    for (int i = 0; i < PROGRESSION_STATE_MAX_TRACKS; ++i) {
        if (progression_state.tracks[i].used && strcmp(progression_state.tracks[i].key, id) == 0) {
            return &progression_state.tracks[i];
        }
    }
    return NULL;
}

/* The key is built into a validated buffer first: a truncated key can never be
   found again, so every later mutation would burn a fresh slot instead. */
static ProgressionTrackState *find_or_alloc_track(const char *id) {
    ProgressionTrackState *existing = find_track(id);
    if (existing != NULL) {
        return existing;
    }
    char key[PROGRESSION_STATE_STRING_MAX];
    int n = snprintf(key, sizeof key, "%s", id);
    NT_ASSERT(n >= 0 && (size_t)n < sizeof key);
    if (n < 0 || (size_t)n >= sizeof key) {
        return NULL;
    }
    for (int i = 0; i < PROGRESSION_STATE_MAX_TRACKS; ++i) {
        if (!progression_state.tracks[i].used) {
            ProgressionTrackState *slot = &progression_state.tracks[i];
            slot->used = true;
            (void)snprintf(slot->key, sizeof slot->key, "%s", key);
            slot->level = PROGRESSION_STATE_TRACK_STATE_LEVEL_DEFAULT;
            slot->xp = PROGRESSION_STATE_TRACK_STATE_XP_DEFAULT;
            return slot;
        }
    }
    return NULL; /* PROGRESSION_STATE_MAX_TRACKS budget exhausted */
}

/* Anti-hang: an auto track whose grants nearly cover its own price still has to
   give the frame back. The catalog gate rejects grants that fully cover it. */
#define PROGRESSION_MAX_LEVELUPS_PER_TRACK 64

static const progression_step_t *step_at(const progression_track_def_t *def, int level) {
    assert(level >= 0 && level < def->max_level);
    return &def->steps[level];
}

/* ---- Reads ---- */

int progression_level(progression_track_ref_t track) {
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL) {
        return 0;
    }
    const ProgressionTrackState *st = find_track(def->id);
    return st ? st->level : 0;
}

int progression_max_level(progression_track_ref_t track) {
    const progression_track_def_t *def = progression_track_def(track);
    return def ? def->max_level : 0;
}

/* Only a threshold track accumulates xp; an item-paid track spends the purse. */
int64_t progression_xp_current(progression_track_ref_t track) {
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL || def->mode != PROGRESSION_MODE_THRESHOLD) {
        return 0;
    }
    const ProgressionTrackState *st = find_track(def->id);
    return st ? st->xp : 0;
}

static const progression_step_t *current_step(progression_track_ref_t track) {
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL) {
        return NULL;
    }
    int level = progression_level(track);
    return level < def->max_level ? step_at(def, level) : NULL;
}

uint32_t progression_cost_count(progression_track_ref_t track) {
    const progression_step_t *step = current_step(track);
    return step ? (uint32_t)step->cost_count : 0u;
}

progression_amount_t progression_cost_at(progression_track_ref_t track, uint32_t index) {
    const progression_step_t *step = current_step(track);
    if (step == NULL || index >= (uint32_t)step->cost_count) {
        return (progression_amount_t){NULL, 0};
    }
    return step->cost[index];
}

int64_t progression_xp_cost(progression_track_ref_t track) {
    const progression_step_t *step = current_step(track);
    return step ? step->xp_cost : 0;
}

static int clamped_level(const progression_track_def_t *def, int level) {
    if (level < 0) {
        return 0;
    }
    return level > def->max_level ? def->max_level : level;
}

int64_t progression_valuei_at(progression_track_ref_t track, progression_value_t value, int level) {
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL || def->exact == NULL || value >= def->value_count) {
        return 0;
    }
    NT_ASSERT(k_progression_value_exact[value] && "exact read of a fractional column");
    if (!k_progression_value_exact[value]) {
        return 0;
    }
    return def->exact[(uint32_t)clamped_level(def, level) * def->value_count + value];
}

int64_t progression_valuei(progression_track_ref_t track, progression_value_t value) {
    return progression_valuei_at(track, value, progression_level(track));
}

double progression_valuef_at(progression_track_ref_t track, progression_value_t value, int level) {
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL || def->fractional == NULL || value >= def->value_count) {
        return 0.0;
    }
    NT_ASSERT(!k_progression_value_exact[value] && "fractional read of an exact column");
    if (k_progression_value_exact[value]) {
        return 0.0;
    }
    return def->fractional[(uint32_t)clamped_level(def, level) * def->value_count + value];
}

double progression_valuef(progression_track_ref_t track, progression_value_t value) {
    return progression_valuef_at(track, value, progression_level(track));
}

/* ---- Mutations (reason required) ---- */

static void emit_levelup(
    const progression_track_def_t *def, const char *reason, int old_level, int new_level,
    const progression_step_t *step, int64_t xp_before) {
    ProgressionEvLevelupCostIn charged[ITEMS_PAYMENT_MAX_REQUIREMENTS];
    int count = step->cost_count < (int)ITEMS_PAYMENT_MAX_REQUIREMENTS
                    ? step->cost_count
                    : (int)ITEMS_PAYMENT_MAX_REQUIREMENTS;
    for (int i = 0; i < count; ++i) {
        /* Read after the spend, so add back what this step just took. */
        charged[i] = (ProgressionEvLevelupCostIn){
            step->cost[i].def_id,
            step->cost[i].amount,
            scope_count(step->cost[i].def_id) + step->cost[i].amount,
        };
    }
    progression_emit_levelup(
        def->id, mode_name(def->mode), reason, old_level, new_level,
        step->xp_cost, xp_before, charged, (uint32_t)count);
}

/* Grants land in the first container of the payment scope: a level hands items
   back into the purse it charged. */
static void apply_grants(const progression_step_t *step, const char *reason) {
    if (s_scope.count == 0) {
        return;
    }
    for (int i = 0; i < step->grant_count; ++i) {
        (void)items_try_stack_add(
            s_scope.containers[0], step->grants[i].def_id, step->grants[i].amount,
            ITEMS_SLOT_AUTO, reason, NULL, NULL);
    }
}

/* Checking, then allocating, then paying is the whole point of the order: paying
   first would burn the purse whenever the tracks map is full. */
static bool buy_step(
    const progression_track_def_t *def, const progression_step_t *step, const char *reason,
    ProgressionTrackState **out_state) {
    const char *def_ids[ITEMS_PAYMENT_MAX_REQUIREMENTS];
    int64_t counts[ITEMS_PAYMENT_MAX_REQUIREMENTS];
    if (step->cost_count > (int)ITEMS_PAYMENT_MAX_REQUIREMENTS) {
        return false; /* the catalog gate bounds this; defensive */
    }
    for (int i = 0; i < step->cost_count; ++i) {
        def_ids[i] = step->cost[i].def_id;
        counts[i] = step->cost[i].amount;
    }
    const uint32_t count = (uint32_t)step->cost_count;
    if (items_can_pay_stacks(s_scope, def_ids, counts, count) != ITEMS_RESULT_OK) {
        return false;
    }
    ProgressionTrackState *st = find_or_alloc_track(def->id);
    if (st == NULL) {
        return false; /* budget exhausted -- purse untouched */
    }
    if (items_try_pay_stacks(s_scope, def_ids, counts, count, reason) != ITEMS_RESULT_OK) {
        return false; /* state moved between the check and the payment */
    }
    *out_state = st;
    return true;
}

bool progression_level_up(progression_track_ref_t track, const char *reason) {
    progression_reason_check(reason);
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL || def->mode != PROGRESSION_MODE_MANUAL) {
        return false; /* auto/threshold are moved by progression_update() */
    }
    int level = progression_level(track);
    if (level >= def->max_level) {
        return false;
    }
    const progression_step_t *step = step_at(def, level);
    ProgressionTrackState *st = NULL;
    if (!buy_step(def, step, reason, &st)) {
        return false;
    }
    int old_level = st->level;
    st->level += 1;
    emit_levelup(def, reason, old_level, st->level, step, 0);
    apply_grants(step, "loot:levelup");
    game_save_mark_dirty();
    return true;
}

void progression_add_xp(progression_track_ref_t track, int64_t n, const char *reason) {
    progression_reason_check(reason);
    if (n <= 0) {
        return;
    }
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL || def->mode != PROGRESSION_MODE_THRESHOLD) {
        return; /* xp fed into an item-paid track is a silent no-op */
    }
    ProgressionTrackState *st = find_or_alloc_track(def->id);
    if (st == NULL) {
        return;
    }
    int64_t before = st->xp;
    if (n > PROGRESSION_STATE_TRACK_STATE_XP_MAX - before) {
        st->xp = PROGRESSION_STATE_TRACK_STATE_XP_MAX; /* clamp to the schema max */
    } else {
        st->xp = before + n;
    }
    progression_emit_xp_added(def->id, reason, st->xp - before, before, st->xp);
    game_save_mark_dirty();
}

void progression_set_level(progression_track_ref_t track, int level, const char *reason) {
    progression_reason_check(reason);
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL) {
        return;
    }
    ProgressionTrackState *st = find_or_alloc_track(def->id);
    if (st == NULL) {
        return;
    }
    int old_level = st->level;
    st->level = clamped_level(def, level); /* xp untouched: set_level is not a reset */
    progression_emit_level_set(def->id, reason, level, old_level, st->level);
    game_save_mark_dirty();
}

void progression_reset(progression_track_ref_t track, const char *reason) {
    progression_reason_check(reason);
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL) {
        return;
    }
    ProgressionTrackState *st = find_track(def->id);
    if (st == NULL) {
        return; /* no record -- already at the reset state */
    }
    int old_level = st->level;
    int64_t old_xp = st->xp;
    /* Free the slot outright: a record parked at 0/0 reads the same as no record
       but holds a tracks-map slot forever. The purse is not touched. */
    memset(st, 0, sizeof(*st));
    progression_emit_reset(def->id, reason, old_level, old_xp);
    game_save_mark_dirty();
}

/* ---- Tick ---- */

static void resolve_track(const progression_track_def_t *def) {
    if (def->mode == PROGRESSION_MODE_MANUAL) {
        return; /* manual is moved by progression_level_up(), never the tick */
    }
    ProgressionTrackState *st = find_track(def->id);
    int level = st ? st->level : 0;
    int iterations = 0;
    while (iterations < PROGRESSION_MAX_LEVELUPS_PER_TRACK && level < def->max_level) {
        const progression_step_t *step = step_at(def, level);
        int64_t xp_before = 0;
        int old_level = 0;
        if (def->mode == PROGRESSION_MODE_AUTO) {
            if (!buy_step(def, step, "level_cost:auto", &st)) {
                break;
            }
            old_level = st->level;
            st->level += 1;
            emit_levelup(def, "level_cost:auto", old_level, st->level, step, 0);
        } else {
            if ((st ? st->xp : 0) < step->xp_cost) {
                break;
            }
            st = find_or_alloc_track(def->id); /* only right before a real level-up */
            if (st == NULL) {
                break;
            }
            xp_before = st->xp;
            st->xp -= step->xp_cost;
            old_level = st->level;
            st->level += 1;
            emit_levelup(def, "level_cost:threshold", old_level, st->level, step, xp_before);
        }
        apply_grants(step, "loot:levelup");
        level = st->level;
        iterations += 1;
    }
    if (iterations == PROGRESSION_MAX_LEVELUPS_PER_TRACK) {
        nt_log_warn("progression: per-track levelup cap at '%s' (dropped rest this frame)", def->id);
    }
    if (iterations > 0) {
        game_save_mark_dirty();
    }
}

void progression_update(void) {
    for (int i = 0; i < k_tracks_count; ++i) {
        resolve_track(&k_tracks[i]);
    }
}
