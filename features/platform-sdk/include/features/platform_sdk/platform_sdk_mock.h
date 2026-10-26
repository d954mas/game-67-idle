#ifndef FEATURES_PLATFORM_SDK_PLATFORM_SDK_MOCK_H
#define FEATURES_PLATFORM_SDK_PLATFORM_SDK_MOCK_H

#include "features/platform_sdk/platform_sdk.h"

#include <stdbool.h>

/* A native local build has no portal to show an ad or a login dialog, so this
   stands one in that a developer answers by hand: each request opens a flow,
   the game's debug UI draws it and calls one answer below. On every other
   build the flow stays idle and the answers do nothing. */
typedef enum platform_sdk_mock_flow_t {
    PLATFORM_SDK_MOCK_IDLE = 0,
    PLATFORM_SDK_MOCK_INTERSTITIAL,
    PLATFORM_SDK_MOCK_REWARDED,
    PLATFORM_SDK_MOCK_LOGIN,
} platform_sdk_mock_flow_t;

/* Installs the hand-answered backend where it applies, then boots the facade
   when the selected adapter is the mock one. */
void platform_sdk_mock_init(void);
/* Destroys the facade. */
void platform_sdk_mock_shutdown(void);

platform_sdk_mock_flow_t platform_sdk_mock_flow(void);
/* The placement of the open ad flow; "default" when the game passed none. */
const char *platform_sdk_mock_placement(void);

/* Each answer closes the open flow and returns what it delivered to the
   facade, so a panel can print it. */
platform_sdk_ad_result_t platform_sdk_mock_close_interstitial(void);
/* COMPLETED pays out; SKIPPED was shown and pays nothing; DECLINED and FAILED
   were never shown. */
platform_sdk_rewarded_result_t platform_sdk_mock_answer_rewarded(platform_sdk_ad_reason_t reason);
platform_sdk_auth_result_t platform_sdk_mock_answer_login(bool accepted);

#endif /* FEATURES_PLATFORM_SDK_PLATFORM_SDK_MOCK_H */
