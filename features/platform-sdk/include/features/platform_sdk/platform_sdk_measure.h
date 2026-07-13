#ifndef FEATURES_PLATFORM_SDK_PLATFORM_SDK_MEASURE_H
#define FEATURES_PLATFORM_SDK_PLATFORM_SDK_MEASURE_H

#include "features/platform_sdk/platform_sdk.h"

/* Analytics-sink seam for event bridges. This is intentionally separate from
 * the ordinary game-facing facade: gameplay emits typed game events first. */
platform_sdk_result_t platform_sdk_measure(const char *category,
                                           const char *what,
                                           const char *action);

#endif
