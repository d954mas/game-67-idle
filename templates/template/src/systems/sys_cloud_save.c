#include "systems/sys_cloud_save.h"

#include <stddef.h>

#include "features/platform_sdk/platform_sdk_cloud.h"

static game_save_cloud_read_status_t transport_read(char **owned_text) {
    if (owned_text != NULL) *owned_text = NULL;
    switch (platform_sdk_cloud_status()) {
        case PLATFORM_SDK_CLOUD_PENDING:
            return GAME_SAVE_CLOUD_READ_PENDING;
        case PLATFORM_SDK_CLOUD_READY:
            if (owned_text != NULL) *owned_text = platform_sdk_cloud_take();
            return owned_text != NULL && *owned_text != NULL
                ? GAME_SAVE_CLOUD_READ_READY : GAME_SAVE_CLOUD_READ_FAILED;
        case PLATFORM_SDK_CLOUD_EMPTY:
            return GAME_SAVE_CLOUD_READ_EMPTY;
        case PLATFORM_SDK_CLOUD_FAILED:
        default:
            return GAME_SAVE_CLOUD_READ_FAILED;
    }
}

static game_save_cloud_write_status_t transport_write_status(void) {
    switch (platform_sdk_cloud_write_status()) {
        case PLATFORM_SDK_CLOUD_WRITE_PENDING:
            return GAME_SAVE_CLOUD_WRITE_PENDING;
        case PLATFORM_SDK_CLOUD_WRITE_ACKNOWLEDGED:
            return GAME_SAVE_CLOUD_WRITE_ACKNOWLEDGED;
        case PLATFORM_SDK_CLOUD_WRITE_FAILED:
        default:
            return GAME_SAVE_CLOUD_WRITE_FAILED;
    }
}

void cloud_save_init(game_save_cloud_choose_fn choose,
                     game_save_cloud_same_features_fn same_features) {
    const game_save_cloud_config_t config = {
        .transport = {
            .supported = platform_sdk_cloud_supported,
            .load = platform_sdk_cloud_load,
            .read = transport_read,
            .store = platform_sdk_cloud_store,
            .write_status = transport_write_status,
        },
        .choose = choose,
        .same_features = same_features,
    };
    (void)game_save_cloud_init(&config);
}
