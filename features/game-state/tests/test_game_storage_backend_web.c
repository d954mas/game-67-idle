#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "game_storage.h"
#include "game_storage_web.h"

static char s_primary[256];
static char s_last_key[256];
static char s_corrupt_key[256];
static char s_corrupt_data[256];
static int s_load_status;
static bool s_save_allowed;
static bool s_delete_allowed;
static bool s_probe_allowed;
static bool s_primary_exists;

static char *copy_text(const char *text) {
    const size_t size = strlen(text) + 1U;
    char *copy = (char *)malloc(size);
    if (copy != NULL) {
        memcpy(copy, text, size);
    }
    return copy;
}

void setUp(void) {
    s_primary[0] = '\0';
    s_last_key[0] = '\0';
    s_corrupt_key[0] = '\0';
    s_corrupt_data[0] = '\0';
    s_load_status = GAME_STORAGE_READ_ABSENT;
    s_save_allowed = true;
    s_delete_allowed = true;
    s_probe_allowed = true;
    s_primary_exists = false;
}

void tearDown(void) {}

int game_storage_web_key_exists(const char *key) {
    (void)snprintf(s_last_key, sizeof s_last_key, "%s", key);
    return s_primary_exists ? 1 : 0;
}

int game_storage_web_save(const char *key, const char *text) {
    (void)snprintf(s_last_key, sizeof s_last_key, "%s", key);
    if (!s_save_allowed) {
        return 0;
    }
    if (strstr(key, ".corrupt") != NULL) {
        (void)snprintf(s_corrupt_key, sizeof s_corrupt_key, "%s", key);
        (void)snprintf(s_corrupt_data, sizeof s_corrupt_data, "%s", text);
    } else {
        (void)snprintf(s_primary, sizeof s_primary, "%s", text);
        s_primary_exists = true;
        s_load_status = GAME_STORAGE_READ_OK;
    }
    return 1;
}

char *game_storage_web_load(const char *key, size_t max_bytes, int *status) {
    (void)snprintf(s_last_key, sizeof s_last_key, "%s", key);
    if (s_primary_exists && strlen(s_primary) > max_bytes) {
        if (status != NULL) {
            *status = GAME_STORAGE_READ_ERROR;
        }
        return NULL;
    }
    if (status != NULL) {
        *status = s_load_status;
    }
    if (!s_primary_exists || s_load_status != GAME_STORAGE_READ_OK) {
        return NULL;
    }
    return copy_text(s_primary);
}

int game_storage_web_quarantine(const char *key) {
    (void)snprintf(s_last_key, sizeof s_last_key, "%s", key);
    if (!s_primary_exists || !s_save_allowed) {
        return 0;
    }
    (void)snprintf(s_corrupt_key, sizeof s_corrupt_key, "%s.corrupt", key);
    (void)snprintf(s_corrupt_data, sizeof s_corrupt_data, "%s", s_primary);
    if (s_delete_allowed) {
        s_primary_exists = false;
    }
    return 1;
}

int game_storage_web_probe(const char *key) {
    (void)snprintf(s_last_key, sizeof s_last_key, "%s", key);
    return s_probe_allowed ? 1 : 0;
}

void test_web_backend_scopes_keys_and_round_trips(void) {
    char error[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write_blocking("autosave", "payload", error, (int)sizeof error));
    TEST_ASSERT_EQUAL_STRING("web_storage_test/save/autosave", s_last_key);
    TEST_ASSERT_TRUE(game_storage_exists("autosave"));

    char *out = NULL;
    game_storage_read_status_t status = GAME_STORAGE_READ_ERROR;
    TEST_ASSERT_TRUE(game_storage_read(
        "autosave", &out, &status, error, (int)sizeof error));
    TEST_ASSERT_EQUAL_INT(GAME_STORAGE_READ_OK, status);
    TEST_ASSERT_EQUAL_STRING("payload", out);
    free(out);
}

void test_web_backend_reports_write_and_probe_failures(void) {
    char error[128] = {0};
    s_save_allowed = false;
    TEST_ASSERT_FALSE(game_storage_write("autosave", "payload", error, (int)sizeof error));
    TEST_ASSERT_TRUE(error[0] != '\0');

    error[0] = '\0';
    s_probe_allowed = false;
    TEST_ASSERT_FALSE(game_storage_probe(error, (int)sizeof error));
    TEST_ASSERT_EQUAL_STRING("web_storage_test/__probe", s_last_key);
    TEST_ASSERT_TRUE(error[0] != '\0');
}

void test_web_backend_rejects_oversize_write_before_adapter(void) {
    char error[128] = {0};
    char *oversize = malloc((size_t)GAME_STORAGE_MAX_BYTES + 2U);
    TEST_ASSERT_NOT_NULL(oversize);
    memset(oversize, 'x', (size_t)GAME_STORAGE_MAX_BYTES + 1U);
    oversize[GAME_STORAGE_MAX_BYTES + 1U] = '\0';

    TEST_ASSERT_FALSE(game_storage_write(
        "autosave", oversize, error, (int)sizeof error));
    TEST_ASSERT_FALSE(s_primary_exists);
    TEST_ASSERT_TRUE(error[0] != '\0');
    free(oversize);
}

void test_web_backend_distinguishes_absent_and_error(void) {
    char error[128] = {0};
    char *out = NULL;
    game_storage_read_status_t status = GAME_STORAGE_READ_ERROR;

    TEST_ASSERT_FALSE(game_storage_read(
        "autosave", &out, &status, error, (int)sizeof error));
    TEST_ASSERT_EQUAL_INT(GAME_STORAGE_READ_ABSENT, status);

    s_primary_exists = true;
    s_load_status = GAME_STORAGE_READ_ERROR;
    TEST_ASSERT_FALSE(game_storage_read(
        "autosave", &out, &status, error, (int)sizeof error));
    TEST_ASSERT_EQUAL_INT(GAME_STORAGE_READ_ERROR, status);

    s_load_status = GAME_STORAGE_READ_ERROR_PRESERVED;
    TEST_ASSERT_FALSE(game_storage_read(
        "autosave", &out, &status, error, (int)sizeof error));
    TEST_ASSERT_EQUAL_INT(GAME_STORAGE_READ_ERROR_PRESERVED, status);
}

void test_web_backend_quarantines_before_delete(void) {
    char error[128] = {0};
    (void)snprintf(s_primary, sizeof s_primary, "%s", "broken");
    s_primary_exists = true;
    s_load_status = GAME_STORAGE_READ_OK;

    TEST_ASSERT_TRUE(game_storage_quarantine(
        "autosave", error, (int)sizeof error));
    TEST_ASSERT_EQUAL_STRING(
        "web_storage_test/save/autosave.corrupt", s_corrupt_key);
    TEST_ASSERT_EQUAL_STRING("broken", s_corrupt_data);
    TEST_ASSERT_FALSE(s_primary_exists);
}

void test_web_backend_keeps_primary_when_quarantine_copy_fails(void) {
    char error[128] = {0};
    (void)snprintf(s_primary, sizeof s_primary, "%s", "broken");
    s_primary_exists = true;
    s_load_status = GAME_STORAGE_READ_OK;
    s_save_allowed = false;

    TEST_ASSERT_FALSE(game_storage_quarantine(
        "autosave", error, (int)sizeof error));
    TEST_ASSERT_TRUE(s_primary_exists);
    TEST_ASSERT_TRUE(error[0] != '\0');
}

void test_web_backend_accepts_verified_copy_when_primary_delete_fails(void) {
    char error[128] = {0};
    (void)snprintf(s_primary, sizeof s_primary, "%s", "broken");
    s_primary_exists = true;
    s_load_status = GAME_STORAGE_READ_OK;
    s_delete_allowed = false;

    TEST_ASSERT_TRUE(game_storage_quarantine(
        "autosave", error, (int)sizeof error));
    TEST_ASSERT_EQUAL_STRING("broken", s_corrupt_data);
    TEST_ASSERT_TRUE(s_primary_exists);
}

void test_web_backup_is_noop_but_still_validates_slot(void) {
    char error[128] = {0};
    TEST_ASSERT_TRUE(game_storage_write_backup(
        "autosave", error, (int)sizeof error));
    TEST_ASSERT_FALSE(game_storage_write_backup(
        "../bad", error, (int)sizeof error));
    TEST_ASSERT_TRUE(error[0] != '\0');

    char *out = NULL;
    TEST_ASSERT_FALSE(game_storage_read_backup(
        "autosave", &out, error, (int)sizeof error));
    TEST_ASSERT_NULL(out);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_web_backend_scopes_keys_and_round_trips);
    RUN_TEST(test_web_backend_reports_write_and_probe_failures);
    RUN_TEST(test_web_backend_rejects_oversize_write_before_adapter);
    RUN_TEST(test_web_backend_distinguishes_absent_and_error);
    RUN_TEST(test_web_backend_quarantines_before_delete);
    RUN_TEST(test_web_backend_keeps_primary_when_quarantine_copy_fails);
    RUN_TEST(test_web_backend_accepts_verified_copy_when_primary_delete_fails);
    RUN_TEST(test_web_backup_is_noop_but_still_validates_slot);
    return UNITY_END();
}
