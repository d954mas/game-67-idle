#include "unity.h"

#include "audio_backend.h"
#include "resource/nt_resource.h"
#include "nt_pack_format.h"

typedef enum fake_resource_mode_t {
    FAKE_RESOURCE_MISSING = 0,
    FAKE_RESOURCE_NOT_READY,
    FAKE_RESOURCE_WRONG_TYPE,
    FAKE_RESOURCE_READY_BLOB,
    FAKE_RESOURCE_EMPTY_BLOB,
} fake_resource_mode_t;

static fake_resource_mode_t s_mode;
static const uint8_t s_bytes[] = {0x52, 0x49, 0x46, 0x46};

void setUp(void) { s_mode = FAKE_RESOURCE_MISSING; }
void tearDown(void) {}

nt_resource_t nt_resource_find(nt_hash64_t resource_id) {
    return resource_id.value != 0 && s_mode != FAKE_RESOURCE_MISSING
        ? (nt_resource_t){1}
        : NT_RESOURCE_INVALID;
}

bool nt_resource_is_ready(nt_resource_t handle) {
    return handle.id != 0 && s_mode != FAKE_RESOURCE_NOT_READY;
}

uint8_t nt_resource_get_asset_type(nt_resource_t handle) {
    if (handle.id == 0) return 0;
    return s_mode == FAKE_RESOURCE_WRONG_TYPE ? NT_ASSET_TEXTURE : NT_ASSET_BLOB;
}

const uint8_t *nt_resource_get_blob(nt_resource_t handle, uint32_t *out_size) {
    if (handle.id == 0 || s_mode == FAKE_RESOURCE_EMPTY_BLOB) {
        if (out_size != NULL) *out_size = 0;
        return NULL;
    }
    if (out_size != NULL) *out_size = (uint32_t)sizeof(s_bytes);
    return s_bytes;
}

static uint64_t asset_id(void) { return UINT64_C(0x1234); }

void test_resource_state_requires_ready_blob(void) {
    TEST_ASSERT_EQUAL_UINT32(0, audio_core_resource_state(asset_id()));
    s_mode = FAKE_RESOURCE_NOT_READY;
    TEST_ASSERT_EQUAL_UINT32(1, audio_core_resource_state(asset_id()));
    s_mode = FAKE_RESOURCE_WRONG_TYPE;
    TEST_ASSERT_EQUAL_UINT32(3, audio_core_resource_state(asset_id()));
    s_mode = FAKE_RESOURCE_READY_BLOB;
    TEST_ASSERT_EQUAL_UINT32(2, audio_core_resource_state(asset_id()));
}

void test_blob_view_returns_only_non_empty_ready_bytes(void) {
    const void *bytes = NULL;
    uint32_t size = 0;
    TEST_ASSERT_FALSE(audio_core_resource_blob_view(asset_id(), &bytes, &size));

    s_mode = FAKE_RESOURCE_EMPTY_BLOB;
    TEST_ASSERT_FALSE(audio_core_resource_blob_view(asset_id(), &bytes, &size));

    s_mode = FAKE_RESOURCE_READY_BLOB;
    TEST_ASSERT_TRUE(audio_core_resource_blob_view(asset_id(), &bytes, &size));
    TEST_ASSERT_EQUAL_PTR(s_bytes, bytes);
    TEST_ASSERT_EQUAL_UINT32(sizeof(s_bytes), size);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_resource_state_requires_ready_blob);
    RUN_TEST(test_blob_view_returns_only_non_empty_ready_bytes);
    return UNITY_END();
}
