#ifndef FEATURES_PLATFORM_SDK_PLATFORM_SDK_CLOUD_H
#define FEATURES_PLATFORM_SDK_PLATFORM_SDK_CLOUD_H

#include <stdbool.h>
#include <stdint.h>

typedef enum platform_sdk_cloud_status {
    PLATFORM_SDK_CLOUD_IDLE = 0,
    PLATFORM_SDK_CLOUD_PENDING,
    PLATFORM_SDK_CLOUD_READY,
    PLATFORM_SDK_CLOUD_EMPTY,
    PLATFORM_SDK_CLOUD_UNAVAILABLE,
    PLATFORM_SDK_CLOUD_FAILED
} platform_sdk_cloud_status_t;

typedef enum platform_sdk_cloud_write_status {
    PLATFORM_SDK_CLOUD_WRITE_IDLE = 0,
    PLATFORM_SDK_CLOUD_WRITE_PENDING,
    PLATFORM_SDK_CLOUD_WRITE_ACKNOWLEDGED,
    PLATFORM_SDK_CLOUD_WRITE_UNAVAILABLE,
    PLATFORM_SDK_CLOUD_WRITE_FAILED
} platform_sdk_cloud_write_status_t;

/* Borrowed arguments last through the backend call; asynchronous backends
   copy them before returning. Completions must echo the supplied request id. */
typedef struct platform_sdk_cloud_backend {
    bool (*supported)(void *context);
    void (*load)(uint32_t request_id, const char *key, void *context);
    void (*store)(uint32_t request_id, const char *key, const char *text, void *context);
} platform_sdk_cloud_backend_t;

void platform_sdk_cloud_set_backend(const platform_sdk_cloud_backend_t *backend, void *context);
void platform_sdk_cloud_install_web_backend(void);
void platform_sdk_cloud_reset(void);
bool platform_sdk_cloud_supported(void);
void platform_sdk_cloud_load(const char *key);
platform_sdk_cloud_status_t platform_sdk_cloud_status(void);
/* Ownership passes to the caller, which must free the returned text. */
char *platform_sdk_cloud_take(void);
/* One write at a time. The caller retains unsent snapshots and owns retries. */
bool platform_sdk_cloud_store(const char *key, const char *text);
platform_sdk_cloud_write_status_t platform_sdk_cloud_write_status(void);
void platform_sdk_cloud_complete_load(uint32_t request_id, platform_sdk_cloud_status_t status,
                                      const char *text);
void platform_sdk_cloud_complete_store(uint32_t request_id, platform_sdk_cloud_write_status_t status);

#endif
