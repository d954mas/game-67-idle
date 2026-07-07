/* T0327 tail: the ONE integration test that lifts all 4 REAL fragments
   (settings/items/progression/game) through the REAL game_save registry --
   the existing per-fragment tests (test_items_fragment, test_progression*,
   test_game_save's FAKE fragment) never assemble the composed registry.
   System headers before Unity to avoid noreturn / __declspec conflict on
   MSVC (unity_internals.h pulls in <stdnoreturn.h>, precedent test_game_save.c). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* clang-format off */
#include "game_save.h"
#include "game_state.h"
#include "settings_state.h"
#include "items_state.h"
#include "progression_state.h"
#include "features/settings/settings.h"
#include "features/items/items.h"
#include "features/progression/progression.h"
#include "game_events.h"
#include "unity.h"
/* clang-format on */

/* Unity is built with UNITY_EXCLUDE_FLOAT (engine deps/unity CMakeLists, cf.
   test_items_fragment.c) -- no TEST_ASSERT_EQUAL_FLOAT; compare with fabsf + epsilon. */
#define COMPOSITION_TEST_FLOAT_EPS 0.0001f

#define PRIMARY_PATH "build/saves/test_composition.json"
#define PRIMARY_TMP "build/saves/test_composition.json.tmp"
#define BAK_PATH "build/saves/test_composition.bak"
#define BAK_TMP "build/saves/test_composition.bak.tmp"

/* ---- injected clocks (GAME_SAVE_TESTING) ---- */
static int64_t g_mono_ms;
static int64_t g_wall_ms;
static int64_t test_mono(void) { return g_mono_ms; }
static int64_t test_wall(void) { return g_wall_ms; }

static void remove_slot_files(void) {
    (void)remove(PRIMARY_PATH);
    (void)remove(PRIMARY_TMP);
    (void)remove(BAK_PATH);
    (void)remove(BAK_TMP);
}

/* Fresh deterministic slot every test; distinct slot name (not "test_slot")
   so it cannot collide with test_game_save under parallel ctest. */
void setUp(void) {
    remove_slot_files();
    game_event_frame_reset(); /* per-test event-log isolation */
    g_mono_ms = 5000000;
    g_wall_ms = 1720080000000LL;
    game_save__set_clocks_for_test(test_mono, test_wall);
    game_save_set_transforms(NULL, 0);
    game_save_init();
}
void tearDown(void) { remove_slot_files(); }

/* 1. Locks the exact registration contract main.c depends on: order matters
   for reconcile/on_new_game fan-out and the L1(items)-before-L2(progression) rule.
   A new template fragment updates this assertion together with its own
   registration -- an intentional composition contract, not brittleness. */
void test_registry_has_four_fragments_in_order(void) {
    TEST_ASSERT_EQUAL_INT(4, game_save_fragment_count());
    TEST_ASSERT_EQUAL_STRING("settings", game_save_fragment_at(0)->id);
    TEST_ASSERT_EQUAL_STRING("items", game_save_fragment_at(1)->id);
    TEST_ASSERT_EQUAL_STRING("progression", game_save_fragment_at(2)->id);
    TEST_ASSERT_EQUAL_STRING("game", game_save_fragment_at(3)->id);
}

/* 2. Proves the orchestrator's on_new_game fan-out composes the documented
   starting state (50 gold + 1 potion + empty hero track + default volumes) --
   the single thing the T0327 review flagged as untested. */
void test_new_game_seeds_across_all_fragments(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));

    TEST_ASSERT_EQUAL_INT64(50, items_count("purse", "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(1, items_count("backpack", "tmpl.potion"));
    TEST_ASSERT_EQUAL_INT(0, progression_level("hero")); /* empty tracks = lazy */
    TEST_ASSERT_TRUE(fabsf(settings_master() - 0.8f) < COMPOSITION_TEST_FLOAT_EPS);
}

/* 3. Proves ONE envelope carries cross-fragment state through
   build_root -> load_from_doc (incl. reconcile_all) -- the composed save/load
   nobody else exercises. Uses the items -> progression edge (purse xp spend on
   auto-mode level-up) so the L2 -> L1 dependency is proven end-to-end through
   the composed registry. export/import (no disk) keeps it deterministic;
   the disk path is exercised by cases 2 and 4 (new_game / apply). */
void test_cross_fragment_save_load_roundtrip(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err)); /* baseline: 50 gold */

    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 25, "cheat:rt")); /* gold -> 75; "cheat" is items' closed reason-verb (reason_tags.h), not free text */
    const int64_t need = progression_xp_needed("hero"); /* curve-agnostic (T5 curve edits safe) */
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.xp", need, "cheat:rt"));
    progression_update(); /* auto-mode consumes xp -> level 1 */
    TEST_ASSERT_EQUAL_INT(1, progression_level("hero"));
    settings_set_master(0.30f);

    char *snap = game_save_export_string(err, (int)sizeof err); /* in-memory envelope */
    TEST_ASSERT_NOT_NULL(snap);

    /* scramble every fragment back to its NEUTRAL reset() default (not on_new_game) */
    settings_state_fragment.reset();
    items_state_fragment.reset();
    progression_state_fragment.reset();
    game_state_fragment.reset();

    TEST_ASSERT_EQUAL_INT64(0, items_count("purse", "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT(0, progression_level("hero"));
    TEST_ASSERT_TRUE(fabsf(settings_master() - 0.8f) < COMPOSITION_TEST_FLOAT_EPS); /* reset default */

    TEST_ASSERT_TRUE(game_save_import_string(snap, err, (int)sizeof err));
    free(snap);

    TEST_ASSERT_EQUAL_INT64(75, items_count("purse", "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT(1, progression_level("hero"));
    TEST_ASSERT_TRUE(fabsf(settings_master() - 0.30f) < COMPOSITION_TEST_FLOAT_EPS);
}

/* 4. The live T0327 hygiene mechanic: "Hold to reset progress" wipes
   items+progression back to seed while volumes survive, on the REAL
   4-fragment registry (the review's flagged live mechanic). */
void test_hold_to_reset_preserves_settings(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    settings_set_master(0.30f);
    TEST_ASSERT_TRUE(items_add("purse", "tmpl.gold", 999, "cheat:test")); /* "cheat" is items' closed reason-verb */
    progression_set_level("hero", 3, "test:prologue"); /* deterministic, no xp economics */

    game_save_request_new_game("settings");
    TEST_ASSERT_TRUE(game_save_apply_pending_new_game());

    TEST_ASSERT_TRUE(fabsf(settings_master() - 0.30f) < COMPOSITION_TEST_FLOAT_EPS); /* skipped fragment survived (crown invariant) */
    TEST_ASSERT_EQUAL_INT64(50, items_count("purse", "tmpl.gold"));  /* reset + on_new_game re-seeded */
    TEST_ASSERT_EQUAL_INT64(1, items_count("backpack", "tmpl.potion"));
    TEST_ASSERT_EQUAL_INT(0, progression_level("hero")); /* reset, no hook -> empty tracks */
}

int main(void) {
    /* registration ONCE (no unregister API; registering per-setUp would
       duplicate/overflow), in the documented order (settings -> items ->
       progression -> game; `game` last, §14 п.2). */
    game_events_init();
    game_save_register_fragment(&settings_state_fragment);
    game_save_register_fragment(&items_state_fragment);
    game_save_register_fragment(&progression_state_fragment);
    game_save_register_fragment(&game_state_fragment);

    UNITY_BEGIN();
    RUN_TEST(test_registry_has_four_fragments_in_order);
    RUN_TEST(test_new_game_seeds_across_all_fragments);
    RUN_TEST(test_cross_fragment_save_load_roundtrip);
    RUN_TEST(test_hold_to_reset_preserves_settings);
    const int r = UNITY_END();

    game_events_shutdown();
    return r;
}
