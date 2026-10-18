#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "game_save.h"
#include "game_state_json.h"
#include "game_storage.h"
#include "game_storage_web.h"
#include "unity.h"

/* The real web backend over a fake localStorage with a byte quota, so the
   loader's reaction to QuotaExceededError is exercised end to end. */

#define FAKE_KEYS 8
#define PRIMARY_KEY "quota_test/save/test_slot"
#define DROPPABLE_KEY PRIMARY_KEY ".premigration"
#define KEPT_KEY PRIMARY_KEY ".premigration-kept"
#define LATEST_KEY PRIMARY_KEY ".premigration-latest"

typedef struct {
    char key[128];
    char *value;
} fake_item_t;

static fake_item_t s_items[FAKE_KEYS];
static size_t s_quota;
static const char *s_forced_failure;

static fake_item_t *find_item(const char *key) {
    for (int i = 0; i < FAKE_KEYS; i++) {
        if (s_items[i].value != NULL && strcmp(s_items[i].key, key) == 0) {
            return &s_items[i];
        }
    }
    return NULL;
}

static size_t stored_bytes(void) {
    size_t total = 0;
    for (int i = 0; i < FAKE_KEYS; i++) {
        if (s_items[i].value != NULL) {
            total += strlen(s_items[i].value);
        }
    }
    return total;
}

static void fail_with(char *reason, int reason_cap, const char *why) {
    if (reason != NULL && reason_cap > 0) {
        (void)snprintf(reason, (size_t)reason_cap, "%s", why);
    }
}

static char *copy_text(const char *text) {
    const size_t size = strlen(text) + 1U;
    char *copy = (char *)malloc(size);
    if (copy != NULL) {
        memcpy(copy, text, size);
    }
    return copy;
}

static void remove_item(const char *key) {
    fake_item_t *item = find_item(key);
    if (item != NULL) {
        free(item->value);
        item->value = NULL;
    }
}

static int put_item(const char *key, const char *text, char *reason, int reason_cap) {
    if (s_forced_failure != NULL) {
        fail_with(reason, reason_cap, s_forced_failure);
        return 0;
    }
    fake_item_t *item = find_item(key);
    const size_t old_size = item != NULL ? strlen(item->value) : 0U;
    if (stored_bytes() - old_size + strlen(text) > s_quota) {
        fail_with(reason, reason_cap, "exception QuotaExceededError");
        return 0;
    }
    if (item == NULL) {
        for (int i = 0; i < FAKE_KEYS && item == NULL; i++) {
            if (s_items[i].value == NULL) {
                item = &s_items[i];
            }
        }
        TEST_ASSERT_NOT_NULL(item);
        (void)snprintf(item->key, sizeof item->key, "%s", key);
    } else {
        free(item->value);
    }
    item->value = copy_text(text);
    return 1;
}

int game_storage_web_key_exists(const char *key) { return find_item(key) != NULL; }

int game_storage_web_save(const char *key, const char *text, char *reason, int reason_cap) {
    return put_item(key, text, reason, reason_cap);
}

char *game_storage_web_load(
    const char *key, size_t max_bytes, int *status, char *reason, int reason_cap) {
    (void)max_bytes;
    (void)reason;
    (void)reason_cap;
    fake_item_t *item = find_item(key);
    if (status != NULL) {
        *status = item != NULL ? GAME_STORAGE_READ_OK : GAME_STORAGE_READ_ABSENT;
    }
    return item != NULL ? copy_text(item->value) : NULL;
}

int game_storage_web_quarantine(const char *key, char *reason, int reason_cap) {
    fake_item_t *item = find_item(key);
    if (item == NULL) {
        fail_with(reason, reason_cap, "slot holds nothing to move aside");
        return 0;
    }
    char quarantine_key[160];
    (void)snprintf(quarantine_key, sizeof quarantine_key, "%s.corrupt", key);
    char *data = copy_text(item->value);
    const int ok = put_item(quarantine_key, data, reason, reason_cap);
    free(data);
    if (ok) {
        remove_item(key);
    }
    return ok;
}

int game_storage_web_remove(const char *key, char *reason, int reason_cap) {
    (void)reason;
    (void)reason_cap;
    remove_item(key);
    return 1;
}

int game_storage_web_probe(const char *key, char *reason, int reason_cap) {
    (void)key;
    (void)reason;
    (void)reason_cap;
    return 1;
}

static int64_t s_clock_ms = 1000000;
static int64_t test_clock(void) { return s_clock_ms; }

static int s_coins;

static bool migrate_game_v1_to_v2(cJSON *frag, char *err, int cap) {
    (void)err;
    (void)cap;
    const cJSON *coins = cJSON_GetObjectItemCaseSensitive(frag, "coins");
    return cJSON_IsNumber(coins) &&
           cJSON_ReplaceItemInObjectCaseSensitive(frag, "coins", cJSON_CreateNumber(coins->valuedouble + 1000));
}

static const GameSaveMigrateFn s_game_steps[] = {migrate_game_v1_to_v2};

static cJSON *game_to_json(void) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "coins", (double)s_coins);
    return o;
}

static bool game_from_json(const cJSON *json, char *err, int cap) {
    int coins = 0;
    if (!gsj_read_int_range(json, "coins", 0, 1000000000, &coins, err, cap)) {
        return false;
    }
    s_coins = coins;
    return true;
}

static void game_reset(void) { s_coins = 0; }

static const GameSaveFragment s_game = {
    .id = "game",
    .version = 2,
    .steps = s_game_steps,
    .reset = game_reset,
    .to_json = game_to_json,
    .from_json = game_from_json,
};

static const char k_v1_save[] =
    "{\"format\":1,\"save_version\":1,\"saved_at\":1,\"save_seq\":1,\"app\":\"quota_test\","
    "\"build\":\"0\",\"features\":{\"game\":{\"v\":1,\"coins\":5}}}";

void setUp(void) {
    for (int i = 0; i < FAKE_KEYS; i++) {
        free(s_items[i].value);
        s_items[i].value = NULL;
    }
    s_quota = 1U << 20;
    s_forced_failure = NULL;
    s_coins = 0;
    game_save__set_clocks_for_test(test_clock, test_clock);
    game_save_init();
}

void tearDown(void) {}

/* Start a session whose load migrated the stored v1 save into a droppable copy. */
static void load_migrated_save(void) {
    char error[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write_blocking("test_slot", k_v1_save, error, (int)sizeof error));
    game_save_load_result_t result;
    game_save_load(&result);
    TEST_ASSERT_EQUAL_INT(GAME_SAVE_LOAD_LOADED, result.status);
    TEST_ASSERT_EQUAL_INT(1005, s_coins);
    TEST_ASSERT_NOT_NULL(find_item(DROPPABLE_KEY));
}

void test_full_storage_drops_the_droppable_copy_and_saves(void) {
    load_migrated_save();
    s_quota = stored_bytes() - 1U;
    char error[128] = {0};

    TEST_ASSERT_TRUE(game_save_flush(error, (int)sizeof error));

    TEST_ASSERT_NULL(find_item(DROPPABLE_KEY));
    TEST_ASSERT_NOT_NULL(strstr(find_item(PRIMARY_KEY)->value, "1005"));
    TEST_ASSERT_FALSE(game_save_is_unpersisted());
}

void test_kept_copy_survives_when_dropping_the_droppable_one_is_enough(void) {
    load_migrated_save();
    char error[128] = {0};
    TEST_ASSERT_TRUE(put_item(KEPT_KEY, "LOST-DATA-EVIDENCE", error, (int)sizeof error));
    s_quota = stored_bytes() - 1U;

    TEST_ASSERT_TRUE(game_save_flush(error, (int)sizeof error));

    TEST_ASSERT_NULL(find_item(DROPPABLE_KEY));
    TEST_ASSERT_NOT_NULL(find_item(KEPT_KEY));
}

void test_kept_copy_goes_last_when_nothing_else_makes_room(void) {
    load_migrated_save();
    char error[128] = {0};
    remove_item(DROPPABLE_KEY);
    TEST_ASSERT_TRUE(put_item(KEPT_KEY, k_v1_save, error, (int)sizeof error));
    s_quota = stored_bytes() - 1U;

    TEST_ASSERT_TRUE(game_save_flush(error, (int)sizeof error));

    TEST_ASSERT_NULL(find_item(KEPT_KEY));
}

void test_latest_copy_goes_before_the_first_one(void) {
    load_migrated_save();
    char error[128] = {0};
    remove_item(DROPPABLE_KEY);
    TEST_ASSERT_TRUE(put_item(KEPT_KEY, k_v1_save, error, (int)sizeof error));
    TEST_ASSERT_TRUE(put_item(LATEST_KEY, k_v1_save, error, (int)sizeof error));
    s_quota = stored_bytes() - 1U;

    TEST_ASSERT_TRUE(game_save_flush(error, (int)sizeof error));

    TEST_ASSERT_NULL(find_item(LATEST_KEY));
    TEST_ASSERT_NOT_NULL(find_item(KEPT_KEY));
}

void test_other_write_failures_keep_the_copy(void) {
    load_migrated_save();
    s_forced_failure = "exception SecurityError";
    char error[128] = {0};

    TEST_ASSERT_FALSE(game_save_flush(error, (int)sizeof error));

    TEST_ASSERT_NOT_NULL(find_item(DROPPABLE_KEY));
    TEST_ASSERT_TRUE(game_save_is_unpersisted());
}

int main(void) {
    game_save_register_fragment(&s_game);
    UNITY_BEGIN();
    RUN_TEST(test_full_storage_drops_the_droppable_copy_and_saves);
    RUN_TEST(test_kept_copy_survives_when_dropping_the_droppable_one_is_enough);
    RUN_TEST(test_kept_copy_goes_last_when_nothing_else_makes_room);
    RUN_TEST(test_latest_copy_goes_before_the_first_one);
    RUN_TEST(test_other_write_failures_keep_the_copy);
    return UNITY_END();
}
