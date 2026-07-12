set(AUDIO_CORE_DIR    "${CMAKE_CURRENT_SOURCE_DIR}/../../features/audio-core")
set(AUDIO_CORE_INC    "${AUDIO_CORE_DIR}/include")
set(AUDIO_CORE_SRC    "${AUDIO_CORE_DIR}/src")
set(AUDIO_CORE_WEB    "${AUDIO_CORE_DIR}/web")
set(AUDIO_CORE_VENDOR "${AUDIO_CORE_DIR}/vendor/miniaudio")

target_sources(${GAME_TARGET} PRIVATE
    src/game_audio.c
    "${AUDIO_CORE_SRC}/audio.c"
    "${AUDIO_CORE_SRC}/audio_resource.c")
if(EMSCRIPTEN)
    target_sources(${GAME_TARGET} PRIVATE "${AUDIO_CORE_SRC}/audio_backend_web.c")
else()
    find_package(Threads REQUIRED)
    function(audio_core_link_native_systems target)
        target_link_libraries(${target} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
        if(UNIX AND NOT APPLE)
            target_link_libraries(${target} PRIVATE m)
        endif()
    endfunction()
    target_sources(${GAME_TARGET} PRIVATE
        "${AUDIO_CORE_SRC}/audio_backend_miniaudio.c"
        "${AUDIO_CORE_SRC}/audio_miniaudio_impl.c")
    audio_core_link_native_systems(${GAME_TARGET})
endif()

# E4: the E3 renderer is ALSO the analytics writer's dependency. When devapi is OFF but
# analytics is ON, pull it in here -- guarded against the devapi block's copy above so it
# is never listed twice (E3 lines untouched; the renderer compiles if EITHER wants it).
if((GAME_ANALYTICS_ENABLED OR GAME_EVENTS_LOG_MIRROR) AND NOT GAME_DEVAPI_ENABLED)
    target_sources(${GAME_TARGET} PRIVATE "${GAME_EVENTS_SRC}/game_event_render.c") # E3 renderer (shared with E4/mirror)
endif()
if(GAME_ANALYTICS_ENABLED)
    target_sources(${GAME_TARGET} PRIVATE "${GAME_EVENTS_SRC}/game_analytics.c") # E4: local analytics writer
endif()
if(GAME_EVENTS_LOG_MIRROR)
    target_sources(${GAME_TARGET} PRIVATE "${GAME_EVENTS_SRC}/game_events_log_mirror.c")
endif()
target_include_directories(${GAME_TARGET} PRIVATE
    "${AUDIO_CORE_INC}" "${ITEMS_CORE_INC}" "${PROGRESSION_CORE_INC}" "${PLATFORM_SDK_INC}" "${GAME_EVENTS_INC}"
    src "${GAME_SOURCE_GENERATED_DIR}" "${ENGINE_DIR}/deps/glfw/deps")
target_compile_definitions(${GAME_TARGET} PRIVATE GAME_WINDOW_TITLE="${GAME_WINDOW_TITLE}")
target_link_libraries(${GAME_TARGET} PRIVATE
    nt_core nt_app nt_input nt_window nt_gfx nt_text_renderer nt_font
    nt_resource nt_material nt_shared nt_render nt_hash nt_fs nt_log nt_math nt_time
    nt_mesh_renderer nt_entity nt_transform_comp nt_mesh_comp nt_material_comp nt_drawable_comp
    nt_ui nt_sprite_renderer nt_atlas nt_mem_scratch nt_clipboard
)
if(EMSCRIPTEN)
    target_sources(${GAME_TARGET} PRIVATE "${PLATFORM_SDK_SRC}/platform_sdk_web.c")
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http nt_platform_web)
    # Web-devapi host contract (nt_devapi_web.h): the host exports the JS
    # transport entry points; the exports are also what pulls the EM_JS object
    # out of libnt_devapi_web.a so nt_devapi_web_install_shim resolves at link.
    set(GAME_WEB_EXPORTS "_main,_malloc,_free,_platform_sdk_web_complete_init,_platform_sdk_web_complete_interstitial,_platform_sdk_web_complete_rewarded")
    if(GAME_AUDIO_BROWSER_SMOKE)
        # Narrow opt-in browser-smoke seam; ordinary release artifacts export no audio controls.
        set(GAME_WEB_EXPORTS "${GAME_WEB_EXPORTS},_game_audio_play_cue,_game_audio_play_music,_game_audio_stop_music,_game_audio_set_enabled,_game_audio_set_paused")
    endif()
    if(GAME_DEVAPI_ENABLED)
        set(GAME_WEB_EXPORTS "${GAME_WEB_EXPORTS},_nt_devapi_web_submit,_nt_devapi_web_poll")
    endif()
    target_link_options(${GAME_TARGET} PRIVATE
        "SHELL:--js-library ${AUDIO_CORE_WEB}/audio_web.library.js"
        "SHELL:-sFORCE_FILESYSTEM=1"
        "SHELL:-sEXPORTED_FUNCTIONS=${GAME_WEB_EXPORTS}"
        "SHELL:-sMIN_WEBGL_VERSION=2"
        "SHELL:-sMAX_WEBGL_VERSION=2"
        "SHELL:-sFULL_ES3=1"
        # Engine targets set memory flags PRIVATE on static libs, which never
        # reach the executable link: without these the heap is fixed at 16MB
        # and texture transcode OOMs (Aborted(OOM) on web).
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sINITIAL_MEMORY=64MB"
    )
    if(GAME_DEVAPI_ENABLED)
        # The engine JS shim drives the transport through Module.ccall, which
        # recent emcc no longer exports by default (runtime need, not link).
        target_link_options(${GAME_TARGET} PRIVATE
            "SHELL:-sEXPORTED_RUNTIME_METHODS=ccall,UTF8ToString")
    endif()
else()
    # glad/stb_image_write are native-only (no GL loader / file dump on web).
    target_link_libraries(${GAME_TARGET} PRIVATE nt_http_stub glad stb_image_write)
endif()
if(EMSCRIPTEN)
    set(GAME_PLATFORM_SDK_RELEASE_JS "false")
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(GAME_PLATFORM_SDK_RELEASE_JS "true")
    endif()

    file(MAKE_DIRECTORY "${GAME_OUTPUT_DIR}")
    configure_file("${PLATFORM_SDK_WEB}/platform-sdk.js"
                   "${GAME_OUTPUT_DIR}/platform-sdk.js" COPYONLY)
    configure_file("${PLATFORM_SDK_WEB}/platform-sdk-core.js"
                   "${GAME_OUTPUT_DIR}/platform-sdk-core.js" COPYONLY)
    configure_file("${PLATFORM_SDK_WEB}/adapters/${GAME_PLATFORM_SDK}.js"
                   "${GAME_OUTPUT_DIR}/platform-sdk-adapter.js" COPYONLY)
    file(REMOVE "${GAME_OUTPUT_DIR}/platform-sdk-debug-ui.js")
    if(GAME_PUBLISH_TARGET STREQUAL "playgama")
        configure_file("${PLATFORM_SDK_WEB}/portal/playgama-bridge-config.json"
                       "${GAME_OUTPUT_DIR}/playgama-bridge-config.json" COPYONLY)
    else()
        file(REMOVE "${GAME_OUTPUT_DIR}/playgama-bridge-config.json")
    endif()

    add_custom_target(platform_sdk_web_assets
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_OUTPUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/platform-sdk.js" "${GAME_OUTPUT_DIR}/platform-sdk.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/platform-sdk-core.js" "${GAME_OUTPUT_DIR}/platform-sdk-core.js"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/adapters/${GAME_PLATFORM_SDK}.js" "${GAME_OUTPUT_DIR}/platform-sdk-adapter.js"
        COMMAND ${CMAKE_COMMAND} -E rm -f "${GAME_OUTPUT_DIR}/platform-sdk-debug-ui.js"
        DEPENDS
            "${PLATFORM_SDK_WEB}/platform-sdk.js"
            "${PLATFORM_SDK_WEB}/platform-sdk-core.js"
            "${PLATFORM_SDK_WEB}/adapters/${GAME_PLATFORM_SDK}.js"
        COMMENT "Staging platform-sdk web backend assets (${GAME_PUBLISH_TARGET} -> ${GAME_PLATFORM_SDK})"
        VERBATIM)
    if(GAME_PUBLISH_TARGET STREQUAL "playgama")
        add_custom_target(platform_sdk_playgama_config_asset
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PLATFORM_SDK_WEB}/portal/playgama-bridge-config.json"
                "${GAME_OUTPUT_DIR}/playgama-bridge-config.json"
            DEPENDS "${PLATFORM_SDK_WEB}/portal/playgama-bridge-config.json"
            COMMENT "Staging Playgama Bridge placeholder config"
            VERBATIM)
        add_dependencies(platform_sdk_web_assets platform_sdk_playgama_config_asset)
    endif()

    # Deliver the tracked web shell to bin/ at configure time. Two substitutions:
    # the tab title (GAME_TITLE) and Module.arguments. The engine web devapi shim
    # installs only if main() saw `--devapi <port>` in argv (main.c parses it, and
    # on web argv comes ONLY from Module.arguments); the devapi presets carry the
    # flag, the human presets (devapi OFF) expand to empty.
    if(GAME_DEVAPI_ENABLED)
        set(GAME_DEVAPI_MODULE_ARGS "arguments: ['--devapi', '17890'],")
    else()
        set(GAME_DEVAPI_MODULE_ARGS "")
    endif()
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/web/index.html.in"
                   "${GAME_OUTPUT_DIR}/index.html" @ONLY)
endif()
if(GAME_DEVAPI_ENABLED)
    # E3: readable event type/hash labels for game.events.tail. Consuming-CMake edit
    # (engine tree untouched); devapi presets only -> no collision with native-debug's
    # clean nt_hash (each preset has its own build/engine/<preset> tree).
    target_compile_definitions(nt_hash PRIVATE NT_HASH_LABELS=1)
    if(EMSCRIPTEN)
        set(GAME_DEVAPI_TRANSPORT nt_devapi_web nt_platform_web)
    else()
        set(GAME_DEVAPI_TRANSPORT nt_devapi_net)
    endif()
    target_link_libraries(${GAME_TARGET} PRIVATE
        ${GAME_DEVAPI_TRANSPORT}
        nt_devapi_default
        nt_platform
        nt_metrics
        nt_log_ring
    )
endif()
if(WIN32 AND NOT EMSCRIPTEN)
    target_link_libraries(${GAME_TARGET} PRIVATE winmm)
endif()
# On web the engine streams packs over HTTP relative to the page URL, so the
# path must be RELATIVE and the pack ships as a plain file next to index.html
# (tools/build_web.sh copies it from the native build). Native keeps the
# absolute bin/assets path.
if(EMSCRIPTEN)
    set(GAME_PACK_RUNTIME_PATH "assets/game.ntpack")
else()
    set(GAME_PACK_RUNTIME_PATH "${GAME_ASSETS_DIR}/game.ntpack")
endif()
target_compile_definitions(${GAME_TARGET} PRIVATE
    GAME_ASSET_PACK_PATH="${GAME_PACK_RUNTIME_PATH}"
    PLATFORM_SDK_TARGET_ID=${GAME_PLATFORM_TARGET_ID}
    PLATFORM_SDK_CURRENT_ID=${GAME_PLATFORM_SDK_ID}
    PLATFORM_SDK_EXTERNAL_LINKS_ALLOWED=${GAME_PLATFORM_EXTERNAL_LINKS_ALLOWED}
    PLATFORM_SDK_ADS_SUPPORTED=${GAME_PLATFORM_ADS_SUPPORTED}
    PLATFORM_SDK_REWARDED_SUPPORTED=${GAME_PLATFORM_REWARDED_SUPPORTED}
    PLATFORM_SDK_STORAGE_SUPPORTED=${GAME_PLATFORM_STORAGE_SUPPORTED}
    GAME_PLATFORM_SDK_DEBUG_UI=$<BOOL:${GAME_PLATFORM_SDK_DEBUG_UI}>
    NT_INTROSPECT_ENABLED=$<BOOL:${NT_INTROSPECT_ENABLED}>
    NT_INTROSPECT_WRITE_ENABLED=$<BOOL:${NT_INTROSPECT_WRITE_ENABLED}>
    _CRT_SECURE_NO_WARNINGS
)
# E4: FEATURE_GAME_ANALYTICS is defined ALWAYS (1/0) at target level -- game_features.c and
# main.c carry #if FEATURE_GAME_ANALYTICS unconditionally, and an undefined macro under
# -Wundef + -Werror would fail native-debug/release. Mirrors NT_DEVAPI_ENABLED=0.
target_compile_definitions(${GAME_TARGET} PRIVATE
    FEATURE_GAME_ANALYTICS=$<BOOL:${GAME_ANALYTICS_ENABLED}>
    FEATURE_GAME_EVENTS=1
    GAME_EVENTS_LOG_MIRROR=$<BOOL:${GAME_EVENTS_LOG_MIRROR}>)
# GAME_STORAGE_APP_ID namespaces the shared web-origin (itch) localStorage; "template" is
# the template's own default, games override it (A2).
target_compile_definitions(${GAME_TARGET} PRIVATE GAME_STORAGE_APP_ID="template")
if(NOT GAME_DEVAPI_ENABLED)
    target_compile_definitions(${GAME_TARGET} PRIVATE NT_DEVAPI_ENABLED=0)
endif()
nt_set_warning_flags(${GAME_TARGET})
# Match the sanitizer posture of the engine archives we link: the engine
# instruments Debug modules (non-Windows/Clang targets, incl. wasm) and expects
# the consumer executable to carry the same flags -- without this the Debug
# wasm link dies on undefined __asan_*/__ubsan_* (lead decision 2026-07-07).
nt_set_sanitizer_flags(${GAME_TARGET})
set_target_properties(${GAME_TARGET} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${GAME_OUTPUT_DIR}")

if(GAME_DEVAPI_ENABLED AND NOT EMSCRIPTEN)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_custom_target(devapi_smoke
            COMMAND ${CMAKE_COMMAND} -E env
                "AI_STUDIO_GAME_EXE=$<TARGET_FILE:${GAME_TARGET}>"
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/devapi/smoke_bot.py"
                "--exe" "$<TARGET_FILE:${GAME_TARGET}>"
            DEPENDS ${GAME_TARGET}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
            COMMENT "Running template DevAPI smoke bot"
            VERBATIM
        )
        add_custom_target(quality_responsive
            COMMAND ${CMAKE_COMMAND} -E env
                "AI_STUDIO_GAME_EXE=$<TARGET_FILE:${GAME_TARGET}>"
                "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/devapi/responsive_viewports.py"
                "--exe" "$<TARGET_FILE:${GAME_TARGET}>"
            DEPENDS ${GAME_TARGET}
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../.."
            COMMENT "Capturing QCLR_002 responsive viewport evidence"
            VERBATIM
        )
    endif()
endif()
