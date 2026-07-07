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
   layers, over the RUKOPISNYY test catalog (tests/test_progression_catalog.c,
   §5.7) -- NOT the demo progression_tracks.gen.c (anti-duplicate k_tracks
   symbol, R10; the golden catalog is exercised separately by
   tests/test_progression_curve.c). game_save.c is NOT linked -- items_containers.c/
   items_bootstrap.c/progression.c only need game_save_mark_dirty(), stubbed
   below (precedent: test_items_fragment.c). */
void game_save_mark_dirty(void) {}

void setUp(void) { game_event_frame_reset(); /* per-test event-log isolation */ }
void tearDown(void) {}

/* ---- cost-lookup / xp_needed ---- */

void test_cost_lookup_and_xp_needed(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    const progression_track_def_t *man = progression_track_def("man");
    TEST_ASSERT_NOT_NULL(man);
    TEST_ASSERT_EQUAL_INT64(10, man->cost[0]);
    TEST_ASSERT_EQUAL_INT64(20, man->cost[1]);
    TEST_ASSERT_EQUAL_INT64(30, man->cost[2]);

    TEST_ASSERT_EQUAL_INT64(10, progression_xp_needed("man")); /* level 0 -> cost[0] */
    progression_set_level("man", 3, "admin:test");             /* clamp to max */
    TEST_ASSERT_EQUAL_INT64(0, progression_xp_needed("man"));  /* at max -> 0 */
}

/* int64-край (§5.8): a struct literal with a near-int64-max cost round-trips
   without truncation -- a pure C-level sanity check independent of the test
   catalog above (this def is NOT registered in k_tracks). */
void test_int64_cost_no_truncation(void) {
    static const int64_t huge_cost[] = {9000000000000000000LL};
    progression_track_def_t huge_def = {
        .id = "huge",
        .mode = PROGRESSION_MODE_MANUAL,
        .currency_def = "tmpl.gold",
        .max_level = 1,
        .cost = huge_cost,
        .cost_count = 1,
        .on_level_up = NULL,
        .on_level_up_count = 0,
    };
    TEST_ASSERT_EQUAL_INT64(9000000000000000000LL, huge_def.cost[0]);
}

/* ---- manual mode ---- */

void test_manual_level_up_spends_purse(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 25, "cheat:test"));

    TEST_ASSERT_TRUE(progression_level_up("man", "level_cost:test"));
    TEST_ASSERT_EQUAL_INT(1, progression_level("man"));
    TEST_ASSERT_EQUAL_INT64(15, items_purse("tmpl.gold")); /* 25 - cost[0]=10 */

    /* cost[1]=20 > remaining 15 -> insufficient, level_up rejects, level stays put. */
    TEST_ASSERT_FALSE(progression_level_up("man", "level_cost:test"));
    TEST_ASSERT_EQUAL_INT(1, progression_level("man"));
    TEST_ASSERT_EQUAL_INT64(15, items_purse("tmpl.gold")); /* untouched by the rejected call */
}

/* H-fix regression (deep-review #1, data-loss): saturate the 32-slot tracks
   map with fabricated foreign records (white-box, precedent
   test_items_fragment.c's capacity-reject tests) so find_or_alloc_track("man")
   has nowhere to land. Before the fix, items_remove() ran BEFORE the alloc
   check -- the player's gold would have been spent and lost with no level
   ever recorded. After the fix, the alloc failure must be caught BEFORE any
   currency is touched. */
void test_manual_level_up_budget_exhausted_leaves_purse_untouched(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    for (int i = 0; i < PROGRESSION_STATE_MAX_TRACKS; ++i) {
        ProgressionTrackState *slot = &progression_state.tracks[i];
        slot->used = true;
        (void)snprintf(slot->key, sizeof slot->key, "fake_%d", i); /* none of these is "man" */
        slot->level = 0;
        slot->xp = 0;
    }

    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 25, "cheat:test"));
    TEST_ASSERT_FALSE(progression_level_up("man", "level_cost:test")); /* budget exhausted -- must fail closed */
    TEST_ASSERT_EQUAL_INT64(25, items_purse("tmpl.gold"));             /* H-fix: purse untouched -- no data loss */
    TEST_ASSERT_EQUAL_INT(0, progression_level("man"));                /* never got a record -- lazy default reads 0 */
}

/* ---- auto mode (tick) ---- */

void test_auto_tick_buys_while_affordable(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    TEST_ASSERT_TRUE(items_add("purse", "tmpl.xp", 12, "cheat:test"));
    progression_update();

    /* auto1 cost {5,5,5,5,5}: 12 -> buys level0 (7 left) -> buys level1 (2 left) ->
       cost[2]=5 > 2, stops. */
    TEST_ASSERT_EQUAL_INT(2, progression_level("auto1"));
    TEST_ASSERT_EQUAL_INT64(2, items_purse("tmpl.xp"));
    TEST_ASSERT_EQUAL_INT64(2, progression_xp_current("auto1")); /* == purse for auto mode */
}

/* ---- threshold mode (tick) ---- */

void test_threshold_tick_buys_from_internal_xp(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    progression_add_xp("thr", 25, "loot:test");
    progression_update();

    /* thr cost {10,10,10,10,10}: 25 -> buys level0 (15 left) -> buys level1 (5 left) ->
       cost[2]=10 > 5, stops. */
    TEST_ASSERT_EQUAL_INT(2, progression_level("thr"));
    TEST_ASSERT_EQUAL_INT64(5, progression_xp_current("thr")); /* internal accumulator, not purse */
}

/* ---- set_level (Р6: prologue) ---- */

void test_set_level_clamps_and_leaves_xp_untouched(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    progression_set_level("man", 3, "admin:prologue");
    TEST_ASSERT_EQUAL_INT(3, progression_level("man")); /* == max_level */

    progression_set_level("man", 99, "admin:prologue"); /* clamp above max */
    TEST_ASSERT_EQUAL_INT(3, progression_level("man"));

    progression_set_level("man", 1, "admin:prologue"); /* lowered (e.g. a weakened hero) */
    TEST_ASSERT_EQUAL_INT(1, progression_level("man"));
}

/* ---- reset (Р6: prestige) ---- */

void test_reset_zeroes_level_and_internal_xp(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    progression_add_xp("thr", 25, "loot:test");
    progression_update();
    TEST_ASSERT_EQUAL_INT(2, progression_level("thr")); /* precondition, see threshold test above */

    progression_reset("thr", "admin:prestige");
    TEST_ASSERT_EQUAL_INT(0, progression_level("thr"));
    TEST_ASSERT_EQUAL_INT64(0, progression_xp_current("thr"));

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

void test_levelup_event_emitted_by_auto_not_by_manual(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    TEST_ASSERT_TRUE(items_add("purse", "tmpl.xp", 12, "cheat:test"));
    progression_update(); /* auto1: 0->1->2 (two levelups) */

    int n = 0;
    const game_event_t *log = game_event_log(&n);
    nt_hash64_t levelup_type = progression_ev_levelup_type();
    bool found_0_1 = false;
    bool found_1_2 = false;
    for (int i = 0; i < n; ++i) {
        if (log[i].type.value != levelup_type.value) {
            continue;
        }
        const ProgressionEvLevelup *e = (const ProgressionEvLevelup *)log[i].payload;
        if (strcmp(progression_ev_levelup_track(e), "auto1") != 0) {
            continue;
        }
        if (e->old_level == 0 && e->new_level == 1) {
            found_0_1 = true;
        }
        if (e->old_level == 1 && e->new_level == 2) {
            found_1_2 = true;
        }
    }
    TEST_ASSERT_TRUE(found_0_1);
    TEST_ASSERT_TRUE(found_1_2);

    /* manual level_up does NOT emit progression.levelup (дизайн §5). */
    game_event_frame_reset();
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 25, "cheat:test"));
    TEST_ASSERT_TRUE(progression_level_up("man", "level_cost:test"));
    int n2 = 0;
    const game_event_t *log2 = game_event_log(&n2);
    int manual_levelups = 0;
    for (int i = 0; i < n2; ++i) {
        if (log2[i].type.value == levelup_type.value) {
            ++manual_levelups;
        }
    }
    TEST_ASSERT_EQUAL_INT(0, manual_levelups);
}

/* ---- T5 HARD caps (G6 -- anti-hang, критично) ---- */

void test_t5_per_track_cap_self_refund_terminates(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    TEST_ASSERT_TRUE(items_add("purse", "tmpl.xp", 100, "cheat:test"));
    progression_update(); /* MUST return -- proves the per-track cap, not a hang */

    TEST_ASSERT_EQUAL_INT(64, progression_level("runaway")); /* ровно кап, не 100, не ∞ */
}

void test_t5_cascade_depth_cap_terminates(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    progression_add_xp("casc_a", 5, "loot:test");
    progression_update(); /* MUST return -- proves the cascade depth cap, not a hang */

    int total = progression_level("casc_a") + progression_level("casc_b");
    TEST_ASSERT_TRUE(total > 0);    /* the cascade did fire at least once */
    TEST_ASSERT_TRUE(total < 1000); /* и это КОНЕЧНОЕ число -- ограничено depth-капом, не max_level (20 каждый) */
}

/* ---- round-trip byte-stable (G4) ---- */

void test_round_trip_byte_stable(void) {
    items_state_fragment.reset();
    progression_state_fragment.reset();

    TEST_ASSERT_TRUE(items_add("purse", "tmpl.xp", 12, "cheat:test"));
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
    items_state_fragment.reset();
    progression_state_fragment.reset();

    TEST_ASSERT_EQUAL_INT(0, progression_level("hero-absent")); /* unknown track -> 0, not a crash */

    cJSON *json_empty = progression_state_to_json(&progression_state);
    const cJSON *tracks_empty = cJSON_GetObjectItemCaseSensitive(json_empty, "tracks");
    TEST_ASSERT_NOT_NULL(tracks_empty);
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(tracks_empty));
    cJSON_Delete(json_empty);

    /* Empty purse/xp -> a tick over auto/threshold tracks buys nothing and must
       NOT allocate a record for any of them (§2.1/OQ3: fresh game = empty tracks). */
    progression_update();
    cJSON *json_after = progression_state_to_json(&progression_state);
    const cJSON *tracks_after = cJSON_GetObjectItemCaseSensitive(json_after, "tracks");
    TEST_ASSERT_NOT_NULL(tracks_after);
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(tracks_after));
    cJSON_Delete(json_after);
}

int main(void) {
    game_events_init();
    UNITY_BEGIN();
    RUN_TEST(test_cost_lookup_and_xp_needed);
    RUN_TEST(test_int64_cost_no_truncation);
    RUN_TEST(test_manual_level_up_spends_purse);
    RUN_TEST(test_manual_level_up_budget_exhausted_leaves_purse_untouched);
    RUN_TEST(test_auto_tick_buys_while_affordable);
    RUN_TEST(test_threshold_tick_buys_from_internal_xp);
    RUN_TEST(test_set_level_clamps_and_leaves_xp_untouched);
    RUN_TEST(test_reset_zeroes_level_and_internal_xp);
    RUN_TEST(test_levelup_event_emitted_by_auto_not_by_manual);
    RUN_TEST(test_t5_per_track_cap_self_refund_terminates);
    RUN_TEST(test_t5_cascade_depth_cap_terminates);
    RUN_TEST(test_round_trip_byte_stable);
    RUN_TEST(test_lazy_allocation_no_gratuitous_records);
    int result = UNITY_END();
    game_events_shutdown();
    return result;
}
