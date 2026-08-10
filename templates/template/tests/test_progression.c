/* System headers before Unity (noreturn/__declspec conflict on MSVC, ср.
   test_items_fragment.c/test_game_save.c). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

#include "features/progression/progression.h"
#include "features/items/items.h"
#include "game_events.h"
#include "items_state.h"
#include "progression_state.h"
#include "progression_state_events.gen.h"

/* Full-stack И3a test: progression logic + items runtime + generated state
   layers, over the hand-written progression catalog
   (tests/test_progression_catalog.c, §5.7) -- NOT progression_tracks.gen.c
   (anti-duplicate k_tracks symbol, R10). The generated progression catalog is
   exercised separately by
   tests/test_progression_curve.c). game_save.c is NOT linked -- items_containers.c/
   items_bootstrap.c/progression.c only need game_save_mark_dirty(), stubbed
   below (precedent: test_items_fragment.c). */
void game_save_mark_dirty(void) {}

static items_container_ref_t s_resources = ITEMS_CONTAINER_REF_NONE;

/* Resolved once per reset: every call below takes the handle, never the id. */
static progression_track_ref_t s_man, s_auto1, s_thr, s_runaway, s_mixed;

static void reset_test_state(void) {
    char error[128] = {0};
    items_state_fragment.reset();
    TEST_ASSERT_TRUE_MESSAGE(items_runtime_rebuild(error, (int)sizeof(error)), error);
    items_container_desc_t desc = {
        .capacity = 32,
        .policy = ITEMS_CONTAINER_POLICY_GENERIC,
        .lifetime = ITEMS_LIFETIME_PERSISTENT,
    };
    TEST_ASSERT_EQUAL_INT(
        ITEMS_RESULT_OK, items_try_container_create(desc, &s_resources));
    progression_state_fragment.reset();
    items_payment_scope_t scope = {.count = 1, .containers = {s_resources}};
    progression_bind_payment_scope(scope);
    s_man = progression_track("man");
    s_auto1 = progression_track("auto1");
    s_thr = progression_track("thr");
    s_runaway = progression_track("runaway");
    s_mixed = progression_track("mixed");
}

static bool resource_add(const char *def_id, int64_t count, const char *reason) {
    return items_try_stack_add(
               s_resources, def_id, count, ITEMS_SLOT_AUTO, reason, NULL, NULL) == ITEMS_RESULT_OK;
}

static int64_t resource_count(const char *def_id) {
    return items_stack_count(s_resources, def_id);
}

void setUp(void) {
    game_event_frame_reset();
    reset_test_state();
}
void tearDown(void) {}

static const ProgressionEvLevelup *find_levelup(
    const char *track, const char *reason, int64_t old_level, int64_t new_level) {
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t levelup_type = progression_ev_levelup_type();
    for (int i = 0; i < n; ++i) {
        if (log[i].type.value != levelup_type.value) {
            continue;
        }
        const ProgressionEvLevelup *e = (const ProgressionEvLevelup *)log[i].payload;
        if (strcmp(progression_ev_levelup_track(e), track) == 0 &&
            strcmp(progression_ev_levelup_reason(e), reason) == 0 &&
            e->old_level == old_level && e->new_level == new_level) {
            return e;
        }
    }
    return NULL;
}

/* An item-paid levelup carries the price in cost[] and leaves the xp pair at zero. */
static bool levelup_paid_event_exists(
    const char *track,
    const char *mode,
    const char *reason,
    int64_t old_level,
    int64_t new_level,
    const char *def_id,
    int64_t amount,
    int64_t before) {
    const ProgressionEvLevelup *e = find_levelup(track, reason, old_level, new_level);
    return e != NULL &&
           strcmp(progression_ev_levelup_mode(e), mode) == 0 &&
           e->xp_cost == 0 && e->xp_before == 0 &&
           progression_ev_levelup_cost_count(e) == 1u &&
           strcmp(progression_ev_levelup_cost_def_id(e, 0), def_id) == 0 &&
           progression_ev_levelup_cost_at(e, 0)->amount == amount &&
           progression_ev_levelup_cost_at(e, 0)->before == before;
}

/* A threshold levelup spends its own accumulator, so it names no item at all. */
static bool levelup_threshold_event_exists(
    const char *track,
    const char *reason,
    int64_t old_level,
    int64_t new_level,
    int64_t xp_cost,
    int64_t xp_before) {
    const ProgressionEvLevelup *e = find_levelup(track, reason, old_level, new_level);
    return e != NULL &&
           strcmp(progression_ev_levelup_mode(e), "threshold") == 0 &&
           e->xp_cost == xp_cost && e->xp_before == xp_before &&
           progression_ev_levelup_cost_count(e) == 0u;
}

static bool xp_added_event_exists(const char *track, const char *reason, int64_t delta, int64_t before_xp, int64_t after_xp) {
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t type = progression_ev_xp_added_type();
    for (int i = 0; i < n; ++i) {
        if (log[i].type.value != type.value) {
            continue;
        }
        const ProgressionEvXpAdded *e = (const ProgressionEvXpAdded *)log[i].payload;
        if (strcmp(progression_ev_xp_added_track(e), track) == 0 &&
            strcmp(progression_ev_xp_added_reason(e), reason) == 0 &&
            e->delta == delta && e->before_xp == before_xp && e->after_xp == after_xp) {
            return true;
        }
    }
    return false;
}

static bool level_set_event_exists(
    const char *track,
    const char *reason,
    int64_t requested_level,
    int64_t old_level,
    int64_t new_level) {
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t type = progression_ev_level_set_type();
    for (int i = 0; i < n; ++i) {
        if (log[i].type.value != type.value) {
            continue;
        }
        const ProgressionEvLevelSet *e = (const ProgressionEvLevelSet *)log[i].payload;
        if (strcmp(progression_ev_level_set_track(e), track) == 0 &&
            strcmp(progression_ev_level_set_reason(e), reason) == 0 &&
            e->requested_level == requested_level &&
            e->old_level == old_level &&
            e->new_level == new_level) {
            return true;
        }
    }
    return false;
}

static bool reset_event_exists(const char *track, const char *reason, int64_t old_level, int64_t old_xp) {
    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t type = progression_ev_reset_type();
    for (int i = 0; i < n; ++i) {
        if (log[i].type.value != type.value) {
            continue;
        }
        const ProgressionEvReset *e = (const ProgressionEvReset *)log[i].payload;
        if (strcmp(progression_ev_reset_track(e), track) == 0 &&
            strcmp(progression_ev_reset_reason(e), reason) == 0 &&
            e->old_level == old_level &&
            e->old_xp == old_xp) {
            return true;
        }
    }
    return false;
}

/* ---- the price of the next level ---- */

void test_cost_of_the_next_level_follows_the_current_one(void) {
    reset_test_state();

    const progression_track_def_t *man = progression_track_def(s_man);
    TEST_ASSERT_NOT_NULL(man);
    TEST_ASSERT_EQUAL_INT64(10, man->steps[0].cost[0].amount);
    TEST_ASSERT_EQUAL_INT64(20, man->steps[1].cost[0].amount);
    TEST_ASSERT_EQUAL_INT64(30, man->steps[2].cost[0].amount);

    TEST_ASSERT_EQUAL_UINT32(1u, progression_cost_count(s_man));
    TEST_ASSERT_EQUAL_STRING("tmpl.gold", progression_cost_at(s_man, 0).def_id);
    TEST_ASSERT_EQUAL_INT64(10, progression_cost_at(s_man, 0).amount);

    progression_set_level(s_man, 3, "admin:test"); /* clamp to max */
    /* At the cap there is no next level, so there is no price to name. */
    TEST_ASSERT_EQUAL_UINT32(0u, progression_cost_count(s_man));
    TEST_ASSERT_NULL(progression_cost_at(s_man, 0).def_id);
}

/* An unresolved handle answers like an absent record instead of crashing. */
void test_unknown_track_reads_as_empty(void) {
    reset_test_state();

    progression_track_ref_t missing = progression_track("no_such_track");
    TEST_ASSERT_FALSE(progression_track_valid(missing));
    TEST_ASSERT_NULL(progression_track_def(missing));
    TEST_ASSERT_EQUAL_INT(0, progression_level(missing));
    TEST_ASSERT_EQUAL_UINT32(0u, progression_cost_count(missing));
    TEST_ASSERT_FALSE(progression_level_up(missing, "level_cost:test"));
}

/* int64-край (§5.8): a struct literal with a near-int64-max cost round-trips
   without truncation -- a pure C-level sanity check independent of the test
   catalog above (this def is NOT registered in k_tracks). */
void test_int64_cost_no_truncation(void) {
    static const progression_amount_t huge_cost[] = {{"tmpl.gold", 9000000000000000000LL}};
    static const progression_step_t huge_steps[] = {{huge_cost, 1, 0, NULL, 0}};
    progression_track_def_t huge_def = {
        .id = "huge",
        .mode = PROGRESSION_MODE_MANUAL,
        .max_level = 1,
        .steps = huge_steps,
        .exact = NULL,
        .fractional = NULL,
        .value_count = 0u,
    };
    TEST_ASSERT_EQUAL_INT64(9000000000000000000LL, huge_def.steps[0].cost[0].amount);
}

/* ---- manual mode ---- */

void test_manual_level_up_spends_purse(void) {
    reset_test_state();

    TEST_ASSERT_TRUE(resource_add("tmpl.gold", 25, "cheat:test"));

    TEST_ASSERT_TRUE(progression_level_up(s_man, "level_cost:test"));
    TEST_ASSERT_EQUAL_INT(1, progression_level(s_man));
    TEST_ASSERT_EQUAL_INT64(15, resource_count("tmpl.gold")); /* 25 - cost[0]=10 */
    TEST_ASSERT_TRUE(levelup_paid_event_exists(
        "man", "manual", "level_cost:test", 0, 1, "tmpl.gold", 10, 25));

    /* cost[1]=20 > remaining 15 -> insufficient, level_up rejects, level stays put. */
    TEST_ASSERT_FALSE(progression_level_up(s_man, "level_cost:test"));
    TEST_ASSERT_EQUAL_INT(1, progression_level(s_man));
    TEST_ASSERT_EQUAL_INT64(15, resource_count("tmpl.gold")); /* untouched by the rejected call */
}

/* H-fix regression (deep-review #1, data-loss): saturate the 32-slot tracks
   map with fabricated foreign records (white-box, precedent
   test_items_fragment.c's capacity-reject tests) so find_or_alloc_track("man")
   has nowhere to land. Before the fix, items_remove() ran BEFORE the alloc
   check -- the player's gold would have been spent and lost with no level
   ever recorded. After the fix, the alloc failure must be caught BEFORE any
   currency is touched. */
void test_manual_level_up_budget_exhausted_leaves_purse_untouched(void) {
    reset_test_state();

    for (int i = 0; i < PROGRESSION_STATE_MAX_TRACKS; ++i) {
        ProgressionTrackState *slot = &progression_state.tracks[i];
        slot->used = true;
        (void)snprintf(slot->key, sizeof slot->key, "fake_%d", i); /* none of these is "man" */
        slot->level = 0;
        slot->xp = 0;
    }

    TEST_ASSERT_TRUE(resource_add("tmpl.gold", 25, "cheat:test"));
    TEST_ASSERT_FALSE(progression_level_up(s_man, "level_cost:test")); /* budget exhausted -- must fail closed */
    TEST_ASSERT_EQUAL_INT64(25, resource_count("tmpl.gold"));          /* resource container untouched */
    TEST_ASSERT_EQUAL_INT(0, progression_level(s_man));                /* never got a record -- lazy default reads 0 */
}

/* ---- auto mode (tick) ---- */

void test_auto_tick_buys_while_affordable(void) {
    reset_test_state();

    TEST_ASSERT_TRUE(resource_add("tmpl.xp", 12, "cheat:test"));
    progression_update();

    /* auto1 cost {5,5,5,5,5}: 12 -> buys level0 (7 left) -> buys level1 (2 left) ->
       cost[2]=5 > 2, stops. */
    TEST_ASSERT_EQUAL_INT(2, progression_level(s_auto1));
    TEST_ASSERT_EQUAL_INT64(2, resource_count("tmpl.xp"));
    /* An item-paid track has no accumulator of its own; its balance is the purse. */
    TEST_ASSERT_EQUAL_INT64(0, progression_xp_current(s_auto1));
}

/* ---- threshold mode (tick) ---- */

void test_threshold_tick_buys_from_internal_xp(void) {
    reset_test_state();

    progression_add_xp(s_thr, 25, "loot:test");
    TEST_ASSERT_TRUE(xp_added_event_exists("thr", "loot:test", 25, 0, 25));
    progression_update();

    /* thr cost {10,10,10,10,10}: 25 -> buys level0 (15 left) -> buys level1 (5 left) ->
       cost[2]=10 > 5, stops. */
    TEST_ASSERT_EQUAL_INT(2, progression_level(s_thr));
    TEST_ASSERT_EQUAL_INT64(5, progression_xp_current(s_thr)); /* internal accumulator, not purse */
    /* The price is xp, so it rides xp_cost/xp_before and cost[] stays empty. */
    TEST_ASSERT_TRUE(levelup_threshold_event_exists("thr", "level_cost:threshold", 0, 1, 10, 25));
    TEST_ASSERT_TRUE(levelup_threshold_event_exists("thr", "level_cost:threshold", 1, 2, 10, 15));
}

/* ---- set_level (Р6: prologue) ---- */

void test_set_level_clamps_and_leaves_xp_untouched(void) {
    reset_test_state();

    progression_set_level(s_man, 3, "admin:prologue");
    TEST_ASSERT_EQUAL_INT(3, progression_level(s_man)); /* == max_level */
    TEST_ASSERT_TRUE(level_set_event_exists("man", "admin:prologue", 3, 0, 3));

    game_event_frame_reset();
    progression_set_level(s_man, 99, "admin:prologue"); /* clamp above max */
    TEST_ASSERT_EQUAL_INT(3, progression_level(s_man));
    TEST_ASSERT_TRUE(level_set_event_exists("man", "admin:prologue", 99, 3, 3));

    game_event_frame_reset();
    progression_set_level(s_man, 1, "admin:prologue"); /* lowered (e.g. a weakened hero) */
    TEST_ASSERT_EQUAL_INT(1, progression_level(s_man));
    TEST_ASSERT_TRUE(level_set_event_exists("man", "admin:prologue", 1, 3, 1));
}

/* ---- reset (Р6: prestige) ---- */

void test_reset_zeroes_level_and_internal_xp(void) {
    reset_test_state();

    progression_add_xp(s_thr, 25, "loot:test");
    progression_update();
    TEST_ASSERT_EQUAL_INT(2, progression_level(s_thr)); /* precondition, see threshold test above */

    progression_reset(s_thr, "admin:prestige");
    TEST_ASSERT_EQUAL_INT(0, progression_level(s_thr));
    TEST_ASSERT_EQUAL_INT64(0, progression_xp_current(s_thr));
    TEST_ASSERT_TRUE(reset_event_exists("thr", "admin:prestige", 2, 5));

    /* L-fix (deep-review #4): reset FREES the slot (used=false), not just
       zeroes level/xp in place -- precedent items remove_raw at count<=0. */
    bool slot_freed = true;
    for (int i = 0; i < PROGRESSION_STATE_MAX_TRACKS; ++i) {
        if (progression_state.tracks[i].used && strcmp(progression_state.tracks[i].key, "thr") == 0) {
            slot_freed = false;
        }
    }
    TEST_ASSERT_TRUE(slot_freed);
}

/* ---- progression.levelup event ---- */

void test_levelup_events_include_context_for_auto_and_manual(void) {
    reset_test_state();

    TEST_ASSERT_TRUE(resource_add("tmpl.xp", 12, "cheat:test"));
    progression_update(); /* auto1: 0->1->2 (two levelups) */

    TEST_ASSERT_TRUE(levelup_paid_event_exists(
        "auto1", "auto", "level_cost:auto", 0, 1, "tmpl.xp", 5, 12));
    TEST_ASSERT_TRUE(levelup_paid_event_exists(
        "auto1", "auto", "level_cost:auto", 1, 2, "tmpl.xp", 5, 7));

    /* Manual level_up is also a fact event now; analytics should not infer it from items.txn. */
    game_event_frame_reset();
    TEST_ASSERT_TRUE(resource_add("tmpl.gold", 25, "cheat:test"));
    TEST_ASSERT_TRUE(progression_level_up(s_man, "level_cost:test"));
    TEST_ASSERT_TRUE(levelup_paid_event_exists(
        "man", "manual", "level_cost:test", 0, 1, "tmpl.gold", 10, 25));
}

/* ---- T5 HARD caps (G6 -- anti-hang, критично) ---- */

void test_t5_per_track_cap_self_refund_terminates(void) {
    reset_test_state();

    TEST_ASSERT_TRUE(resource_add("tmpl.xp", 100, "cheat:test"));
    progression_update(); /* MUST return -- proves the per-track cap, not a hang */

    TEST_ASSERT_EQUAL_INT(64, progression_level(s_runaway)); /* exactly the cap: not 100, not forever */
}

/* ---- round-trip byte-stable (G4) ---- */

void test_round_trip_byte_stable(void) {
    reset_test_state();

    TEST_ASSERT_TRUE(resource_add("tmpl.xp", 12, "cheat:test"));
    progression_update(); /* auto1 -> level 2, xp record allocated */

    cJSON *ja = progression_state_to_json(&progression_state);
    TEST_ASSERT_NOT_NULL(ja);
    char *sa = cJSON_PrintUnformatted(ja);
    TEST_ASSERT_NOT_NULL(sa);

    ProgressionState reloaded;
    char err[128] = {0};
    TEST_ASSERT_TRUE(progression_state_from_json(&reloaded, ja, err, (int)sizeof(err)));
    cJSON *jb = progression_state_to_json(&reloaded);
    char *sb = cJSON_PrintUnformatted(jb);
    TEST_ASSERT_NOT_NULL(sb);

    TEST_ASSERT_EQUAL_STRING(sa, sb);

    cJSON_free(sa);
    cJSON_free(sb);
    cJSON_Delete(ja);
    cJSON_Delete(jb);
}

/* ---- lazy allocation (#6) ---- */

void test_lazy_allocation_no_gratuitous_records(void) {
    reset_test_state();

    TEST_ASSERT_EQUAL_INT(0, progression_level(s_man)); /* no record yet -> 0, not a crash */

    cJSON *json_empty = progression_state_to_json(&progression_state);
    const cJSON *tracks_empty = cJSON_GetObjectItemCaseSensitive(json_empty, "tracks");
    TEST_ASSERT_NOT_NULL(tracks_empty);
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(tracks_empty));
    cJSON_Delete(json_empty);

    /* Empty purse/xp -> a tick over auto/threshold tracks buys nothing and must
       not allocate a record for any of them; fresh games keep tracks empty. */
    progression_update();
    cJSON *json_after = progression_state_to_json(&progression_state);
    const cJSON *tracks_after = cJSON_GetObjectItemCaseSensitive(json_after, "tracks");
    TEST_ASSERT_NOT_NULL(tracks_after);
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(tracks_after));
    cJSON_Delete(json_after);
}

/* ---- columns ---- */

void test_columns_answer_the_read_their_type_declares(void) {
    reset_test_state();

    TEST_ASSERT_EQUAL_INT64(0, progression_valuei(s_mixed, 0u));   /* level 0 contributes nothing */
    TEST_ASSERT_EQUAL_INT64(4, progression_valuei_at(s_mixed, 0u, 1));
    TEST_ASSERT_EQUAL_INT64(9, progression_valuei_at(s_mixed, 0u, 2));
    TEST_ASSERT_TRUE(progression_valuef_at(s_mixed, 1u, 0) == 1.0);
    TEST_ASSERT_TRUE(progression_valuef_at(s_mixed, 1u, 2) == 1.5);

    /* A level past the cap reads the cap rather than walking off the table. A
       level is player state and can legitimately be anything; a column index is
       a compile-time name, so an unknown one is a caller bug and traps instead
       of answering. That is why there is no read-a-column-nobody-declared case
       here: the only outcome it has is the abort. */
    TEST_ASSERT_EQUAL_INT64(9, progression_valuei_at(s_mixed, 0u, 99));

    /* An unresolved track still reads as zero -- every other read here does. */
    TEST_ASSERT_EQUAL_INT64(0, progression_valuei(PROGRESSION_TRACK_REF_NONE, 0u));
    TEST_ASSERT_TRUE(progression_valuef(PROGRESSION_TRACK_REF_NONE, 1u) == 0.0);
}

/* ---- a price in more than one resource ---- */

void test_multi_resource_level_up_is_all_or_nothing(void) {
    reset_test_state();

    /* Enough gold, not enough wood: the level must not be granted and neither
       resource may move. */
    TEST_ASSERT_TRUE(resource_add("tmpl.gold", 5, "cheat:test"));
    TEST_ASSERT_TRUE(resource_add("tmpl.wood", 1, "cheat:test"));
    TEST_ASSERT_FALSE(progression_level_up(s_mixed, "level_cost:test"));
    TEST_ASSERT_EQUAL_INT(0, progression_level(s_mixed));
    TEST_ASSERT_EQUAL_INT64(5, resource_count("tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(1, resource_count("tmpl.wood"));

    TEST_ASSERT_TRUE(resource_add("tmpl.wood", 1, "cheat:test"));
    TEST_ASSERT_TRUE(progression_level_up(s_mixed, "level_cost:test"));
    TEST_ASSERT_EQUAL_INT(1, progression_level(s_mixed));
    TEST_ASSERT_EQUAL_INT64(4, resource_count("tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(0, resource_count("tmpl.wood"));
    TEST_ASSERT_EQUAL_UINT32(2u, progression_cost_count(s_mixed));
}

int main(void) {
    game_events_init();
    UNITY_BEGIN();
    RUN_TEST(test_cost_of_the_next_level_follows_the_current_one);
    RUN_TEST(test_unknown_track_reads_as_empty);
    RUN_TEST(test_int64_cost_no_truncation);
    RUN_TEST(test_manual_level_up_spends_purse);
    RUN_TEST(test_manual_level_up_budget_exhausted_leaves_purse_untouched);
    RUN_TEST(test_auto_tick_buys_while_affordable);
    RUN_TEST(test_threshold_tick_buys_from_internal_xp);
    RUN_TEST(test_set_level_clamps_and_leaves_xp_untouched);
    RUN_TEST(test_reset_zeroes_level_and_internal_xp);
    RUN_TEST(test_levelup_events_include_context_for_auto_and_manual);
    RUN_TEST(test_t5_per_track_cap_self_refund_terminates);
    RUN_TEST(test_round_trip_byte_stable);
    RUN_TEST(test_lazy_allocation_no_gratuitous_records);
    RUN_TEST(test_columns_answer_the_read_their_type_declares);
    RUN_TEST(test_multi_resource_level_up_is_all_or_nothing);
    int result = UNITY_END();
    game_events_shutdown();
    return result;
}
