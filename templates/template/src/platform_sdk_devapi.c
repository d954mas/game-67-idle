/* The developer-facing view of the platform SDK. It exists so the probe panel
   does not have to: a panel that reports state has to occupy the player's
   screen to do it, and this reports the same state to the agent that asked. */
#if NT_DEVAPI_ENABLED

#include "platform_sdk_devapi.h"

#include "cJSON.h"
#include "devapi/nt_devapi.h"
#include "features/platform_sdk/platform_sdk.h"

static const char *boot_status_name(void) {
    switch (platform_sdk_status()) {
    case PLATFORM_SDK_BOOT_NOT_STARTED:
        return "not_started";
    case PLATFORM_SDK_BOOT_INITIALIZING:
        return "initializing";
    case PLATFORM_SDK_BOOT_READY:
        return "ready";
    case PLATFORM_SDK_BOOT_FAILED:
        return "failed";
    case PLATFORM_SDK_BOOT_DESTROYED:
        return "destroyed";
    }
    return "failed";
}

static bool platform_sdk_state(const cJSON *params, cJSON *result_obj, nt_devapi_error *err, void *user) {
    (void)params;
    (void)err;
    (void)user;
    const platform_sdk_capabilities_t caps = platform_sdk_capabilities();
    cJSON_AddStringToObject(result_obj, "target", platform_sdk_target_name());
    cJSON_AddStringToObject(result_obj, "sdk", platform_sdk_current_name());
    cJSON_AddStringToObject(result_obj, "status", boot_status_name());
    cJSON *capabilities = cJSON_AddObjectToObject(result_obj, "capabilities");
    cJSON_AddBoolToObject(capabilities, "external_links_allowed", caps.external_links_allowed);
    cJSON_AddBoolToObject(capabilities, "ads_supported", caps.ads_supported);
    cJSON_AddBoolToObject(capabilities, "rewarded_supported", caps.rewarded_supported);
    cJSON_AddBoolToObject(capabilities, "storage_supported", caps.storage_supported);
    return true;
}

void game_platform_sdk_register_devapi(void) {
    static const nt_devapi_command_desc desc = {
        "game.platform_sdk.state",
        "game",
        "Report the platform SDK target, backend, boot status and capabilities.",
        "none",
        "target, sdk, status, capabilities",
        "immediate",
        "none",
    };
    (void)nt_devapi_register(&desc, platform_sdk_state, NULL);
}

#endif
