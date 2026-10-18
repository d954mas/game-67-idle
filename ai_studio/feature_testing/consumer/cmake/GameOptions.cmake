set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Studio repo root. Public games and templates sit at depth 2 (games/<id>,
# templates/template); private games sit at depth 3 (games/private/<id>).
# Detect by the engine checkout so the template can be copied verbatim.
if(NOT GAME_REPO_ROOT)
    set(GAME_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../..")
    if(NOT EXISTS "${GAME_REPO_ROOT}/external/neotolis-engine")
        set(GAME_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
    endif()
endif()
if(NOT EXISTS "${GAME_REPO_ROOT}/external/neotolis-engine")
    message(FATAL_ERROR "Cannot locate the studio repo root (external/neotolis-engine) above ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Two-build convention: the human build (native-debug, VS Code) has no DevAPI;
# agents configure their own dir (build/devapi-debug) with -DGAME_DEVAPI_ENABLED=ON
# and get a distinct window title so both can run side by side.
if(NOT DEFINED GAME_DEVAPI_ENABLED)
    set(GAME_DEVAPI_ENABLED OFF CACHE BOOL "Build the template with engine DevAPI automation support." FORCE)
endif()
if(NOT DEFINED GAME_DEBUG_SCENES_ENABLED)
    set(GAME_DEBUG_SCENES_ENABLED OFF CACHE BOOL "Compile game-owned debug and capture scenes." FORCE)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND GAME_DEBUG_SCENES_ENABLED)
    message(FATAL_ERROR "GAME_DEBUG_SCENES_ENABLED must be OFF for Release artifacts.")
endif()

option(GAME_AUDIO_BROWSER_SMOKE "Export narrow game_audio controls for a local browser smoke artifact." OFF)
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND GAME_AUDIO_BROWSER_SMOKE)
    message(FATAL_ERROR "GAME_AUDIO_BROWSER_SMOKE must be OFF for Release artifacts.")
endif()

set(GAME_PLATFORM_TARGET_CONFIG "${GAME_REPO_ROOT}/features/platform-sdk/publish-targets/targets.json")
if(NOT EXISTS "${GAME_PLATFORM_TARGET_CONFIG}")
    message(FATAL_ERROR "Platform target descriptor is missing: ${GAME_PLATFORM_TARGET_CONFIG}")
endif()
file(READ "${GAME_PLATFORM_TARGET_CONFIG}" GAME_PLATFORM_TARGET_CONFIG_JSON)
string(JSON GAME_PLATFORM_TARGET_SCHEMA ERROR_VARIABLE GAME_PLATFORM_TARGET_SCHEMA_ERROR
    GET "${GAME_PLATFORM_TARGET_CONFIG_JSON}" schema)
if(GAME_PLATFORM_TARGET_SCHEMA_ERROR OR NOT GAME_PLATFORM_TARGET_SCHEMA STREQUAL "ai_studio.platform_target_descriptors.v1")
    message(FATAL_ERROR "Platform target descriptor schema is invalid")
endif()
string(JSON GAME_PLATFORM_TARGET_COUNT ERROR_VARIABLE GAME_PLATFORM_TARGET_COUNT_ERROR
    LENGTH "${GAME_PLATFORM_TARGET_CONFIG_JSON}" targets)
if(GAME_PLATFORM_TARGET_COUNT_ERROR OR GAME_PLATFORM_TARGET_COUNT LESS 1)
    message(FATAL_ERROR "Platform target descriptor has no targets")
endif()
math(EXPR GAME_PLATFORM_TARGET_LAST "${GAME_PLATFORM_TARGET_COUNT} - 1")
set(GAME_PLATFORM_TARGETS)
foreach(GAME_PLATFORM_TARGET_INDEX RANGE ${GAME_PLATFORM_TARGET_LAST})
    string(JSON GAME_PLATFORM_TARGET_NAME ERROR_VARIABLE GAME_PLATFORM_TARGET_NAME_ERROR
        MEMBER "${GAME_PLATFORM_TARGET_CONFIG_JSON}" targets ${GAME_PLATFORM_TARGET_INDEX})
    if(GAME_PLATFORM_TARGET_NAME_ERROR OR NOT GAME_PLATFORM_TARGET_NAME MATCHES "^[a-z][a-z0-9-]*$")
        message(FATAL_ERROR "Platform target descriptor has an invalid target name")
    endif()
    list(APPEND GAME_PLATFORM_TARGETS "${GAME_PLATFORM_TARGET_NAME}")
endforeach()

set(GAME_PUBLISH_TARGET "local" CACHE STRING "Publish target selected from the platform target descriptor.")
set_property(CACHE GAME_PUBLISH_TARGET PROPERTY STRINGS ${GAME_PLATFORM_TARGETS})
list(FIND GAME_PLATFORM_TARGETS "${GAME_PUBLISH_TARGET}" GAME_PLATFORM_TARGET_INDEX)
if(GAME_PLATFORM_TARGET_INDEX LESS 0)
    message(FATAL_ERROR "GAME_PUBLISH_TARGET must be one of: ${GAME_PLATFORM_TARGETS}")
endif()

macro(game_platform_descriptor_value OUTPUT KEY)
    string(JSON ${OUTPUT} ERROR_VARIABLE GAME_PLATFORM_DESCRIPTOR_ERROR
        GET "${GAME_PLATFORM_TARGET_CONFIG_JSON}" targets "${GAME_PUBLISH_TARGET}" "${KEY}")
    if(GAME_PLATFORM_DESCRIPTOR_ERROR)
        message(FATAL_ERROR "Platform target ${GAME_PUBLISH_TARGET} is missing ${KEY}")
    endif()
endmacro()

macro(game_platform_descriptor_bool OUTPUT KEY)
    game_platform_descriptor_value(GAME_PLATFORM_DESCRIPTOR_BOOL "${KEY}")
    if(GAME_PLATFORM_DESCRIPTOR_BOOL STREQUAL "ON")
        set(${OUTPUT} 1)
    elseif(GAME_PLATFORM_DESCRIPTOR_BOOL STREQUAL "OFF")
        set(${OUTPUT} 0)
    else()
        message(FATAL_ERROR "Platform target ${GAME_PUBLISH_TARGET} has invalid ${KEY}")
    endif()
endmacro()

game_platform_descriptor_value(GAME_PLATFORM_SDK adapter)
game_platform_descriptor_value(GAME_PLATFORM_TARGET_ID target_id)
game_platform_descriptor_value(GAME_PLATFORM_SDK_ID sdk_id)
game_platform_descriptor_bool(GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED external_links_allowed)
game_platform_descriptor_bool(GAME_PLATFORM_ADS_SUPPORTED ads_supported)
game_platform_descriptor_bool(GAME_PLATFORM_REWARDED_SUPPORTED rewarded_supported)
game_platform_descriptor_bool(GAME_PLATFORM_STORAGE_SUPPORTED storage_supported)
set(GAME_RUNTIME_BUILD_FINGERPRINT "" CACHE STRING "Runtime build fingerprint supplied by tools/build_web.mjs.")
set(GAME_RUNTIME_BUILD_PROFILE "" CACHE STRING "Web preset bound into the runtime build witness.")
set(GAME_RUNTIME_BUILD_DEBUG_UI "" CACHE STRING "Debug UI flag bound into the runtime build witness.")
set(GAME_RUNTIME_BUILD_DEVAPI "" CACHE STRING "DevAPI flag bound into the runtime build witness.")
set(GAME_RUNTIME_BUILD_ANALYTICS "" CACHE STRING "Analytics flag bound into the runtime build witness.")
set(GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR "" CACHE STRING "Event log mirror flag bound into the runtime build witness.")
if(EMSCRIPTEN AND (NOT GAME_RUNTIME_BUILD_PROFILE MATCHES "^(wasm-release|wasm-debug|wasm-devapi-debug)$"
        OR NOT GAME_RUNTIME_BUILD_DEBUG_UI MATCHES "^[01]$"
        OR NOT GAME_RUNTIME_BUILD_DEVAPI MATCHES "^[01]$"
        OR NOT GAME_RUNTIME_BUILD_ANALYTICS MATCHES "^[01]$"
        OR NOT GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR MATCHES "^[01]$"))
    message(FATAL_ERROR "Emscripten builds must declare the runtime build profile through tools/build_web.mjs.")
endif()
string(LENGTH "${GAME_RUNTIME_BUILD_FINGERPRINT}" GAME_RUNTIME_BUILD_FINGERPRINT_LENGTH)
set(GAME_RUNTIME_BUILD_FINGERPRINT_VALID OFF)
if(GAME_RUNTIME_BUILD_FINGERPRINT_LENGTH EQUAL 64 AND GAME_RUNTIME_BUILD_FINGERPRINT MATCHES "^[0-9a-f]+$")
    set(GAME_RUNTIME_BUILD_FINGERPRINT_VALID ON)
endif()
if(GAME_RUNTIME_BUILD_FINGERPRINT AND NOT GAME_RUNTIME_BUILD_FINGERPRINT_VALID)
    message(FATAL_ERROR "GAME_RUNTIME_BUILD_FINGERPRINT must be a lowercase SHA-256 supplied by tools/build_web.mjs.")
endif()
if(EMSCRIPTEN AND NOT GAME_RUNTIME_BUILD_FINGERPRINT_VALID)
    message(FATAL_ERROR "Emscripten builds must be owned by tools/build_web.mjs with a runtime fingerprint.")
endif()

if(EMSCRIPTEN)
    set(_game_default_em_cache "${CMAKE_SOURCE_DIR}/build/emscripten-cache")
    if(DEFINED ENV{EM_CACHE} AND NOT "$ENV{EM_CACHE}" STREQUAL "")
        set(_game_default_em_cache "$ENV{EM_CACHE}")
    endif()
    set(GAME_EMSCRIPTEN_CACHE_DIR "${_game_default_em_cache}" CACHE PATH "Emscripten cache directory for this game checkout.")
    file(MAKE_DIRECTORY "${GAME_EMSCRIPTEN_CACHE_DIR}")
    set(ENV{EM_CACHE} "${GAME_EMSCRIPTEN_CACHE_DIR}")
    set(_game_emcache_launcher "\"${CMAKE_COMMAND}\" -E env \"EM_CACHE=${GAME_EMSCRIPTEN_CACHE_DIR}\"")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE "${_game_emcache_launcher}")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK "${_game_emcache_launcher}")
endif()

# Preset name must encode the devapi split: engine libs land in
# build/engine/<preset> and build/tools/builder/<preset>, so two builds sharing
# one preset would overwrite each other's engine libs (nt_input with/without
# inject symbols; nt_devapi_web presence) — the parallel-build collision the
# convention exists to prevent. Every web (publish-target x devapi x build-type)
# gets a UNIQUE name, so no combination can alias into another's engine dir.
# Local keeps the legacy wasm preset names because it is the default developer
# path; portal targets append the publish target.
if(EMSCRIPTEN)
    if(GAME_DEVAPI_ENABLED)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            set(NT_PRESET_NAME "wasm-devapi-release")
        else()
            set(NT_PRESET_NAME "wasm-devapi-debug")
        endif()
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(NT_PRESET_NAME "wasm-release")
    else()
        set(NT_PRESET_NAME "wasm-debug")
    endif()
else()
    if(GAME_DEVAPI_ENABLED)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            set(NT_PRESET_NAME "native-devapi-release")
        else()
            set(NT_PRESET_NAME "devapi-debug")
        endif()
    elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(NT_PRESET_NAME "native-release")
    else()
        set(NT_PRESET_NAME "native-debug")
    endif()
endif()
if(EMSCRIPTEN AND NOT GAME_PUBLISH_TARGET STREQUAL "local")
    set(NT_PRESET_NAME "${NT_PRESET_NAME}-${GAME_PUBLISH_TARGET}")
endif()
set(GAME_TITLE "Template" CACHE STRING "Game window title base")
if(GAME_DEVAPI_ENABLED)
    set(GAME_WINDOW_TITLE "${GAME_TITLE} [AI]")
else()
    set(GAME_WINDOW_TITLE "${GAME_TITLE}")
endif()
if(GAME_DEVAPI_ENABLED)
    set(NT_DEVAPI_ENABLED ON CACHE BOOL "Build the engine DevAPI command layer." FORCE)
    set(NT_UI_DEBUG_TOOLS ON CACHE BOOL "Build UI probe data for engine DevAPI ui.* commands." FORCE)
    set(NT_LOG_RING_ENABLED ON CACHE BOOL "Build log ring data for engine DevAPI obs commands." FORCE)
    set(NT_METRICS_ENABLED ON CACHE BOOL "Build metrics data for engine DevAPI obs commands." FORCE)
    set(NT_INTROSPECT_ENABLED ON CACHE BOOL "Build entity introspection for engine DevAPI obs commands." FORCE)
    set(NT_DEVAPI_GROUP_UI ON CACHE BOOL "Build engine DevAPI ui.* commands." FORCE)
    set(NT_DEVAPI_GROUP_OBS ON CACHE BOOL "Build engine DevAPI log/perf/entity/resource commands." FORCE)
    set(NT_DEVAPI_GROUP_CAPTURE ON CACHE BOOL "Build engine DevAPI capture.frame/capture.region commands." FORCE)
else()
    set(NT_DEVAPI_ENABLED OFF CACHE BOOL "Build the engine DevAPI command layer." FORCE)
    set(NT_UI_DEBUG_TOOLS OFF CACHE BOOL "Build UI probe data for engine DevAPI ui.* commands." FORCE)
    set(NT_LOG_RING_ENABLED OFF CACHE BOOL "Build log ring data for engine DevAPI obs commands." FORCE)
    set(NT_METRICS_ENABLED OFF CACHE BOOL "Build metrics data for engine DevAPI obs commands." FORCE)
    set(NT_INTROSPECT_ENABLED OFF CACHE BOOL "Build entity introspection for engine DevAPI obs commands." FORCE)
    foreach(_grp CORE TIME INPUT DISCOVERY UI OBS ENTITY_WRITE CAPTURE)
        set(NT_DEVAPI_GROUP_${_grp} OFF CACHE BOOL "Disabled with GAME_DEVAPI_ENABLED=OFF." FORCE)
    endforeach()
endif()

# E4: GAME_ANALYTICS_ENABLED gates the local analytics writer. OPT-IN only
# (-D override wins): it renders and writes an NDJSON line for EVERY event
# from inside the sim tick — telemetry sessions ask for it explicitly,
# automation farms never need it.
if(NOT DEFINED GAME_ANALYTICS_ENABLED)
    set(GAME_ANALYTICS_ENABLED OFF)
endif()
# Raw full-stream mirror is OPT-IN only: it prints EVERY event every tick
# (synchronous stdout inside the sim tick), which floods automated runs and
# taxes the tick. A game that wants a console voice adds a curated shortlog.
if(NOT DEFINED GAME_EVENTS_LOG_MIRROR)
    set(GAME_EVENTS_LOG_MIRROR OFF)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND
        (GAME_ANALYTICS_ENABLED OR GAME_EVENTS_LOG_MIRROR))
    message(FATAL_ERROR "GAME_ANALYTICS_ENABLED and GAME_EVENTS_LOG_MIRROR must be OFF for Release artifacts.")
endif()

if(NOT DEFINED GAME_PLATFORM_SDK_DEBUG_UI)
    # OFF in every build a player can see, debug included: the probe panel is a
    # developer instrument that covers the game behind it, and DevAPI
    # (game.platform_sdk.state) already reports the same state without taking
    # the screen. Turn it on deliberately with -DGAME_PLATFORM_SDK_DEBUG_UI=ON.
    set(GAME_PLATFORM_SDK_DEBUG_UI OFF)
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Release" AND GAME_PLATFORM_SDK_DEBUG_UI)
    message(FATAL_ERROR "GAME_PLATFORM_SDK_DEBUG_UI must be OFF for Release artifacts.")
endif()

if(EMSCRIPTEN AND ((GAME_RUNTIME_BUILD_DEBUG_UI AND NOT GAME_PLATFORM_SDK_DEBUG_UI)
        OR (GAME_PLATFORM_SDK_DEBUG_UI AND NOT GAME_RUNTIME_BUILD_DEBUG_UI)
        OR (GAME_RUNTIME_BUILD_DEVAPI AND NOT GAME_DEVAPI_ENABLED)
        OR (GAME_DEVAPI_ENABLED AND NOT GAME_RUNTIME_BUILD_DEVAPI)
        OR (GAME_RUNTIME_BUILD_ANALYTICS AND NOT GAME_ANALYTICS_ENABLED)
        OR (GAME_ANALYTICS_ENABLED AND NOT GAME_RUNTIME_BUILD_ANALYTICS)
        OR (GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR AND NOT GAME_EVENTS_LOG_MIRROR)
        OR (GAME_EVENTS_LOG_MIRROR AND NOT GAME_RUNTIME_BUILD_EVENTS_LOG_MIRROR)))
    message(FATAL_ERROR "Runtime build profile flags do not match the configured compiler flags.")
endif()
