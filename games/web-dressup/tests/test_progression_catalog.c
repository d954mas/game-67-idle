#include "features/progression/progression.h"

#include <stddef.h>

/* РУКОПИСНЫЙ тест-каталог. test_progression links
   THIS file, never progression_tracks.gen.c (anti-duplicate k_tracks symbol,
   R10) -- controlled tracks for exact assertions, plus two pathological
   tracks that exist ONLY to exercise the T5 HARD caps (§2.4/§5.6):
     - `runaway`: an auto track whose on_level_up refunds >= its own cost
       every level -- without PROGRESSION_MAX_LEVELUPS_PER_TRACK the while
       loop in resolve_track() never terminates.
     - `casc_a` / `casc_b`: a mutual threshold xp-cascade (A's on_level_up
       feeds B, B's feeds A) -- without PROGRESSION_MAX_CASCADE_DEPTH the
       resolve_track() recursion never terminates.
   Both use demo items currencies (tmpl.gold/tmpl.xp) from content/items.json
   -- this test binary links the demo items catalog + items runtime. */

static const int64_t COST_MAN[] = {10LL, 20LL, 30LL};
static const int64_t COST_AUTO1[] = {5LL, 5LL, 5LL, 5LL, 5LL};
static const int64_t COST_THR[] = {10LL, 10LL, 10LL, 10LL, 10LL};

/* 100 levels, all costing 10 -- comfortably above PROGRESSION_MAX_LEVELUPS_PER_TRACK
   (64) so the T5 per-track cap fires well before max_level would (proves the
   CAP stopped it, not running out of levels). */
static const int64_t COST_RUNAWAY[] = {
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
    10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL, 10LL,
};

/* 20 levels, all costing 5 -- comfortably above PROGRESSION_MAX_CASCADE_DEPTH
   (8) so the depth cap fires well before max_level would. */
static const int64_t COST_CASC[] = {
    5LL, 5LL, 5LL, 5LL, 5LL, 5LL, 5LL, 5LL, 5LL, 5LL,
    5LL, 5LL, 5LL, 5LL, 5LL, 5LL, 5LL, 5LL, 5LL, 5LL,
};

static const progression_emit_t RUNAWAY_ON_LEVEL_UP[] = {
    {.def_id = "tmpl.xp", .to_track = NULL, .amount = 10}, /* self-refund == cost -> per-track cap */
};

static const progression_emit_t CASC_A_ON_LEVEL_UP[] = {
    {.def_id = NULL, .to_track = "casc_b", .amount = 5}, /* A -> B */
};

static const progression_emit_t CASC_B_ON_LEVEL_UP[] = {
    {.def_id = NULL, .to_track = "casc_a", .amount = 5}, /* B -> A: closes the A<->B loop */
};

const progression_track_def_t k_tracks[] = {
    {
        .id = "man",
        .mode = PROGRESSION_MODE_MANUAL,
        .currency_def = "tmpl.gold",
        .max_level = 3,
        .cost = COST_MAN,
        .cost_count = 3,
        .on_level_up = NULL,
        .on_level_up_count = 0,
    },
    {
        .id = "auto1",
        .mode = PROGRESSION_MODE_AUTO,
        .currency_def = "tmpl.xp",
        .max_level = 5,
        .cost = COST_AUTO1,
        .cost_count = 5,
        .on_level_up = NULL,
        .on_level_up_count = 0,
    },
    {
        .id = "thr",
        .mode = PROGRESSION_MODE_THRESHOLD,
        .currency_def = NULL,
        .max_level = 5,
        .cost = COST_THR,
        .cost_count = 5,
        .on_level_up = NULL,
        .on_level_up_count = 0,
    },
    {
        .id = "runaway",
        .mode = PROGRESSION_MODE_AUTO,
        .currency_def = "tmpl.xp",
        .max_level = 100,
        .cost = COST_RUNAWAY,
        .cost_count = 100,
        .on_level_up = RUNAWAY_ON_LEVEL_UP,
        .on_level_up_count = 1,
    },
    {
        .id = "casc_a",
        .mode = PROGRESSION_MODE_THRESHOLD,
        .currency_def = NULL,
        .max_level = 20,
        .cost = COST_CASC,
        .cost_count = 20,
        .on_level_up = CASC_A_ON_LEVEL_UP,
        .on_level_up_count = 1,
    },
    {
        .id = "casc_b",
        .mode = PROGRESSION_MODE_THRESHOLD,
        .currency_def = NULL,
        .max_level = 20,
        .cost = COST_CASC,
        .cost_count = 20,
        .on_level_up = CASC_B_ON_LEVEL_UP,
        .on_level_up_count = 1,
    },
};

const int k_tracks_count = (int)(sizeof k_tracks / sizeof k_tracks[0]);
