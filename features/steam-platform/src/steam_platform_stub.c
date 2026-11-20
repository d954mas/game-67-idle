/* The module for a build without the Steamworks SDK: it never starts, so the game plays solo.
   It still refuses a bad config, so a configuration bug shows in this build too. */

#include "steam_platform_common.h"
#include "steam_platform_internal.h"

steam_platform_status_t steam_platform_start(const steam_platform_config_t *config, char *message, size_t message_size) {
    if (steam_platform_check_config(config, message, message_size) != STEAM_PLATFORM_READY) {
        return STEAM_PLATFORM_FAILED;
    }
    steam_platform_write_message(message, message_size, "built without the Steamworks SDK");
    return STEAM_PLATFORM_NO_SDK;
}

void steam_platform_stop(void) {}

bool steam_platform_ready(void) { return false; }

void steam_platform_run_frame(void) {}

bool steam_platform_next_event(steam_platform_event_t *out) {
    (void)out;
    return false;
}

uint64_t steam_platform_user(void) { return 0; }

bool steam_platform_overlay_ready(void) { return false; }

void steam_platform_open_overlay(const char *dialog) { (void)dialog; }

bool steam_platform_internal_listen(int callback_id, steam_platform_callback_fn fn, void *user) {
    (void)callback_id;
    (void)fn;
    (void)user;
    return false;
}

void steam_platform_internal_unlisten(int callback_id, steam_platform_callback_fn fn, void *user) {
    (void)callback_id;
    (void)fn;
    (void)user;
}

bool steam_platform_internal_await_call(uint64_t api_call, int expected_callback_id, steam_platform_call_result_fn fn, void *user) {
    (void)api_call;
    (void)expected_callback_id;
    (void)fn;
    (void)user;
    return false;
}

bool steam_platform_internal_cancel_call(uint64_t api_call) {
    (void)api_call;
    return false;
}

void steam_platform_internal_counters(steam_platform_internal_counters_t *out) {
    if (out != NULL) {
        *out = (steam_platform_internal_counters_t){0};
    }
}

void steam_platform_internal_simulate_failure(bool restart, int init_result, const char *message) {
    (void)restart;
    (void)init_result;
    (void)message;
}

bool steam_platform_internal_feed(int callback_id, const void *data, size_t size) {
    (void)callback_id;
    (void)data;
    (void)size;
    return false;
}
