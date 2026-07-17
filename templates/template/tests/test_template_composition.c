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
#include "game_storage.h"
#include "game_state.h"
#include "settings_state.h"
#include "items_state.h"
#include "items_runtime_test_catalog.h"
#include "progression_state.h"
#include "features/settings/settings.h"
#include "features/items/items.h"
#include "features/progression/progression.h"
#include "game_events.h"
#include "game_items.h"
#include "unity.h"
#if NT_DEVAPI_ENABLED
#include "devapi/nt_devapi.h"
#endif
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

static bool add_stack(
    items_container_ref_t container, const char *def_id,
    int64_t count, const char *reason) {
    return items_try_stack_add(
               container, def_id, count, ITEMS_SLOT_AUTO,
               reason, NULL, NULL) == ITEMS_RESULT_OK;
}

static cJSON *required_object(cJSON *parent, const char *key) {
    cJSON *value = cJSON_GetObjectItemCaseSensitive(parent, key);
    TEST_ASSERT_TRUE(cJSON_IsObject(value));
    return value;
}

static char *read_fixture(const char *path) {
    FILE *file = fopen(path, "rb");
    TEST_ASSERT_NOT_NULL(file);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_END));
    long size = ftell(file);
    TEST_ASSERT_TRUE(size >= 0);
    TEST_ASSERT_EQUAL_INT(0, fseek(file, 0, SEEK_SET));
    char *text = (char *)malloc((size_t)size + 1U);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_EQUAL_INT(size, (long)fread(text, 1, (size_t)size, file));
    text[size] = '\0';
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
    return text;
}

static void assert_legacy_rows_rejected_unchanged(const char *rows, const char *expected_error) {
    const size_t capacity = strlen(rows) + 256U;
    char *text = (char *)malloc(capacity);
    TEST_ASSERT_NOT_NULL(text);
    const int written = snprintf(text, capacity,
        "{\"items\":{\"v\":1,\"owned\":{%s}},\"game\":{\"v\":1}}", rows);
    TEST_ASSERT_TRUE(written > 0 && (size_t)written < capacity);
    cJSON *features = cJSON_Parse(text);
    free(text);
    TEST_ASSERT_NOT_NULL(features);
    cJSON *before = cJSON_Duplicate(features, true);
    TEST_ASSERT_NOT_NULL(before);
    char *before_text = cJSON_PrintUnformatted(before);
    TEST_ASSERT_NOT_NULL(before_text);
    char err[128] = {0};

    TEST_ASSERT_FALSE(game_items_migrate_document_v1_to_v2(
        features, err, (int)sizeof err));

    TEST_ASSERT_EQUAL_STRING(expected_error, err);
    char *after_text = cJSON_PrintUnformatted(features);
    TEST_ASSERT_NOT_NULL(after_text);
    TEST_ASSERT_EQUAL_STRING_MESSAGE(before_text, after_text, rows);
    free(after_text);
    free(before_text);
    cJSON_Delete(before);
    cJSON_Delete(features);
}

static void assert_rejected_import_preserves_state(cJSON *doc, const char *before) {
    char err[128] = {0};
    char *invalid = cJSON_PrintUnformatted(doc);
    TEST_ASSERT_NOT_NULL(invalid);
    TEST_ASSERT_FALSE(game_save_import_string(invalid, err, (int)sizeof err));
    TEST_ASSERT_NOT_EQUAL(0, err[0]);
    free(invalid);

    char *after = game_save_export_string(err, (int)sizeof err);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_EQUAL_STRING(before, after);
    free(after);
}

static char *make_dangling_owner_save(char *error, int error_cap) {
    char *valid = game_save_export_string(error, error_cap);
    if (!valid) { return NULL; }
    cJSON *doc = cJSON_Parse(valid);
    free(valid);
    if (!doc) { return NULL; }
    cJSON *features = cJSON_GetObjectItemCaseSensitive(doc, "features");
    cJSON *game = cJSON_GetObjectItemCaseSensitive(features, "game");
    if (!cJSON_IsObject(game) || !cJSON_ReplaceItemInObjectCaseSensitive(
            game, "inventory_container_id", cJSON_CreateNumber(9999))) {
        cJSON_Delete(doc);
        return NULL;
    }
    char *invalid = cJSON_PrintUnformatted(doc);
    cJSON_Delete(doc);
    return invalid;
}

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
#if NT_DEVAPI_ENABLED
    TEST_ASSERT_EQUAL_INT(NT_OK, nt_devapi_init());
    game_save_register_devapi();
#endif
}
void tearDown(void) {
#if NT_DEVAPI_ENABLED
    nt_devapi_shutdown();
#endif
    remove_slot_files();
}

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

    TEST_ASSERT_EQUAL_INT64(50, items_stack_count(game_wallet_container(), "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(1, items_stack_count(game_inventory_container(), "tmpl.potion"));
    TEST_ASSERT_EQUAL_INT(0, progression_level("hero")); /* empty tracks = lazy */
    TEST_ASSERT_TRUE(fabsf(settings_master() - 0.8f) < COMPOSITION_TEST_FLOAT_EPS);
}

static void assert_invalid_seed_plan_preserves_state(
    const game_items_seed_plan_t *plan, const char *expected_error) {
    cJSON *before_items = items_state_to_json(&items_state);
    GameState before_game = game_state;
    game_event_frame_reset();
    char error[128] = {0};

    TEST_ASSERT_FALSE(game_items_test_try_create_defaults_from_plan(
        plan, error, (int)sizeof error));
    TEST_ASSERT_EQUAL_STRING(expected_error, error);

    cJSON *after_items = items_state_to_json(&items_state);
    TEST_ASSERT_TRUE(cJSON_Compare(before_items, after_items, true));
    TEST_ASSERT_EQUAL_MEMORY(&before_game, &game_state, sizeof before_game);
    cJSON_Delete(after_items);
    cJSON_Delete(before_items);
    int event_count = -1;
    (void)game_event_log(&event_count);
    TEST_ASSERT_EQUAL_INT(0, event_count);
}

void test_incompatible_seed_plans_refuse_partial_initialization(void) {
    char error[128] = {0};
    TEST_ASSERT_TRUE(game_items_validate_default_seed(error, (int)sizeof error));

    const game_items_seed_grant_t missing[] = {
        {"missing.seed", 1, GAME_ITEMS_SEED_INVENTORY},
    };
    const game_items_seed_plan_t missing_plan = {64, 32, missing, 1};
    assert_invalid_seed_plan_preserves_state(&missing_plan, "seed item definition is missing");

    const game_items_seed_grant_t wrong_route[] = {
        {"tmpl.potion", 1, GAME_ITEMS_SEED_WALLET},
    };
    const game_items_seed_plan_t wrong_route_plan = {64, 32, wrong_route, 1};
    assert_invalid_seed_plan_preserves_state(&wrong_route_plan, "seed item storage route is incompatible");

    const game_items_seed_grant_t no_capacity[] = {
        {"tmpl.gold", 50, GAME_ITEMS_SEED_WALLET},
    };
    const game_items_seed_plan_t no_capacity_plan = {64, 0, no_capacity, 1};
    assert_invalid_seed_plan_preserves_state(&no_capacity_plan, "seed container capacity is incompatible");

    const game_items_seed_grant_t too_small[] = {
        {"tmpl.potion", 100, GAME_ITEMS_SEED_INVENTORY},
    };
    const game_items_seed_plan_t too_small_plan = {64, 32, too_small, 1};
    assert_invalid_seed_plan_preserves_state(&too_small_plan, "seed minimum amount is incompatible");

    const game_items_seed_grant_t duplicate[] = {
        {"tmpl.potion", 60, GAME_ITEMS_SEED_INVENTORY},
        {"tmpl.potion", 60, GAME_ITEMS_SEED_INVENTORY},
    };
    const game_items_seed_plan_t duplicate_plan = {2, 32, duplicate, 2};
    assert_invalid_seed_plan_preserves_state(&duplicate_plan, "duplicate logical seed grant");
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

    TEST_ASSERT_TRUE(add_stack(game_wallet_container(), "tmpl.gold", 25, "cheat:rt"));
    const int64_t need = progression_xp_needed("hero"); /* curve-agnostic (T5 curve edits safe) */
    TEST_ASSERT_TRUE(add_stack(game_wallet_container(), "tmpl.xp", need, "cheat:rt"));
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

    TEST_ASSERT_EQUAL_UINT32(0, game_state.wallet_container_id);
    TEST_ASSERT_EQUAL_UINT32(0, game_state.inventory_container_id);
    TEST_ASSERT_EQUAL_INT(0, progression_level("hero"));
    TEST_ASSERT_TRUE(fabsf(settings_master() - 0.8f) < COMPOSITION_TEST_FLOAT_EPS); /* reset default */

    TEST_ASSERT_TRUE(game_save_import_string(snap, err, (int)sizeof err));
    free(snap);

    TEST_ASSERT_EQUAL_INT64(75, items_stack_count(game_wallet_container(), "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT(1, progression_level("hero"));
    TEST_ASSERT_TRUE(fabsf(settings_master() - 0.30f) < COMPOSITION_TEST_FLOAT_EPS);
}

void test_import_rejects_dangling_owner_before_publish(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    char *before = game_save_export_string(err, (int)sizeof err);
    TEST_ASSERT_NOT_NULL(before);
    cJSON *doc = cJSON_Parse(before);
    TEST_ASSERT_NOT_NULL(doc);
    cJSON *features = required_object(doc, "features");
    cJSON *game = required_object(features, "game");
    TEST_ASSERT_TRUE(cJSON_ReplaceItemInObjectCaseSensitive(
        game, "inventory_container_id", cJSON_CreateNumber(9999)));

    assert_rejected_import_preserves_state(doc, before);
    cJSON_Delete(doc);
    free(before);
}

void test_import_rejects_unreferenced_persistent_container_before_publish(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    char *before = game_save_export_string(err, (int)sizeof err);
    TEST_ASSERT_NOT_NULL(before);
    cJSON *doc = cJSON_Parse(before);
    TEST_ASSERT_NOT_NULL(doc);
    cJSON *features = required_object(doc, "features");
    cJSON *items = required_object(features, "items");
    cJSON *containers = cJSON_GetObjectItemCaseSensitive(items, "containers");
    TEST_ASSERT_TRUE(cJSON_IsArray(containers));
    cJSON *extra = cJSON_Duplicate(cJSON_GetArrayItem(containers, 0), true);
    TEST_ASSERT_NOT_NULL(extra);
    TEST_ASSERT_TRUE(cJSON_ReplaceItemInObjectCaseSensitive(
        extra, "container_id", cJSON_CreateNumber(3)));
    TEST_ASSERT_TRUE(cJSON_ReplaceItemInObjectCaseSensitive(
        extra, "entries", cJSON_CreateArray()));
    TEST_ASSERT_TRUE(cJSON_AddItemToArray(containers, extra));
    TEST_ASSERT_TRUE(cJSON_ReplaceItemInObjectCaseSensitive(
        items, "last_container_id", cJSON_CreateNumber(3)));

    assert_rejected_import_preserves_state(doc, before);
    cJSON_Delete(doc);
    free(before);
}

void test_import_rejects_invalid_items_graph_before_publish(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    char *before = game_save_export_string(err, (int)sizeof err);
    TEST_ASSERT_NOT_NULL(before);
    cJSON *doc = cJSON_Parse(before);
    TEST_ASSERT_NOT_NULL(doc);
    cJSON *items = required_object(required_object(doc, "features"), "items");
    cJSON *containers = cJSON_GetObjectItemCaseSensitive(items, "containers");
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(containers));
    cJSON *first_entries = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(containers, 0), "entries");
    cJSON *second_entries = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(containers, 1), "entries");
    TEST_ASSERT_TRUE(cJSON_IsArray(first_entries));
    TEST_ASSERT_TRUE(cJSON_IsArray(second_entries));
    cJSON *first_id = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(first_entries, 0), "entry_id");
    TEST_ASSERT_TRUE(cJSON_IsNumber(first_id));
    TEST_ASSERT_TRUE(cJSON_ReplaceItemInObjectCaseSensitive(
        cJSON_GetArrayItem(second_entries, 0), "entry_id",
        cJSON_CreateNumber(first_id->valuedouble)));

    char *invalid = cJSON_PrintUnformatted(doc);
    TEST_ASSERT_NOT_NULL(invalid);
    TEST_ASSERT_FALSE(game_save_import_string(invalid, err, (int)sizeof err));
    TEST_ASSERT_NOT_NULL(strstr(err, "duplicate entry id"));
    free(invalid);
    char *after = game_save_export_string(err, (int)sizeof err);
    TEST_ASSERT_EQUAL_STRING(before, after);
    free(after);
    cJSON_Delete(doc);
    free(before);
}

void test_save_refuses_invalid_live_ownership_without_replacing_disk_state(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    const uint32_t inventory_id = game_state.inventory_container_id;
    game_state.inventory_container_id = 9999;
    game_save_mark_dirty();
    TEST_ASSERT_FALSE(game_save_flush(err, (int)sizeof err));
    TEST_ASSERT_NOT_EQUAL(0, err[0]);

    game_state.inventory_container_id = inventory_id;
    settings_state_fragment.reset();
    items_state_fragment.reset();
    progression_state_fragment.reset();
    game_state_fragment.reset();
    game_save_load_result_t result;
    game_save_load(&result);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_LOADED, result.status);
    TEST_ASSERT_EQUAL_UINT32(inventory_id, game_state.inventory_container_id);
    TEST_ASSERT_EQUAL_INT64(50, items_stack_count(game_wallet_container(), "tmpl.gold"));
}

void test_disk_load_rejects_invalid_primary_and_recovers_valid_backup(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    TEST_ASSERT_TRUE(game_storage_write_backup("test_composition", err, (int)sizeof err));
    char *invalid = make_dangling_owner_save(err, (int)sizeof err);
    TEST_ASSERT_NOT_NULL(invalid);
    TEST_ASSERT_TRUE(game_storage_write("test_composition", invalid, err, (int)sizeof err));
    free(invalid);

    game_save_load_result_t result;
    game_save_load(&result);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_RECOVERED_BAK, result.status);
    TEST_ASSERT_EQUAL_UINT32(1, game_state.inventory_container_id);
    TEST_ASSERT_EQUAL_UINT32(2, game_state.wallet_container_id);
    TEST_ASSERT_EQUAL_INT64(50, items_stack_count(game_wallet_container(), "tmpl.gold"));
}

void test_disk_load_rejects_invalid_primary_and_backup_before_corrupt_reset(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    char *invalid = make_dangling_owner_save(err, (int)sizeof err);
    TEST_ASSERT_NOT_NULL(invalid);
    TEST_ASSERT_TRUE(game_storage_write("test_composition", invalid, err, (int)sizeof err));
    TEST_ASSERT_TRUE(game_storage_write_backup("test_composition", err, (int)sizeof err));
    free(invalid);

    game_save_load_result_t result;
    game_save_load(&result);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_CORRUPT_RESET, result.status);
    TEST_ASSERT_EQUAL_UINT32(0, game_state.inventory_container_id);
    TEST_ASSERT_EQUAL_UINT32(0, game_state.wallet_container_id);
    TEST_ASSERT_EQUAL_UINT32(0, items_state.last_container_id);
}

#if NT_DEVAPI_ENABLED
static bool reject_nonzero_clicks(const cJSON *features, char *error, int error_cap) {
    if (!game_items_validate_save_document(features, error, error_cap)) { return false; }
    const cJSON *game = cJSON_GetObjectItemCaseSensitive(features, "game");
    const cJSON *clicks = cJSON_GetObjectItemCaseSensitive(game, "test_ui_clicks");
    if (cJSON_IsNumber(clicks) && clicks->valueint != 0) {
        (void)snprintf(error, (size_t)error_cap, "%s", "test validator rejected clicks");
        return false;
    }
    return true;
}

static cJSON *submit_devapi(const char *request) {
    const char *response = nt_devapi_submit(request);
    TEST_ASSERT_NOT_NULL(response);
    cJSON *root = cJSON_Parse(response);
    TEST_ASSERT_NOT_NULL(root);
    return root;
}

static int s_partial_value;
static void partial_reset(void) { s_partial_value = 0; }
static cJSON *partial_to_json(void) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "value", s_partial_value);
    return root;
}
static bool partial_from_json(const cJSON *json, char *error, int error_cap) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "value");
    if (!cJSON_IsNumber(value)) {
        (void)snprintf(error, (size_t)error_cap, "%s", "partial value is required");
        return false;
    }
    s_partial_value = value->valueint;
    return true;
}
static bool partial_set_path(
    const char *path, const cJSON *value, char *error, int error_cap) {
    if (strcmp(path, "value") != 0 || !cJSON_IsNumber(value)) { return false; }
    s_partial_value = value->valueint; /* deliberately partial before refusal */
    (void)snprintf(error, (size_t)error_cap, "%s", "forced partial setter failure");
    return false;
}
static const GameSaveFragment s_partial_fragment = {
    .id = "partial",
    .version = 1,
    .reset = partial_reset,
    .to_json = partial_to_json,
    .from_json = partial_from_json,
    .set_path_json = partial_set_path,
};

void test_devapi_refuses_raw_ownership_writes_without_mutation(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    const uint32_t inventory_id = game_state.inventory_container_id;
    const uint32_t wallet_id = game_state.wallet_container_id;
    cJSON *before_items = items_state_to_json(&items_state);

    cJSON *set = submit_devapi(
        "{\"method\":\"game.state.set\",\"params\":{\"path\":\"game.inventory_container_id\",\"value\":9999}}");
    TEST_ASSERT_TRUE(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(set, "ok")));
    cJSON_Delete(set);
    cJSON *patch = submit_devapi(
        "{\"method\":\"game.state.patch\",\"params\":{\"values\":{\"game.wallet_container_id\":9999}}}");
    cJSON *patch_results = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(patch, "result"), "results");
    TEST_ASSERT_TRUE(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(
        patch_results, "game.wallet_container_id")));
    cJSON_Delete(patch);
    cJSON *reset = submit_devapi(
        "{\"method\":\"game.state.set\",\"params\":{\"path\":\"items.containers\",\"value\":[]}}");
    TEST_ASSERT_TRUE(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(reset, "ok")));
    cJSON_Delete(reset);

    TEST_ASSERT_EQUAL_UINT32(inventory_id, game_state.inventory_container_id);
    TEST_ASSERT_EQUAL_UINT32(wallet_id, game_state.wallet_container_id);
    cJSON *after_items = items_state_to_json(&items_state);
    TEST_ASSERT_TRUE(cJSON_Compare(before_items, after_items, true));
    cJSON_Delete(after_items);
    cJSON_Delete(before_items);
}

void test_devapi_rolls_back_all_successful_patch_groups_when_document_rejects(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    const float master_before = settings_master();
    game_save_set_document_validator(reject_nonzero_clicks);
    cJSON *patch = submit_devapi(
        "{\"method\":\"game.state.patch\",\"params\":{\"values\":{"
        "\"settings.master_volume\":0.25,\"game.test_ui_clicks\":1}}}");
    cJSON *results = cJSON_GetObjectItemCaseSensitive(
        cJSON_GetObjectItemCaseSensitive(patch, "result"), "results");
    TEST_ASSERT_TRUE(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(
        results, "settings.master_volume")));
    TEST_ASSERT_TRUE(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(
        results, "game.test_ui_clicks")));
    cJSON_Delete(patch);
    TEST_ASSERT_TRUE(fabsf(settings_master() - master_before) < COMPOSITION_TEST_FLOAT_EPS);
    TEST_ASSERT_EQUAL_INT(0, game_state.test_ui_clicks);
    game_save_set_document_validator(game_items_validate_save_document);
}

void test_devapi_set_rolls_back_a_partially_mutating_setter(void) {
    s_partial_value = 0;
    game_save_register_fragment(&s_partial_fragment);
    cJSON *response = submit_devapi(
        "{\"method\":\"game.state.set\",\"params\":{\"path\":\"partial.value\",\"value\":7}}");
    TEST_ASSERT_TRUE(cJSON_IsFalse(cJSON_GetObjectItemCaseSensitive(response, "ok")));
    cJSON_Delete(response);
    TEST_ASSERT_EQUAL_INT(0, s_partial_value);
}
#endif

/* 4. The live T0327 hygiene mechanic: "Hold to reset progress" wipes
   items+progression back to seed while volumes survive, on the REAL
   4-fragment registry (the review's flagged live mechanic). */
void test_hold_to_reset_preserves_settings(void) {
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_save_new_game(err, (int)sizeof err));
    settings_set_master(0.30f);
    TEST_ASSERT_TRUE(add_stack(game_wallet_container(), "tmpl.gold", 999, "cheat:test"));
    progression_set_level("hero", 3, "test:prologue"); /* deterministic, no xp economics */

    game_save_request_new_game("settings");
    TEST_ASSERT_TRUE(game_save_apply_pending_new_game());

    TEST_ASSERT_TRUE(fabsf(settings_master() - 0.30f) < COMPOSITION_TEST_FLOAT_EPS); /* skipped fragment survived (crown invariant) */
    TEST_ASSERT_EQUAL_INT64(50, items_stack_count(game_wallet_container(), "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(1, items_stack_count(game_inventory_container(), "tmpl.potion"));
    TEST_ASSERT_EQUAL_INT(0, progression_level("hero")); /* reset, no hook -> empty tracks */
}

void test_legacy_items_fixture_migrates_deterministically_with_owner_refs(void) {
    char *fixture = read_fixture(ITEMS_LEGACY_SAVE_V1_FIXTURE);
    cJSON *first = cJSON_Parse(fixture);
    cJSON *second = cJSON_Parse(fixture);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    char err[128] = {0};
    cJSON *first_features = required_object(first, "features");
    cJSON *second_features = required_object(second, "features");
    TEST_ASSERT_TRUE(game_items_migrate_document_v1_to_v2(
        first_features, err, (int)sizeof err));
    TEST_ASSERT_TRUE(game_items_migrate_document_v1_to_v2(
        second_features, err, (int)sizeof err));
    char *first_text = cJSON_PrintUnformatted(first_features);
    char *second_text = cJSON_PrintUnformatted(second_features);
    TEST_ASSERT_NOT_NULL(first_text);
    TEST_ASSERT_NOT_NULL(second_text);
    TEST_ASSERT_EQUAL_STRING(first_text, second_text);

    cJSON *items = required_object(first_features, "items");
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetObjectItemCaseSensitive(items, "last_container_id")->valueint);
    TEST_ASSERT_EQUAL_INT(4, cJSON_GetObjectItemCaseSensitive(items, "last_entry_id")->valueint);
    cJSON *containers = cJSON_GetObjectItemCaseSensitive(items, "containers");
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetArraySize(containers));
    cJSON *inventory = cJSON_GetArrayItem(containers, 0);
    cJSON *wallet = cJSON_GetArrayItem(containers, 1);
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetObjectItemCaseSensitive(inventory, "container_id")->valueint);
    TEST_ASSERT_EQUAL_INT(20, cJSON_GetObjectItemCaseSensitive(inventory, "capacity")->valueint);
    TEST_ASSERT_EQUAL_STRING("generic", cJSON_GetObjectItemCaseSensitive(inventory, "policy")->valuestring);
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetObjectItemCaseSensitive(wallet, "container_id")->valueint);
    TEST_ASSERT_EQUAL_INT(64, cJSON_GetObjectItemCaseSensitive(wallet, "capacity")->valueint);
    TEST_ASSERT_EQUAL_STRING("currency_only", cJSON_GetObjectItemCaseSensitive(wallet, "policy")->valuestring);
    cJSON *inventory_entries = cJSON_GetObjectItemCaseSensitive(inventory, "entries");
    cJSON *wallet_entries = cJSON_GetObjectItemCaseSensitive(wallet, "entries");
    TEST_ASSERT_EQUAL_INT(3, cJSON_GetArraySize(inventory_entries));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetArraySize(wallet_entries));
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(inventory_entries, 0), "entry_id")->valueint);
    TEST_ASSERT_EQUAL_STRING("removed.relic", cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(inventory_entries, 0), "def_id")->valuestring);
    TEST_ASSERT_TRUE(cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(inventory_entries, 0), "quarantined")));
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(inventory_entries, 1), "entry_id")->valueint);
    TEST_ASSERT_EQUAL_INT(4, cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(inventory_entries, 2), "entry_id")->valueint);
    TEST_ASSERT_EQUAL_INT(3, cJSON_GetObjectItemCaseSensitive(
        cJSON_GetArrayItem(wallet_entries, 0), "entry_id")->valueint);
    for (int i = 0; i < cJSON_GetArraySize(inventory_entries); i++) {
        TEST_ASSERT_NOT_EQUAL(0, strcmp("tmpl.wood", cJSON_GetObjectItemCaseSensitive(
            cJSON_GetArrayItem(inventory_entries, i), "def_id")->valuestring));
    }
    cJSON *game = required_object(first_features, "game");
    TEST_ASSERT_EQUAL_INT(1, cJSON_GetObjectItemCaseSensitive(
        game, "inventory_container_id")->valueint);
    TEST_ASSERT_EQUAL_INT(2, cJSON_GetObjectItemCaseSensitive(
        game, "wallet_container_id")->valueint);

    free(second_text);
    free(first_text);
    cJSON_Delete(second);
    cJSON_Delete(first);
    free(fixture);
}

void test_legacy_items_fixture_imports_through_document_stage(void) {
    char *fixture = read_fixture(ITEMS_LEGACY_SAVE_V1_FIXTURE);
    char err[128] = {0};

    TEST_ASSERT_TRUE(game_save_import_string(fixture, err, (int)sizeof err));

    TEST_ASSERT_EQUAL_UINT32(1, game_state.inventory_container_id);
    TEST_ASSERT_EQUAL_UINT32(2, game_state.wallet_container_id);
    TEST_ASSERT_EQUAL_UINT32(2, items_state.last_container_id);
    TEST_ASSERT_EQUAL_UINT32(4, items_state.last_entry_id);
    TEST_ASSERT_EQUAL_INT64(50, items_stack_count(game_wallet_container(), "tmpl.gold"));
    TEST_ASSERT_EQUAL_INT64(3, items_stack_count(game_inventory_container(), "tmpl.potion"));
    bool found_removed = false;
    for (int i = 0; i < ITEMS_STATE_MAX_CONTAINERS_ENTRIES; i++) {
        if (items_state.containers_entries[i].used &&
            strcmp(items_state.containers_entries[i].def_id, "removed.relic") == 0) {
            found_removed = true;
            TEST_ASSERT_TRUE(items_state.containers_entries[i].quarantined);
            TEST_ASSERT_EQUAL_INT64(2, items_state.containers_entries[i].count);
            TEST_ASSERT_EQUAL_INT(4, items_state.containers_entries[i].level);
            TEST_ASSERT_TRUE(fabsf(items_state.containers_entries[i].durability - 0.5f) <
                             COMPOSITION_TEST_FLOAT_EPS);
        }
    }
    TEST_ASSERT_TRUE(found_removed);
    free(fixture);
}

void test_legacy_items_malformed_keys_routes_and_counts_reject_unchanged(void) {
    typedef struct legacy_rejection_case {
        const char *rows;
        const char *error;
    } legacy_rejection_case_t;
    static const legacy_rejection_case_t cases[] = {
        /* overflowing and noncanonical unique sequences */
        {"\"tmpl.sword#9223372036854775808\":{\"def_id\":\"tmpl.sword\",\"container\":\"backpack\",\"count\":\"1\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "legacy unique sequence overflow"},
        {"\"tmpl.sword#01\":{\"def_id\":\"tmpl.sword\",\"container\":\"backpack\",\"count\":\"1\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "legacy unique key does not match definition"},
        /* overlong and canonical-looking truncated legacy map keys */
        {"\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\":{\"def_id\":\"tmpl.gold\",\"container\":\"purse\",\"count\":\"1\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "invalid legacy owned key"},
        {"\"purse/tmpl.gol\":{\"def_id\":\"tmpl.gold\",\"container\":\"purse\",\"count\":\"1\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "legacy stack key does not match fields"},
        /* key/container mismatch */
        {"\"backpack/tmpl.gold\":{\"def_id\":\"tmpl.gold\",\"container\":\"purse\",\"count\":\"1\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "legacy stack key does not match fields"},
        /* key/definition mismatch */
        {"\"purse/tmpl.gold\":{\"def_id\":\"tmpl.xp\",\"container\":\"purse\",\"count\":\"1\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "legacy stack key does not match fields"},
        /* duplicate logical stack keys */
        {"\"purse/tmpl.gold\":{\"def_id\":\"tmpl.gold\",\"container\":\"purse\",\"count\":\"1\",\"level\":1,\"durability\":1,\"quarantined\":false},"
         "\"purse/tmpl.gold\":{\"def_id\":\"tmpl.gold\",\"container\":\"purse\",\"count\":\"2\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "duplicate legacy owned key"},
        /* unique count other than one */
        {"\"tmpl.sword#2\":{\"def_id\":\"tmpl.sword\",\"container\":\"backpack\",\"count\":\"2\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "legacy unique count must be one"},
        /* unsupported container and storage routes */
        {"\"vault/tmpl.gold\":{\"def_id\":\"tmpl.gold\",\"container\":\"vault\",\"count\":\"1\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "unsupported legacy container route"},
        {"\"backpack/tmpl.sword\":{\"def_id\":\"tmpl.sword\",\"container\":\"backpack\",\"count\":\"1\",\"level\":1,\"durability\":1,\"quarantined\":false}",
         "legacy key conflicts with frozen storage"},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        assert_legacy_rows_rejected_unchanged(cases[i].rows, cases[i].error);
    }
}

void test_legacy_owner_conflict_rejects_frozen_fixture_exactly_unchanged(void) {
    char *fixture = read_fixture(ITEMS_LEGACY_SAVE_V1_FIXTURE);
    cJSON *document = cJSON_Parse(fixture);
    free(fixture);
    TEST_ASSERT_NOT_NULL(document);
    cJSON *features = required_object(document, "features");
    cJSON *game = required_object(features, "game");
    TEST_ASSERT_NOT_NULL(cJSON_AddNumberToObject(game, "inventory_container_id", 99));
    char *before = cJSON_PrintUnformatted(features);
    TEST_ASSERT_NOT_NULL(before);
    char err[128] = {0};

    TEST_ASSERT_FALSE(game_items_migrate_document_v1_to_v2(
        features, err, (int)sizeof err));

    TEST_ASSERT_EQUAL_STRING("legacy owner reference conflicts with frozen mapping", err);
    char *after = cJSON_PrintUnformatted(features);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_EQUAL_STRING(before, after);
    free(after);
    free(before);
    cJSON_Delete(document);
}

int main(void) {
    if (!items_runtime_test_catalog_bind()) {
        return 1;
    }
    /* registration ONCE (no unregister API; registering per-setUp would
       duplicate/overflow), in the documented order (settings -> items ->
       progression -> game; `game` last). */
    game_events_init();
    game_save_register_fragment(&settings_state_fragment);
    game_save_register_fragment(&items_state_fragment);
    game_save_register_fragment(&progression_state_fragment);
    game_save_register_fragment(&game_state_fragment);
    game_items_configure_save();

    UNITY_BEGIN();
    RUN_TEST(test_registry_has_four_fragments_in_order);
    RUN_TEST(test_new_game_seeds_across_all_fragments);
    RUN_TEST(test_incompatible_seed_plans_refuse_partial_initialization);
    RUN_TEST(test_cross_fragment_save_load_roundtrip);
    RUN_TEST(test_import_rejects_dangling_owner_before_publish);
    RUN_TEST(test_import_rejects_unreferenced_persistent_container_before_publish);
    RUN_TEST(test_import_rejects_invalid_items_graph_before_publish);
    RUN_TEST(test_save_refuses_invalid_live_ownership_without_replacing_disk_state);
    RUN_TEST(test_disk_load_rejects_invalid_primary_and_recovers_valid_backup);
    RUN_TEST(test_disk_load_rejects_invalid_primary_and_backup_before_corrupt_reset);
#if NT_DEVAPI_ENABLED
    RUN_TEST(test_devapi_refuses_raw_ownership_writes_without_mutation);
    RUN_TEST(test_devapi_rolls_back_all_successful_patch_groups_when_document_rejects);
#endif
    RUN_TEST(test_hold_to_reset_preserves_settings);
    RUN_TEST(test_legacy_items_fixture_migrates_deterministically_with_owner_refs);
    RUN_TEST(test_legacy_items_fixture_imports_through_document_stage);
    RUN_TEST(test_legacy_items_malformed_keys_routes_and_counts_reject_unchanged);
    RUN_TEST(test_legacy_owner_conflict_rejects_frozen_fixture_exactly_unchanged);
#if NT_DEVAPI_ENABLED
    RUN_TEST(test_devapi_set_rolls_back_a_partially_mutating_setter);
#endif
    const int r = UNITY_END();

    game_events_shutdown();
    items_catalog_shutdown();
    return r;
}
