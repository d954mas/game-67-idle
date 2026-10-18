# Call after game_add_c_test and the game-events/platform-sdk CMake variables exist.
function(portal_metrics_add_core_test)
    game_add_c_test(test_portal_metrics
        SOURCES "${PORTAL_METRICS_DIR}/tests/test_portal_metrics.c"
                "${PORTAL_METRICS_SRC}/sys_portal_metrics.c"
                "${GAME_EVENTS_SRC}/game_events.c"
                "${PLATFORM_SDK_SRC}/platform_sdk.c"
        LIBS nt_hash nt_log nt_core
        INCLUDES "${PORTAL_METRICS_INC}" "${GAME_EVENTS_INC}" "${PLATFORM_SDK_INC}"
        DEFINES NT_DEVAPI_ENABLED=0
                PLATFORM_SDK_TARGET_ID=${GAME_PLATFORM_TARGET_ID}
                PLATFORM_SDK_CURRENT_ID=${GAME_PLATFORM_SDK_ID}
                PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED=${GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED}
                PLATFORM_SDK_ADS_SUPPORTED=${GAME_PLATFORM_ADS_SUPPORTED}
                PLATFORM_SDK_REWARDED_SUPPORTED=${GAME_PLATFORM_REWARDED_SUPPORTED}
                PLATFORM_SDK_STORAGE_SUPPORTED=${GAME_PLATFORM_STORAGE_SUPPORTED}
                PLATFORM_SDK_TESTING=1 FEATURE_GAME_EVENTS=1
        TIER core
        WARNINGS)
endfunction()
