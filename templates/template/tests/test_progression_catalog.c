#include "features/progression/progression.h"

#include <stddef.h>

/* HAND-WRITTEN test catalog. test_progression links THIS file, never
   progression_tracks.gen.c (one k_tracks symbol), so the tracks below can be
   shaped for exact assertions rather than for the demo.

   `runaway` exists only to exercise the per-frame level-up cap: an auto track
   whose grant nearly covers its own price buys a level every iteration, and
   without PROGRESSION_MAX_LEVELUPS_PER_TRACK the tick's while loop would run
   until the track maxed. The catalog gate rejects a grant that FULLY covers the
   price, so the hand-written pathological case stays just under it.

   `mixed` carries both column kinds so the exact/fractional reads have
   something to read. Its indices are the ones the generator would emit for a
   column dictionary sorted by field id. */

static const progression_amount_t GOLD_10[] = {{"tmpl.gold", 10}};
static const progression_amount_t GOLD_20[] = {{"tmpl.gold", 20}};
static const progression_amount_t GOLD_30[] = {{"tmpl.gold", 30}};
static const progression_amount_t XP_5[] = {{"tmpl.xp", 5}};
static const progression_amount_t XP_10[] = {{"tmpl.xp", 10}};
static const progression_amount_t XP_REFUND_9[] = {{"tmpl.xp", 9}};
static const progression_amount_t GOLD_1_WOOD_2[] = {{"tmpl.gold", 1}, {"tmpl.wood", 2}};

static const progression_step_t STEPS_MAN[] = {
    {GOLD_10, 1, 0, NULL, 0},
    {GOLD_20, 1, 0, NULL, 0},
    {GOLD_30, 1, 0, NULL, 0},
};

static const progression_step_t STEPS_AUTO1[] = {
    {XP_5, 1, 0, NULL, 0}, {XP_5, 1, 0, NULL, 0}, {XP_5, 1, 0, NULL, 0},
    {XP_5, 1, 0, NULL, 0}, {XP_5, 1, 0, NULL, 0},
};

static const progression_step_t STEPS_THR[] = {
    {NULL, 0, 10, NULL, 0}, {NULL, 0, 10, NULL, 0}, {NULL, 0, 10, NULL, 0},
    {NULL, 0, 10, NULL, 0}, {NULL, 0, 10, NULL, 0},
};

/* 100 levels priced at 10 with a 9 refund: comfortably above the per-track cap
   (64), so the cap is what stops it, not running out of levels. */
#define STEP_RUNAWAY {XP_10, 1, 0, XP_REFUND_9, 1}
#define STEP_RUNAWAY_10 \
    STEP_RUNAWAY, STEP_RUNAWAY, STEP_RUNAWAY, STEP_RUNAWAY, STEP_RUNAWAY, \
    STEP_RUNAWAY, STEP_RUNAWAY, STEP_RUNAWAY, STEP_RUNAWAY, STEP_RUNAWAY
static const progression_step_t STEPS_RUNAWAY[] = {
    STEP_RUNAWAY_10, STEP_RUNAWAY_10, STEP_RUNAWAY_10, STEP_RUNAWAY_10, STEP_RUNAWAY_10,
    STEP_RUNAWAY_10, STEP_RUNAWAY_10, STEP_RUNAWAY_10, STEP_RUNAWAY_10, STEP_RUNAWAY_10,
};

/* A two-resource price, so the atomic multi-item payment has a real caller. */
static const progression_step_t STEPS_MIXED[] = {
    {GOLD_1_WOOD_2, 2, 0, NULL, 0},
    {GOLD_1_WOOD_2, 2, 0, NULL, 0},
};

/* [level][column] over two columns: 0 exact, 1 fractional. */
static const int64_t EXACT_MIXED[] = {0, 0, 4, 0, 9, 0};
static const double FRACTIONAL_MIXED[] = {0.0, 1.0, 0.0, 1.25, 0.0, 1.5};

const bool k_progression_value_exact[] = {true, false};

const progression_track_def_t k_tracks[] = {
    {
        .id = "man",
        .mode = PROGRESSION_MODE_MANUAL,
        .max_level = 3,
        .steps = STEPS_MAN,
        .exact = NULL,
        .fractional = NULL,
        .value_count = 0u,
    },
    {
        .id = "auto1",
        .mode = PROGRESSION_MODE_AUTO,
        .max_level = 5,
        .steps = STEPS_AUTO1,
        .exact = NULL,
        .fractional = NULL,
        .value_count = 0u,
    },
    {
        .id = "thr",
        .mode = PROGRESSION_MODE_THRESHOLD,
        .max_level = 5,
        .steps = STEPS_THR,
        .exact = NULL,
        .fractional = NULL,
        .value_count = 0u,
    },
    {
        .id = "runaway",
        .mode = PROGRESSION_MODE_AUTO,
        .max_level = 100,
        .steps = STEPS_RUNAWAY,
        .exact = NULL,
        .fractional = NULL,
        .value_count = 0u,
    },
    {
        .id = "mixed",
        .mode = PROGRESSION_MODE_MANUAL,
        .max_level = 2,
        .steps = STEPS_MIXED,
        .exact = EXACT_MIXED,
        .fractional = FRACTIONAL_MIXED,
        .value_count = 2u,
    },
};

const int k_tracks_count = (int)(sizeof k_tracks / sizeof k_tracks[0]);
