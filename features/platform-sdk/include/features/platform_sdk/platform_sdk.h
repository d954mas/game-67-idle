#ifndef FEATURES_PLATFORM_SDK_PLATFORM_SDK_H
#define FEATURES_PLATFORM_SDK_PLATFORM_SDK_H

#include <stdbool.h>
#include <stddef.h>

typedef enum platform_target_t {
    PLATFORM_TARGET_LOCAL = 0,
    PLATFORM_TARGET_ITCH = 1,
    PLATFORM_TARGET_POKI = 2,
    PLATFORM_TARGET_YANDEX = 3,
    PLATFORM_TARGET_PLAYGAMA = 4,
} platform_target_t;

typedef enum platform_sdk_t {
    PLATFORM_SDK_MOCK = 0,
    PLATFORM_SDK_POKI = 1,
    PLATFORM_SDK_YANDEX = 2,
    PLATFORM_SDK_PLAYGAMA = 3,
} platform_sdk_t;

typedef struct platform_sdk_capabilities_t {
    bool external_links_allowed;
    bool ads_supported;
    bool rewarded_supported;
    bool storage_supported;
} platform_sdk_capabilities_t;

typedef enum platform_sdk_boot_status_t {
    PLATFORM_SDK_BOOT_NOT_STARTED = 0,
    PLATFORM_SDK_BOOT_INITIALIZING = 1,
    PLATFORM_SDK_BOOT_READY = 2,
    PLATFORM_SDK_BOOT_FAILED = 3,
    PLATFORM_SDK_BOOT_DESTROYED = 4,
} platform_sdk_boot_status_t;

typedef enum platform_sdk_result_t {
    PLATFORM_SDK_RESULT_OK = 0,
    PLATFORM_SDK_RESULT_NOT_READY = 1,
    PLATFORM_SDK_RESULT_DESTROYED = 2,
    PLATFORM_SDK_RESULT_WAITING_FOR_INPUT = 3,
    PLATFORM_SDK_RESULT_ALREADY_ACTIVE = 4,
    PLATFORM_SDK_RESULT_NOT_ACTIVE = 5,
    PLATFORM_SDK_RESULT_UNSUPPORTED = 6,
    PLATFORM_SDK_RESULT_BUSY = 7,
    PLATFORM_SDK_RESULT_FAILED = 8,
} platform_sdk_result_t;

typedef enum platform_sdk_ad_reason_t {
    PLATFORM_SDK_AD_REASON_NONE = 0,
    PLATFORM_SDK_AD_REASON_UNSUPPORTED = 1,
    PLATFORM_SDK_AD_REASON_NOT_READY = 2,
    PLATFORM_SDK_AD_REASON_RATE_LIMITED = 3,
    PLATFORM_SDK_AD_REASON_FAILED = 4,
    PLATFORM_SDK_AD_REASON_SKIPPED = 5,
    PLATFORM_SDK_AD_REASON_DECLINED = 6,
    PLATFORM_SDK_AD_REASON_COMPLETED = 7,
} platform_sdk_ad_reason_t;

typedef struct platform_sdk_ad_result_t {
    bool supported;
    bool shown;
    platform_sdk_ad_reason_t reason;
} platform_sdk_ad_result_t;

typedef struct platform_sdk_rewarded_result_t {
    bool supported;
    bool shown;
    bool rewarded;
    platform_sdk_ad_reason_t reason;
} platform_sdk_rewarded_result_t;

typedef struct platform_sdk_gameplay_start_result_t {
    bool started;
    platform_sdk_result_t reason;
} platform_sdk_gameplay_start_result_t;

typedef struct platform_sdk_gameplay_stop_result_t {
    bool stopped;
    platform_sdk_result_t reason;
} platform_sdk_gameplay_stop_result_t;

typedef void (*platform_sdk_lifecycle_callback_t)(void *userdata);
typedef void (*platform_sdk_ad_callback_t)(platform_sdk_ad_result_t result, void *userdata);
typedef void (*platform_sdk_rewarded_callback_t)(platform_sdk_rewarded_result_t result, void *userdata);

typedef unsigned int platform_sdk_listener_id_t;

typedef struct platform_sdk_backend_t {
    bool (*init)(void *userdata);
    void (*game_loading_progress)(float progress01, void *userdata);
    void (*game_loading_finished)(void *userdata);
    void (*game_ready)(void *userdata);
    void (*gameplay_start)(void *userdata);
    void (*gameplay_stop)(void *userdata);
    void (*measure)(const char *category, const char *what, const char *action, void *userdata);
    platform_sdk_result_t (*show_interstitial)(const char *placement, void *userdata);
    platform_sdk_result_t (*show_rewarded)(const char *placement, void *userdata);
    void (*destroy)(void *userdata);
} platform_sdk_backend_t;

platform_target_t platform_sdk_target(void);
platform_sdk_t platform_sdk_current(void);
const char *platform_sdk_target_name(void);
const char *platform_sdk_current_name(void);
platform_sdk_capabilities_t platform_sdk_capabilities(void);
bool platform_sdk_external_links_allowed(void);
bool platform_sdk_ads_supported(void);
bool platform_sdk_rewarded_supported(void);
bool platform_sdk_storage_supported(void);

void platform_sdk_set_backend(const platform_sdk_backend_t *backend, void *userdata);
platform_sdk_boot_status_t platform_sdk_status(void);
platform_sdk_result_t platform_sdk_init(void);
platform_sdk_result_t platform_sdk_game_loading_progress(float progress01);
platform_sdk_result_t platform_sdk_game_loading_finished(void);
platform_sdk_result_t platform_sdk_game_ready(void);

void platform_sdk_mark_input(void);
bool platform_sdk_has_input(void);
bool platform_sdk_has_gameplay_started(void);
bool platform_sdk_gameplay_active(void);
platform_sdk_gameplay_start_result_t platform_sdk_gameplay_start(void);
platform_sdk_gameplay_stop_result_t platform_sdk_gameplay_stop(void);

platform_sdk_listener_id_t platform_sdk_on_pause(platform_sdk_lifecycle_callback_t callback, void *userdata);
platform_sdk_listener_id_t platform_sdk_on_resume(platform_sdk_lifecycle_callback_t callback, void *userdata);
void platform_sdk_remove_listener(platform_sdk_listener_id_t listener_id);

platform_sdk_result_t platform_sdk_show_interstitial(
    const char *placement,
    platform_sdk_ad_callback_t callback,
    void *userdata);
platform_sdk_result_t platform_sdk_show_rewarded(
    const char *placement,
    platform_sdk_rewarded_callback_t callback,
    void *userdata);
void platform_sdk_backend_complete_interstitial(platform_sdk_ad_result_t result);
void platform_sdk_backend_complete_rewarded(platform_sdk_rewarded_result_t result);
void platform_sdk_backend_complete_init(bool ready);

void platform_sdk_destroy(void);

#if defined(PLATFORM_SDK_TESTING)
void platform_sdk_reset_for_tests(void);
#endif

#endif /* FEATURES_PLATFORM_SDK_PLATFORM_SDK_H */
