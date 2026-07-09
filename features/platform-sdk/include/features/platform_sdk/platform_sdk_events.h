#ifndef FEATURES_PLATFORM_SDK_PLATFORM_SDK_EVENTS_H
#define FEATURES_PLATFORM_SDK_PLATFORM_SDK_EVENTS_H

#ifndef FEATURE_GAME_EVENTS
#define FEATURE_GAME_EVENTS 0
#endif

#if FEATURE_GAME_EVENTS

#include <stdint.h>

#include "game_event_desc.h"
#include "hash/nt_hash.h"

typedef struct platform_sdk_ev_platform_ready_t {
    unsigned char ready;
} platform_sdk_ev_platform_ready_t;

typedef struct platform_sdk_ev_placement_t {
    uint32_t placement;
} platform_sdk_ev_placement_t;

typedef struct platform_sdk_ev_interstitial_result_t {
    unsigned char supported;
    unsigned char shown;
    uint32_t placement;
    uint32_t reason;
} platform_sdk_ev_interstitial_result_t;

typedef struct platform_sdk_ev_rewarded_result_t {
    unsigned char supported;
    unsigned char shown;
    unsigned char rewarded;
    uint32_t placement;
    uint32_t reason;
} platform_sdk_ev_rewarded_result_t;

nt_hash64_t platform_sdk_ev_platform_ready_type(void);
nt_hash64_t platform_sdk_ev_game_loading_finished_type(void);
nt_hash64_t platform_sdk_ev_gameplay_start_type(void);
nt_hash64_t platform_sdk_ev_gameplay_stop_type(void);
nt_hash64_t platform_sdk_ev_interstitial_request_type(void);
nt_hash64_t platform_sdk_ev_interstitial_result_type(void);
nt_hash64_t platform_sdk_ev_rewarded_request_type(void);
nt_hash64_t platform_sdk_ev_rewarded_result_type(void);

extern const game_event_desc_t platform_sdk_ev_platform_ready_desc;
extern const game_event_desc_t platform_sdk_ev_game_loading_finished_desc;
extern const game_event_desc_t platform_sdk_ev_gameplay_start_desc;
extern const game_event_desc_t platform_sdk_ev_gameplay_stop_desc;
extern const game_event_desc_t platform_sdk_ev_interstitial_request_desc;
extern const game_event_desc_t platform_sdk_ev_interstitial_result_desc;
extern const game_event_desc_t platform_sdk_ev_rewarded_request_desc;
extern const game_event_desc_t platform_sdk_ev_rewarded_result_desc;
extern const game_event_desc_t *const platform_sdk_ev_descs[];
extern const int platform_sdk_ev_desc_count;

void platform_sdk_events_register(void);

#endif /* FEATURE_GAME_EVENTS */

#endif /* FEATURES_PLATFORM_SDK_PLATFORM_SDK_EVENTS_H */
