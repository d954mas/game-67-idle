#if NT_DEVAPI_ENABLED

#include "systems/sys_platform_hooks.h"

#include "app/nt_app.h"
#include "cJSON.h"
#include "devapi/nt_devapi.h"
#include "features/platform_sdk/platform_sdk.h"

#include <stdio.h>

static char s_error[128];

static bool hooks_fail(nt_devapi_error *error, const char *code, const char *message) {
    (void)snprintf(s_error, sizeof s_error, "%s", message);
    error->code = code;
    error->message = s_error;
    return false;
}

static bool optional_bool(const cJSON *params, const char *name, bool *out) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, name);
    if (!cJSON_IsBool(value)) {
        return false;
    }
    *out = cJSON_IsTrue(value) ? true : false;
    return true;
}

static const char *required_string(const cJSON *params, const char *name) {
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(params, name);
    return cJSON_IsString(value) && value->valuestring != NULL && value->valuestring[0] != '\0'
               ? value->valuestring
               : NULL;
}

/* The one snapshot every focus/ad check reads: the app clock is the honest
   witness that the simulation stopped, because every system is driven by it. */
static void add_state(cJSON *result_obj) {
    cJSON_AddBoolToObject(result_obj, "attention", platform_hooks_attention());
    cJSON_AddBoolToObject(result_obj, "gameplay_allowed", platform_hooks_gameplay_allowed());
    cJSON_AddBoolToObject(result_obj, "ad_active", platform_hooks_ad_active());
    cJSON_AddBoolToObject(result_obj, "sim_paused", g_nt_app.paused);
    cJSON_AddNumberToObject(result_obj, "app_frame", (double)g_nt_app.frame);
    cJSON_AddNumberToObject(result_obj, "app_time", g_nt_app.time);
    cJSON_AddBoolToObject(result_obj, "gameplay_active", platform_sdk_gameplay_active());
    cJSON_AddNumberToObject(result_obj, "breaks", (double)platform_hooks_break_count());
    cJSON_AddNumberToObject(result_obj, "rewards", (double)platform_hooks_reward_grant_count());
}

static bool hooks_state(
    const cJSON *params, cJSON *result_obj, nt_devapi_error *error, void *user) {
    (void)params;
    (void)error;
    (void)user;
    add_state(result_obj);
    return true;
}

static bool hooks_attention(
    const cJSON *params, cJSON *result_obj, nt_devapi_error *error, void *user) {
    bool visible = false;
    bool focused = false;
    bool touched = false;
    (void)user;

    if (optional_bool(params, "visible", &visible)) {
        platform_hooks_set_page_visible(visible);
        touched = true;
    }
    if (optional_bool(params, "focused", &focused)) {
        platform_hooks_set_window_focused(focused);
        touched = true;
    }
    if (!touched) {
        return hooks_fail(error, "bad_params", "pass visible and/or focused as booleans");
    }
    add_state(result_obj);
    return true;
}

static bool hooks_break(
    const cJSON *params, cJSON *result_obj, nt_devapi_error *error, void *user) {
    const char *placement = required_string(params, "placement");
    (void)user;
    if (placement == NULL) {
        return hooks_fail(error, "bad_params", "placement must be a non-empty string");
    }
    cJSON_AddBoolToObject(result_obj, "requested", platform_hooks_commercial_break(placement));
    add_state(result_obj);
    return true;
}

/* A bot-visible payout: the reward itself belongs to the system that asked for
   it, so the probe only proves the callback fired. */
static unsigned int s_devapi_reward_calls;

static void devapi_reward(const char *placement, void *userdata) {
    (void)placement;
    (void)userdata;
    s_devapi_reward_calls += 1u;
}

static bool hooks_reward(
    const cJSON *params, cJSON *result_obj, nt_devapi_error *error, void *user) {
    const char *placement = required_string(params, "placement");
    (void)user;
    if (placement == NULL) {
        return hooks_fail(error, "bad_params", "placement must be a non-empty string");
    }
    cJSON_AddBoolToObject(
        result_obj, "requested", platform_hooks_rewarded(placement, devapi_reward, NULL));
    cJSON_AddNumberToObject(result_obj, "callbacks", (double)s_devapi_reward_calls);
    add_state(result_obj);
    return true;
}

void platform_hooks_register_devapi(void) {
    static const nt_devapi_command_desc state_desc = {
        "platform.state", "game",
        "Read attention, ad and app-clock state behind the portal lifecycle.",
        "none",
        "attention, gameplay_allowed, ad_active, sim_paused, app_frame, app_time, "
        "gameplay_active, breaks, rewards",
        "immediate", "none"};
    static const nt_devapi_command_desc attention_desc = {
        "platform.attention", "game",
        "Simulate the browser losing or regaining attention (tab hidden, window blur).",
        "visible, focused",
        "attention, gameplay_allowed, sim_paused, app_frame, app_time, gameplay_active",
        "immediate", "state"};
    static const nt_devapi_command_desc break_desc = {
        "platform.break", "game",
        "Ask the portal for a commercial break at one of the game's placements.",
        "placement",
        "requested, ad_active, breaks",
        "immediate", "state"};
    static const nt_devapi_command_desc reward_desc = {
        "platform.reward", "game",
        "Ask the portal for a rewarded video at a placement and report whether it paid out.",
        "placement",
        "requested, callbacks, ad_active, rewards",
        "immediate", "state"};
    (void)nt_devapi_register(&state_desc, hooks_state, NULL);
    (void)nt_devapi_register(&attention_desc, hooks_attention, NULL);
    (void)nt_devapi_register(&break_desc, hooks_break, NULL);
    (void)nt_devapi_register(&reward_desc, hooks_reward, NULL);
}

#endif /* NT_DEVAPI_ENABLED */
