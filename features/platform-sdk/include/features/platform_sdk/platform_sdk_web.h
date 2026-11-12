#ifndef FEATURES_PLATFORM_SDK_PLATFORM_SDK_WEB_H
#define FEATURES_PLATFORM_SDK_PLATFORM_SDK_WEB_H

#include "features/platform_sdk/platform_sdk.h"

/* Every web part below plus the account storage (platform_sdk_cloud). */
void platform_sdk_install_web_backend(void);

/* The same backend in parts, so a game links only the calls it makes: fill a
   zeroed backend with the parts it uses, then platform_sdk_install_web().
   core: init, locale, loading, gameplay, measure, sound switches, rewarded
   ads, and the portal's pause, resume and audio hooks.
   social: login, the signed-in player, leaderboards. */
void platform_sdk_web_backend_core(platform_sdk_backend_t *backend);
void platform_sdk_web_backend_social(platform_sdk_backend_t *backend);
void platform_sdk_web_backend_banner(platform_sdk_backend_t *backend);
void platform_sdk_web_backend_interstitial(platform_sdk_backend_t *backend);
void platform_sdk_install_web(const platform_sdk_backend_t *backend);

/* The core part alone. */
void platform_sdk_install_web_backend_core(void);

#endif /* FEATURES_PLATFORM_SDK_PLATFORM_SDK_WEB_H */
