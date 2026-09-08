#include "features/platform_sdk/platform_sdk_cloud.h"

#include <stdlib.h>
#include <string.h>

static platform_sdk_cloud_backend_t s_backend;
static void *s_context;
static uint32_t s_next_id;
static uint32_t s_read_id;
static uint32_t s_write_id;
static platform_sdk_cloud_status_t s_read_status;
static platform_sdk_cloud_write_status_t s_write_status;
static char *s_value;

static uint32_t next_id(void) {
    if (++s_next_id == 0) ++s_next_id;
    return s_next_id;
}

void platform_sdk_cloud_reset(void) {
    free(s_value);
    s_value = NULL;
    s_read_id = s_write_id = 0;
    s_read_status = PLATFORM_SDK_CLOUD_IDLE;
    s_write_status = PLATFORM_SDK_CLOUD_WRITE_IDLE;
    s_backend = (platform_sdk_cloud_backend_t){0};
    s_context = NULL;
}

void platform_sdk_cloud_set_backend(const platform_sdk_cloud_backend_t *backend, void *context) {
    platform_sdk_cloud_reset();
    if (backend != NULL) s_backend = *backend;
    s_context = context;
}

bool platform_sdk_cloud_supported(void) {
    return s_backend.load != NULL && s_backend.store != NULL &&
           (s_backend.supported == NULL || s_backend.supported(s_context));
}

void platform_sdk_cloud_load(const char *key) {
    if (key == NULL || key[0] == '\0') return;
    free(s_value);
    s_value = NULL;
    s_read_id = next_id();
    s_read_status = PLATFORM_SDK_CLOUD_UNAVAILABLE;
    if (!platform_sdk_cloud_supported()) return;
    s_read_status = PLATFORM_SDK_CLOUD_PENDING;
    s_backend.load(s_read_id, key, s_context);
}

platform_sdk_cloud_status_t platform_sdk_cloud_status(void) { return s_read_status; }

char *platform_sdk_cloud_take(void) {
    if (s_read_status != PLATFORM_SDK_CLOUD_READY) return NULL;
    char *value = s_value;
    s_value = NULL;
    s_read_status = PLATFORM_SDK_CLOUD_IDLE;
    return value;
}

bool platform_sdk_cloud_store(const char *key, const char *text) {
    if (key == NULL || key[0] == '\0' || text == NULL ||
        s_write_status == PLATFORM_SDK_CLOUD_WRITE_PENDING) return false;
    s_write_status = PLATFORM_SDK_CLOUD_WRITE_UNAVAILABLE;
    if (!platform_sdk_cloud_supported()) return false;
    s_write_id = next_id();
    s_write_status = PLATFORM_SDK_CLOUD_WRITE_PENDING;
    s_backend.store(s_write_id, key, text, s_context);
    return true;
}

platform_sdk_cloud_write_status_t platform_sdk_cloud_write_status(void) { return s_write_status; }

void platform_sdk_cloud_complete_load(uint32_t id, platform_sdk_cloud_status_t status,
                                      const char *text) {
    if (id != s_read_id || s_read_status != PLATFORM_SDK_CLOUD_PENDING) return;
    if (status == PLATFORM_SDK_CLOUD_READY && text != NULL) {
        const size_t length = strlen(text);
        s_value = malloc(length + 1);
        if (s_value != NULL) memcpy(s_value, text, length + 1);
        s_read_status = s_value != NULL ? PLATFORM_SDK_CLOUD_READY : PLATFORM_SDK_CLOUD_FAILED;
    } else {
        s_read_status = status == PLATFORM_SDK_CLOUD_EMPTY || status == PLATFORM_SDK_CLOUD_UNAVAILABLE
            ? status : PLATFORM_SDK_CLOUD_FAILED;
    }
}

void platform_sdk_cloud_complete_store(uint32_t id, platform_sdk_cloud_write_status_t status) {
    if (id != s_write_id || s_write_status != PLATFORM_SDK_CLOUD_WRITE_PENDING) return;
    s_write_status = status == PLATFORM_SDK_CLOUD_WRITE_ACKNOWLEDGED ||
                     status == PLATFORM_SDK_CLOUD_WRITE_UNAVAILABLE
        ? status : PLATFORM_SDK_CLOUD_WRITE_FAILED;
}
