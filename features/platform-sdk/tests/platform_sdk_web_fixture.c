#include "features/platform_sdk/platform_sdk.h"
#include "features/platform_sdk/platform_sdk_cloud.h"
#include "features/platform_sdk/platform_sdk_web.h"

#include <stdlib.h>
#include <string.h>

static int s_pause_count;
static int s_resume_count;
static int s_interstitial_completions;
static int s_last_interstitial_reason;

static void noted_pause(void *userdata) {
    (void)userdata;
    ++s_pause_count;
}

static void noted_resume(void *userdata) {
    (void)userdata;
    ++s_resume_count;
}

static void completed_interstitial(platform_sdk_ad_result_t result, void *userdata) {
    (void)userdata;
    ++s_interstitial_completions;
    s_last_interstitial_reason = (int)result.reason;
}

int fixture_boot(void) {
    platform_sdk_install_web_backend();
    (void)platform_sdk_on_pause(noted_pause, NULL);
    (void)platform_sdk_on_resume(noted_resume, NULL);
    return (int)platform_sdk_init();
}

int fixture_status(void) { return (int)platform_sdk_status(); }
int fixture_portal_paused(void) { return platform_sdk_portal_paused() ? 1 : 0; }
int fixture_audio_enabled(void) { return platform_sdk_portal_audio_enabled() ? 1 : 0; }
int fixture_break_active(void) { return platform_sdk_break_active() ? 1 : 0; }
int fixture_pause_count(void) { return s_pause_count; }
int fixture_resume_count(void) { return s_resume_count; }

int fixture_show_interstitial(void) {
    return (int)platform_sdk_show_interstitial("contract", completed_interstitial, NULL);
}

unsigned int fixture_active_interstitial_id(void) {
    return platform_sdk_active_interstitial_request_id();
}

void fixture_complete_interstitial(unsigned int id, int shown, int reason) {
    platform_sdk_backend_complete_interstitial_request(id, (platform_sdk_ad_result_t){
        .supported = true,
        .shown = shown != 0,
        .reason = (platform_sdk_ad_reason_t)reason,
    });
}

int fixture_interstitial_completions(void) { return s_interstitial_completions; }
int fixture_last_interstitial_reason(void) { return s_last_interstitial_reason; }

void fixture_cloud_load(void) { platform_sdk_cloud_load("save"); }
int fixture_cloud_load_status(void) { return (int)platform_sdk_cloud_status(); }

int fixture_cloud_take_is(void) {
    char *value = platform_sdk_cloud_take();
    const int matches = value != NULL && strcmp(value, "current") == 0;
    free(value);
    return matches;
}

int fixture_cloud_store(void) { return platform_sdk_cloud_store("save", "snapshot") ? 1 : 0; }
int fixture_cloud_write_status(void) { return (int)platform_sdk_cloud_write_status(); }
