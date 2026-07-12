#include "unity.h"

#include "game_state.h"
#include "game_state_migrations.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* A4 round-trip gate for the generated fragment state layer.
   Links the generated game_state.c + the gsj_* toolkit; game_save is NOT linked
   (the descriptor is data + static wrappers, no game_save_* calls). Unity is
   built with UNITY_EXCLUDE_FLOAT/DOUBLE, so no float/double asserts here. */

void setUp(void) {}
void tearDown(void) {}

/* init_defaults -> to_json -> from_json (other instance) -> to_json: equal. */
void test_defaults_round_trip(void) {
    GameState a;
    game_state_init_defaults(&a);
    cJSON *ja = game_state_to_json(&a);
    TEST_ASSERT_NOT_NULL(ja);
    char *sa = cJSON_PrintUnformatted(ja);
    TEST_ASSERT_NOT_NULL(sa);

    GameState b;
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_state_from_json(&b, ja, err, (int)sizeof(err)));
    cJSON *jb = game_state_to_json(&b);
    char *sb = cJSON_PrintUnformatted(jb);
    TEST_ASSERT_NOT_NULL(sb);

    TEST_ASSERT_EQUAL_STRING(sa, sb);

    cJSON_free(sa);
    cJSON_free(sb);
    cJSON_Delete(ja);
    cJSON_Delete(jb);
}

/* Tolerant from_json: empty object -> defaults; unknown key ignored, others read. */
void test_from_json_tolerant(void) {
    char err[128] = {0};

    cJSON *empty = cJSON_CreateObject();
    GameState a;
    TEST_ASSERT_TRUE(game_state_from_json(&a, empty, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("none", a.outfit_hair_id); /* stable empty-slot sentinel */
    cJSON_Delete(empty);

    cJSON *partial = cJSON_CreateObject();
    cJSON_AddStringToObject(partial, "totally_unknown_key", "ignored");
    cJSON_AddStringToObject(partial, "outfit_hair_id", "hair_bob");
    GameState b;
    TEST_ASSERT_TRUE(game_state_from_json(&b, partial, err, (int)sizeof(err)));
    TEST_ASSERT_EQUAL_STRING("hair_bob", b.outfit_hair_id); /* read despite unknown key */
    cJSON_Delete(partial);
}

/* Gameplay progress fields use bounded integers and stable outfit ids. */
void test_get_set_path_int(void) {
    GameState a;
    game_state_init_defaults(&a);
    char err[128] = {0};

    cJSON *got = game_state_get_path_json(&a, "rounds_completed", err, (int)sizeof(err));
    TEST_ASSERT_NOT_NULL(got);
    TEST_ASSERT_TRUE(cJSON_IsNumber(got));
    TEST_ASSERT_EQUAL_INT(0, (int)got->valuedouble);
    cJSON_Delete(got);

    cJSON *num_val = cJSON_CreateNumber(42);
    TEST_ASSERT_TRUE(game_state_set_path_json(&a, "rounds_completed", num_val, err, (int)sizeof(err)));
    cJSON_Delete(num_val);
    TEST_ASSERT_EQUAL_INT(42, a.rounds_completed);

    cJSON *got2 = game_state_get_path_json(&a, "rounds_completed", err, (int)sizeof(err));
    TEST_ASSERT_NOT_NULL(got2);
    TEST_ASSERT_EQUAL_INT(42, (int)got2->valuedouble);
    cJSON_Delete(got2);

    /* out of range is rejected */
    cJSON *bad_val = cJSON_CreateNumber(-1);
    TEST_ASSERT_FALSE(game_state_set_path_json(&a, "rounds_completed", bad_val, err, (int)sizeof(err)));
    cJSON_Delete(bad_val);
}

void test_awakening_progress_round_trip(void) {
    GameState a;
    game_state_init_defaults(&a);
    strcpy(a.outfit_hair_id, "hair_pink");
    strcpy(a.outfit_main_id, "top_crop");
    strcpy(a.outfit_bottom_id, "bot_skirt");
    strcpy(a.outfit_shoes_id, "shoe_heel");
    strcpy(a.outfit_accent_id, "acc_hat");
    a.recipe_mask = 0x2b;
    a.lookbook_mask = 0x2a155;
    a.essence_mask = 0x07;
    a.rounds_completed = 17;
    a.first_equip_done = true;

    cJSON *json = game_state_to_json(&a);
    TEST_ASSERT_NOT_NULL(json);
    GameState b;
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_state_from_json(&b, json, err, (int)sizeof err));
    TEST_ASSERT_EQUAL_STRING("hair_pink", b.outfit_hair_id);
    TEST_ASSERT_EQUAL_STRING("top_crop", b.outfit_main_id);
    TEST_ASSERT_EQUAL_STRING("bot_skirt", b.outfit_bottom_id);
    TEST_ASSERT_EQUAL_STRING("shoe_heel", b.outfit_shoes_id);
    TEST_ASSERT_EQUAL_STRING("acc_hat", b.outfit_accent_id);
    TEST_ASSERT_EQUAL_INT(0x2b, b.recipe_mask);
    TEST_ASSERT_EQUAL_INT(0x2a155, b.lookbook_mask);
    TEST_ASSERT_EQUAL_INT(0x07, b.essence_mask);
    TEST_ASSERT_EQUAL_INT(17, b.rounds_completed);
    TEST_ASSERT_TRUE(b.first_equip_done);
    cJSON_Delete(json);
}

void test_eighteen_saved_looks_round_trip_at_capacity(void) {
    GameState a;
    game_state_init_defaults(&a);
    for (int i = 0; i < 18; ++i) {
        GameSavedLook *look = &a.saved_looks[i];
        look->used = true;
        snprintf(look->key, sizeof look->key, "recipe-%d/%d", i / 3, i % 3);
        strcpy(look->hair_id, "hair_bob");
        strcpy(look->main_id, "top_tee");
        strcpy(look->bottom_id, "bot_jeans");
        strcpy(look->shoes_id, "shoe_sneak");
        strcpy(look->accent_id, "acc_glasses");
    }
    cJSON *json = game_state_to_json(&a);
    TEST_ASSERT_NOT_NULL(json);
    GameState b;
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_state_from_json(&b, json, err, (int)sizeof err));
    for (int i = 0; i < 18; ++i) {
        TEST_ASSERT_TRUE(b.saved_looks[i].used);
        TEST_ASSERT_EQUAL_STRING(a.saved_looks[i].key, b.saved_looks[i].key);
        TEST_ASSERT_EQUAL_STRING("shoe_sneak", b.saved_looks[i].shoes_id);
    }
    cJSON_Delete(json);
}

void test_v1_to_v2_migration_preserves_progress_and_adds_empty_saved_looks(void) {
    cJSON *v1 = cJSON_CreateObject();
    cJSON_AddNumberToObject(v1, "recipe_mask", 43);
    cJSON_AddNumberToObject(v1, "lookbook_mask", 172373);
    cJSON_AddNumberToObject(v1, "essence_mask", 7);
    cJSON_AddNumberToObject(v1, "rounds_completed", 17);
    cJSON_AddStringToObject(v1, "outfit_main_id", "top_crop");
    char err[128] = {0};
    TEST_ASSERT_TRUE(game_migrate_1_to_2(v1, err, (int)sizeof err));
    TEST_ASSERT_TRUE(cJSON_IsObject(cJSON_GetObjectItemCaseSensitive(v1, "saved_looks")));

    GameState migrated;
    TEST_ASSERT_TRUE(game_state_from_json(&migrated, v1, err, (int)sizeof err));
    TEST_ASSERT_EQUAL_INT(43, migrated.recipe_mask);
    TEST_ASSERT_EQUAL_INT(172373, migrated.lookbook_mask);
    TEST_ASSERT_EQUAL_INT(7, migrated.essence_mask);
    TEST_ASSERT_EQUAL_INT(17, migrated.rounds_completed);
    TEST_ASSERT_EQUAL_STRING("top_crop", migrated.outfit_main_id);
    for (int i = 0; i < GAME_STATE_MAX_SAVED_LOOKS; ++i) {
        TEST_ASSERT_FALSE(migrated.saved_looks[i].used);
    }
    cJSON_Delete(v1);
}

void test_nineteenth_saved_look_is_rejected_transactionally(void) {
    GameState state;
    game_state_init_defaults(&state);
    cJSON *root = cJSON_CreateObject();
    cJSON *looks = cJSON_AddObjectToObject(root, "saved_looks");
    for (int i = 0; i < 19; ++i) {
        char key[32];
        snprintf(key, sizeof key, "recipe-%d/%d", i / 3, i % 3);
        cJSON *look = cJSON_AddObjectToObject(looks, key);
        cJSON_AddStringToObject(look, "hair_id", "hair_bob");
        cJSON_AddStringToObject(look, "main_id", "top_tee");
        cJSON_AddStringToObject(look, "bottom_id", "bot_jeans");
        cJSON_AddStringToObject(look, "shoes_id", "shoe_sneak");
        cJSON_AddStringToObject(look, "accent_id", "acc_glasses");
    }
    char err[128] = {0};
    TEST_ASSERT_FALSE(game_state_from_json(&state, root, err, (int)sizeof err));
    for (int i = 0; i < GAME_STATE_MAX_SAVED_LOOKS; ++i) {
        TEST_ASSERT_FALSE(state.saved_looks[i].used);
    }
    cJSON_Delete(root);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_defaults_round_trip);
    RUN_TEST(test_from_json_tolerant);
    RUN_TEST(test_get_set_path_int);
    RUN_TEST(test_awakening_progress_round_trip);
    RUN_TEST(test_eighteen_saved_looks_round_trip_at_capacity);
    RUN_TEST(test_v1_to_v2_migration_preserves_progress_and_adds_empty_saved_looks);
    RUN_TEST(test_nineteenth_saved_look_is_rejected_transactionally);
    return UNITY_END();
}
