#include "features/platform_sdk/platform_sdk_cloud.h"
#include <stdlib.h>
#include <string.h>

#include "unity.h"

static uint32_t s_read_id;
static uint32_t s_write_id;
static bool supported(void *context) { (void)context; return true; }
static void read_data(uint32_t id, const char *key, void *context) {
    (void)key; (void)context; s_read_id = id;
}
static void write_data(uint32_t id, const char *key, const char *text, void *context) {
    (void)key; (void)text; (void)context; s_write_id = id;
}
void setUp(void) {
    const platform_sdk_cloud_backend_t backend = {
        .supported = supported, .load = read_data, .store = write_data,
    };
    platform_sdk_cloud_set_backend(&backend, NULL);
}
void tearDown(void) { platform_sdk_cloud_reset(); }

static void stale_reads_cannot_replace_newer_documents(void) {
    platform_sdk_cloud_load("save");
    const uint32_t old = s_read_id;
    platform_sdk_cloud_load("save");
    platform_sdk_cloud_complete_load(old, PLATFORM_SDK_CLOUD_READY, "old");
    TEST_ASSERT_EQUAL(PLATFORM_SDK_CLOUD_PENDING, platform_sdk_cloud_status());
    platform_sdk_cloud_complete_load(s_read_id, PLATFORM_SDK_CLOUD_READY, "new");
    char *text = platform_sdk_cloud_take();
    TEST_ASSERT_EQUAL_STRING("new", text);
    free(text);
}

static void errors_are_distinct_from_empty_storage(void) {
    platform_sdk_cloud_load("save");
    platform_sdk_cloud_complete_load(s_read_id, PLATFORM_SDK_CLOUD_FAILED, NULL);
    TEST_ASSERT_EQUAL(PLATFORM_SDK_CLOUD_FAILED, platform_sdk_cloud_status());
    platform_sdk_cloud_load("save");
    platform_sdk_cloud_complete_load(s_read_id, PLATFORM_SDK_CLOUD_EMPTY, NULL);
    TEST_ASSERT_EQUAL(PLATFORM_SDK_CLOUD_EMPTY, platform_sdk_cloud_status());
}

static void writes_require_acknowledgment_and_reject_stale_completions(void) {
    TEST_ASSERT_TRUE(platform_sdk_cloud_store("save", "first"));
    const uint32_t old = s_write_id;
    TEST_ASSERT_FALSE(platform_sdk_cloud_store("save", "second"));
    platform_sdk_cloud_complete_store(old, PLATFORM_SDK_CLOUD_WRITE_FAILED);
    TEST_ASSERT_EQUAL(PLATFORM_SDK_CLOUD_WRITE_FAILED, platform_sdk_cloud_write_status());
    TEST_ASSERT_TRUE(platform_sdk_cloud_store("save", "second"));
    platform_sdk_cloud_complete_store(old, PLATFORM_SDK_CLOUD_WRITE_ACKNOWLEDGED);
    TEST_ASSERT_EQUAL(PLATFORM_SDK_CLOUD_WRITE_PENDING, platform_sdk_cloud_write_status());
    platform_sdk_cloud_complete_store(s_write_id, PLATFORM_SDK_CLOUD_WRITE_ACKNOWLEDGED);
    TEST_ASSERT_EQUAL(PLATFORM_SDK_CLOUD_WRITE_ACKNOWLEDGED, platform_sdk_cloud_write_status());
}

static void replacing_backend_invalidates_inflight_reads(void) {
    platform_sdk_cloud_load("save");
    const uint32_t old = s_read_id;
    setUp();
    platform_sdk_cloud_load("save");
    platform_sdk_cloud_complete_load(old, PLATFORM_SDK_CLOUD_READY, "stale");
    TEST_ASSERT_EQUAL(PLATFORM_SDK_CLOUD_PENDING, platform_sdk_cloud_status());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(stale_reads_cannot_replace_newer_documents);
    RUN_TEST(errors_are_distinct_from_empty_storage);
    RUN_TEST(writes_require_acknowledgment_and_reject_stale_completions);
    RUN_TEST(replacing_backend_invalidates_inflight_reads);
    return UNITY_END();
}
