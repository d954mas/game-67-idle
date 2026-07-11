#include "features/progression/progression.h"

#include "progression_tracks.gen.h" /* codegen: extern k_tracks/k_tracks_count */

#include "progression_state.h"             /* generated: ProgressionState + progression_state instance */
#include "progression_state_events.gen.h"  /* generated: progression_emit_levelup */

#include "core/nt_assert.h" /* NT_ASSERT (L2: catch track-id truncation loudly in debug, precedent items_containers.c) */
#include "game_save.h"       /* game_save_mark_dirty */
#include "log/nt_log.h"      /* nt_log_warn (T5 drop-with-warn) */

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* reason contract (lightweight; not the items verb list -- progression does not
   тянет чужой internal-хедер reason_tags.h). Формат "verb:subject"; debug-only,
   no-op в release. Спенды в purse передают reason В items_remove/add, где
   срабатывает ПОЛНЫЙ items-verb-чек -- verb-список валют не дублируется здесь. */
static inline void progression_reason_check(const char *reason) {
#ifndef NDEBUG
    assert(reason != NULL);
    const char *colon = strchr(reason, ':');
    assert(colon != NULL && colon[1] != '\0');
#else
    (void)reason;
#endif
}

/* Скан состояния напрямую (как items_containers.c -- генераторные find/alloc
   внутри progression_state.c статичны, недостижимы отсюда). find_track для
   ЧТЕНИЯ (никогда не аллоцирует); find_or_alloc_track вызывается ТОЛЬКО перед
   lazy allocation: reads and empty ticks do not create orphan records. */
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

static void emit_levelup(
    const progression_track_def_t *def,
    const char *cause,
    const char *reason,
    int64_t old_level,
    int64_t new_level,
    int64_t cost,
    int64_t resource_before,
    int64_t resource_after,
    int depth) {
    progression_emit_levelup(
        def->id,
        mode_name(def->mode),
        cause,
        reason,
        old_level,
        new_level,
        def->currency_def != NULL ? def->currency_def : "",
        cost,
        resource_before,
        resource_after,
        depth);
}

static ProgressionTrackState *find_track(const char *id) {
    if (id == NULL) {
        return NULL;
    }
    for (int i = 0; i < PROGRESSION_STATE_MAX_TRACKS; ++i) {
        if (progression_state.tracks[i].used && strcmp(progression_state.tracks[i].key, id) == 0) {
            return &progression_state.tracks[i];
        }
    }
    return NULL;
}

/* M-fix (deep-review): build the key into a validated local buffer FIRST --
   precedent items_containers.c:36-40 build_stack_key. A track_id that would
   truncate against PROGRESSION_STATE_STRING_MAX must be REJECTED, never
   silently written half-formed: a truncated key means find_track(full_id)
   can never match it again, so every subsequent mutation call would burn a
   FRESH slot instead of finding the existing one.
   In practice generate_progression_tracks.py now rejects an over-length
   track id at codegen time (loud reject), so this only ever fires for a
   hand-authored id (e.g. a test catalog) -- kept as defense-in-depth,
   mirroring items' own belt-and-suspenders posture. */
static ProgressionTrackState *find_or_alloc_track(const char *id) {
    ProgressionTrackState *existing = find_track(id);
    if (existing != NULL) {
        return existing;
    }
    char key[PROGRESSION_STATE_STRING_MAX];
    int n = snprintf(key, sizeof key, "%s", id);
    NT_ASSERT(n >= 0 && (size_t)n < sizeof key);
    if (n < 0 || (size_t)n >= sizeof key) {
        return NULL; /* truncated key -- reject, never silently corrupt */
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
    return NULL; /* PROGRESSION_STATE_MAX_TRACKS (32) budget exhausted */
}

#define PROGRESSION_MAX_LEVELUPS_PER_TRACK 64 /* T5: per-track per-frame while-кап (self-refund) */
#define PROGRESSION_MAX_CASCADE_DEPTH 8       /* T5: xp-to-track cascade recursion depth cap */

/* Forward declaration: progression_level_up() (manual mutation, below) and
   resolve_track() and tick updates both run the same on_level_up-emission
   path -- defined once, near progression_update() at the bottom of this file. */
static void apply_on_level_up(const progression_track_def_t *def, int depth);

const progression_track_def_t *progression_track_def(const char *track) {
    if (track == NULL) {
        return NULL;
    }
    for (int i = 0; i < k_tracks_count; ++i) {
        if (strcmp(k_tracks[i].id, track) == 0) {
            return &k_tracks[i];
        }
    }
    return NULL;
}

/* L-fix (deep-review #5): index through cost_count, not just max_level.
   cost_count is documented as "== max_level" in progression.h, but that
   invariant was never actually CHECKED at runtime -- a malformed catalog
   (codegen bug, or a hand-authored test catalog whose cost[] array is
   shorter than its declared max_level) would silently read past the array.
   Debug-assert both the invariant and the bound; release keeps the bound
   check only (defensive, cheap) since assert() is compiled out under NDEBUG. */
static int64_t track_cost_at(const progression_track_def_t *def, int level) {
    assert(def->cost_count == def->max_level);
    assert(level >= 0 && level < def->cost_count);
    return def->cost[level];
}

/* ---- Запросы (чистые чтения) ---- */

int progression_level(const char *track) {
    const ProgressionTrackState *st = find_track(track);
    return st ? st->level : 0;
}

int progression_max_level(const char *track) {
    const progression_track_def_t *def = progression_track_def(track);
    return def ? def->max_level : 0;
}

int64_t progression_xp_current(const char *track) {
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL) {
        return 0;
    }
    if (def->mode == PROGRESSION_MODE_MANUAL || def->mode == PROGRESSION_MODE_AUTO) {
        return items_purse(def->currency_def); /* L2 -> L1 dependency. */
    }
    const ProgressionTrackState *st = find_track(track);
    return st ? st->xp : 0;
}

int64_t progression_xp_needed(const char *track) {
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL) {
        return 0;
    }
    int level = progression_level(track);
    if (level >= def->max_level) {
        return 0;
    }
    return track_cost_at(def, level);
}

bool progression_can_level_up(const char *track) {
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL) {
        return false;
    }
    if (progression_level(track) >= def->max_level) {
        return false;
    }
    return progression_xp_current(track) >= progression_xp_needed(track);
}

/* ---- Mutations (reason required) ---- */

bool progression_level_up(const char *track, const char *reason) {
    progression_reason_check(reason);
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL || def->mode != PROGRESSION_MODE_MANUAL) {
        return false; /* auto/threshold are moved by progression_update(), not this call */
    }
    int level = progression_level(track);
    if (level >= def->max_level) {
        return false;
    }
    int64_t cost = track_cost_at(def, level);
    if (items_purse(def->currency_def) < cost) {
        return false;
    }
    /* H-fix (deep-review #1, data-loss): allocate BEFORE spending -- mirrors
       resolve_track's already-correct tick-path order. The old order (spend
       first, alloc second) meant a saturated tracks-map (32/32 used) would
       silently BURN the player's currency with the level never recorded and
       no way to recover it. Budget exhaustion must fail closed with the
       purse untouched. */
    ProgressionTrackState *st = find_or_alloc_track(def->id);
    if (st == NULL) {
        return false; /* PROGRESSION_STATE_MAX_TRACKS budget exhausted -- purse untouched */
    }
    int old_level = st->level;
    int64_t resource_before = items_purse(def->currency_def);
    if (!items_remove("purse", def->currency_def, cost, reason)) {
        return false; /* defensive: items-side verb-check/purse mismatch; level not bumped */
    }
    st->level += 1;
    emit_levelup(def, "manual", reason, old_level, st->level, cost, resource_before, items_purse(def->currency_def), 0);
    apply_on_level_up(def, 0); /* Cut A: shipped/demo on_level_up is always empty -> no-op */
    game_save_mark_dirty();
    return true;
}

void progression_add_xp(const char *track, int64_t n, const char *reason) {
    progression_reason_check(reason);
    if (n <= 0) {
        return;
    }
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL || def->mode != PROGRESSION_MODE_THRESHOLD) {
        return; /* #17: xp fed into a non-threshold track (stray cascade) is a silent no-op */
    }
    ProgressionTrackState *st = find_or_alloc_track(def->id);
    if (st == NULL) {
        return; /* PROGRESSION_STATE_MAX_TRACKS budget exhausted (defensive) */
    }
    int64_t before = st->xp;
    if (n > PROGRESSION_STATE_TRACK_STATE_XP_MAX - before) {
        st->xp = PROGRESSION_STATE_TRACK_STATE_XP_MAX; /* overflow guard, clamp to schema max */
    } else {
        st->xp = before + n;
    }
    progression_emit_xp_added(def->id, reason, st->xp - before, before, st->xp);
    game_save_mark_dirty();
}

void progression_set_level(const char *track, int level, const char *reason) {
    progression_reason_check(reason);
    const progression_track_def_t *def = progression_track_def(track);
    if (def == NULL) {
        return;
    }
    int clamped = level;
    if (clamped < 0) {
        clamped = 0;
    } else if (clamped > def->max_level) {
        clamped = def->max_level;
    }
    ProgressionTrackState *st = find_or_alloc_track(def->id);
    if (st == NULL) {
        return; /* PROGRESSION_STATE_MAX_TRACKS budget exhausted (defensive) */
    }
    int old_level = st->level;
    st->level = clamped; /* xp untouched: set_level differs from prestige reset. */
    progression_emit_level_set(def->id, reason, level, old_level, st->level);
    game_save_mark_dirty();
}

void progression_reset(const char *track, const char *reason) {
    progression_reason_check(reason);
    ProgressionTrackState *st = find_track(track);
    if (st == NULL) {
        return; /* no record -- already at the reset state, no-op */
    }
    int old_level = st->level;
    int64_t old_xp = st->xp;
    /* L-fix (deep-review #4): free the slot entirely (used=false), not just
       zero level/xp in place -- precedent items remove_raw at count<=0
       (items_containers.c:156-158). A record parked at level=0/xp=0 is
       indistinguishable in EFFECT from no record (lazy-default reads both as
       0) but needlessly holds a tracks-map slot forever after a
       prestige reset. */
    memset(st, 0, sizeof(*st)); /* used=false -> slot freed; reset does NOT touch purse (#15) */
    progression_emit_reset(track, reason, old_level, old_xp);
    game_save_mark_dirty();
}

/* ---- Тик: auto/threshold авто-лвлапы, T5 HARD-капы, эмит progression.levelup ---- */

static void resolve_track(const progression_track_def_t *def, int depth) {
    if (def == NULL) {
        return;
    }
    if (def->mode == PROGRESSION_MODE_MANUAL) {
        return; /* manual is moved by progression_level_up(), never the tick */
    }
    if (depth > PROGRESSION_MAX_CASCADE_DEPTH) {
        nt_log_warn("progression: cascade depth cap at '%s' (dropped)", def->id);
        return; /* T5: A->B->A xp-cascade depth cap */
    }
    /* #6: ЛЕНИВО. Не аллоцируем запись на входе (иначе кадр 1 создаёт нулевые
       записи всех auto/threshold-треков -> нарушит инвариант "свежая игра =
       пустые треки". Читаем уровень через find_track (NULL -> 0); alloc ТОЛЬКО
       перед реальным лвлапом. */
    ProgressionTrackState *st = find_track(def->id);
    int level = st ? st->level : 0;
    int iters = 0;
    while (iters < PROGRESSION_MAX_LEVELUPS_PER_TRACK) {
        if (level >= def->max_level) {
            break;
        }
        int64_t cost = track_cost_at(def, level);
        if (def->mode == PROGRESSION_MODE_AUTO) {
            if (items_purse(def->currency_def) < cost) {
                break; /* purse empty -> records not created for nothing */
            }
        } else { /* THRESHOLD */
            if ((st ? st->xp : 0) < cost) {
                break;
            }
        }
        if (st == NULL) {
            st = find_or_alloc_track(def->id); /* lazy alloc: only right before a REAL level-up */
            if (st == NULL) {
                break; /* PROGRESSION_STATE_MAX_TRACKS budget exhausted */
            }
        }
        if (def->mode == PROGRESSION_MODE_AUTO) {
            /* L-fix (deep-review #3): check the return value -- the manual
               path already does (progression_level_up above); this was the
               one asymmetric spot that granted a "free" level on an
               items-side rejection instead of treating it as a hard stop. */
            int64_t resource_before = items_purse(def->currency_def);
            if (!items_remove("purse", def->currency_def, cost, "level_cost:auto")) {
                nt_log_warn("progression: items_remove failed for '%s' despite affordability check (purse desync?)", def->id);
                break; /* do not grant a level the player didn't pay for */
            }
            int old_level = st->level;
            st->level += 1;
            level = st->level;
            emit_levelup(def, "auto", "level_cost:auto", old_level, st->level, cost, resource_before, items_purse(def->currency_def), depth);
        } else {
            int64_t resource_before = st->xp;
            st->xp -= cost;
            int old_level = st->level;
            st->level += 1;
            level = st->level;
            emit_levelup(def, "threshold", "level_cost:threshold", old_level, st->level, cost, resource_before, st->xp, depth);
        }
        apply_on_level_up(def, depth);                            /* каскад внутрь той же глубины-проверки */
        iters += 1;
    }
    if (iters == PROGRESSION_MAX_LEVELUPS_PER_TRACK) {
        nt_log_warn("progression: per-track levelup cap at '%s' (dropped rest this frame)", def->id);
    }
    if (iters > 0) {
        game_save_mark_dirty();
    }
}

/* Cut A: shipped/demo on_level_up is ALWAYS empty (codegen emits NULL,0) -> this
   loop is a no-op in the template. The runtime path stays alive, covered ONLY by
   the hand-written test catalog (tests/test_progression_catalog.c). */
static void apply_on_level_up(const progression_track_def_t *def, int depth) {
    for (int i = 0; i < def->on_level_up_count; ++i) {
        const progression_emit_t *e = &def->on_level_up[i];
        if (e->def_id != NULL) {
            items_add("purse", e->def_id, e->amount, "loot:levelup");
        }
        if (e->to_track != NULL) {
            progression_add_xp(e->to_track, e->amount, "convert:cascade");
            resolve_track(progression_track_def(e->to_track), depth + 1);
            /* #17: if to_track is not threshold, add_xp above is a silent no-op. */
        }
    }
}

void progression_update(void) {
    for (int i = 0; i < k_tracks_count; ++i) {
        resolve_track(&k_tracks[i], 0);
    }
}
