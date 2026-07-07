/* System headers before Unity to avoid noreturn / __declspec conflict on MSVC
   (unity_internals.h pulls in <stdnoreturn.h>; ср. test_game_save.c). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "items_state.h"

/* И2a skeleton (§6.8): the fragment layer round-trips before items_containers.c
   (И2b) exists. Links items_state.c + items_state_events.gen.c (R2: NOT empty,
   unlike settings) + items_bootstrap.c stubs (H1) + gsj_* toolkit + game_events.c;
   game_save.c is NOT linked (generated code makes no game_save_* calls, mirrors
   test_game_state_roundtrip.c). Owned-map manipulation (add/remove/quarantine) is
   И2b (items_containers.c does not exist yet). */

void setUp(void) {}
void tearDown(void) {}

/* reset() -> owned{} empty (Р9: reset is the NEUTRAL default, distinct from
   on_new_game -- И2a stub is a no-op, so this also proves the stub does nothing). */
void test_reset_owned_empty(void) {
    items_state_fragment.reset();
    cJSON *json = items_state_to_json(&items_state);
    TEST_ASSERT_NOT_NULL(json);
    const cJSON *owned = cJSON_GetObjectItemCaseSensitive(json, "owned");
    TEST_ASSERT_NOT_NULL(owned);
    TEST_ASSERT_TRUE(cJSON_IsObject(owned));
    TEST_ASSERT_EQUAL_INT(0, cJSON_GetArraySize(owned));
    cJSON_Delete(json);
}

/* init_defaults -> to_json -> from_json (other instance) -> to_json: equal (G4 groundwork). */
void test_round_trip_byte_stable(void) {
    ItemsState a;
    items_state_init_defaults(&a);
    cJSON *ja = items_state_to_json(&a);
    TEST_ASSERT_NOT_NULL(ja);
    char *sa = cJSON_PrintUnformatted(ja);
    TEST_ASSERT_NOT_NULL(sa);

    ItemsState b;
    char err[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&b, ja, err, (int)sizeof(err)));
    cJSON *jb = items_state_to_json(&b);
    char *sb = cJSON_PrintUnformatted(jb);
    TEST_ASSERT_NOT_NULL(sb);

    TEST_ASSERT_EQUAL_STRING(sa, sb);

    cJSON_free(sa);
    cJSON_free(sb);
    cJSON_Delete(ja);
    cJSON_Delete(jb);
}

void test_schema_json_contains_owned(void) {
    cJSON *schema = items_state_schema_json();
    TEST_ASSERT_NOT_NULL(schema);
    char *text = cJSON_PrintUnformatted(schema);
    TEST_ASSERT_NOT_NULL(text);
    TEST_ASSERT_NOT_NULL(strstr(text, "owned"));
    cJSON_free(text);
    cJSON_Delete(schema);
}

/* fixtures/items_v1.json (§6.8/§9): v1-payload migration anchor -- tolerant
   from_json loads it. WORKING_DIRECTORY for this test is tests/ (CMakeLists.txt),
   so the relative path below resolves regardless of ctest's own cwd. */
void test_fixture_v1_loads(void) {
    FILE *f = fopen("fixtures/items_v1.json", "rb");
    TEST_ASSERT_NOT_NULL(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)size + 1);
    TEST_ASSERT_NOT_NULL(buf);
    size_t got = fread(buf, 1, (size_t)size, f);
    buf[got] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    TEST_ASSERT_NOT_NULL(json);

    ItemsState state;
    char err[128] = {0};
    TEST_ASSERT_TRUE(items_state_from_json(&state, json, err, (int)sizeof(err)));
    cJSON_Delete(json);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reset_owned_empty);
    RUN_TEST(test_round_trip_byte_stable);
    RUN_TEST(test_schema_json_contains_owned);
    RUN_TEST(test_fixture_v1_loads);
    return UNITY_END();
}
