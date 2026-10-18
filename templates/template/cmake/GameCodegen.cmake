if(CMAKE_HOST_WIN32)
    set(STUDIO_PYTHON "${GAME_REPO_ROOT}/.venv/Scripts/python.exe")
else()
    set(STUDIO_PYTHON "${GAME_REPO_ROOT}/.venv/bin/python")
endif()
if(NOT EXISTS "${STUDIO_PYTHON}")
    message(FATAL_ERROR "Studio Python is missing at ${STUDIO_PYTHON}; run node ai_studio/dev_environment/python_setup.mjs")
endif()
set(Python3_EXECUTABLE "${STUDIO_PYTHON}" CACHE FILEPATH "Studio root venv Python" FORCE)
find_package(Python3 3.12 EXACT COMPONENTS Interpreter REQUIRED)

set(GAME_STATE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/state/game_state.schema.json")
set(GAME_STATE_GENERATOR "${GAME_STATE_DIR}/scripts/generate_state.py")
set(GAME_STATE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated/game-state")
set(GAME_STATE_GENERATED_SOURCE "${GAME_STATE_GENERATED_DIR}/game_state.c")
set(GAME_STATE_GENERATED_HEADER "${GAME_STATE_GENERATED_DIR}/game_state.h")
add_custom_command(
    OUTPUT "${GAME_STATE_GENERATED_HEADER}" "${GAME_STATE_GENERATED_SOURCE}"
        "${GAME_STATE_GENERATED_DIR}/game_state_schema.gen.h"
        "${GAME_STATE_GENERATED_DIR}/game_state_events.gen.h"
        "${GAME_STATE_GENERATED_DIR}/game_state_events.gen.c"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${GAME_STATE_GENERATED_DIR}"
    COMMAND "${Python3_EXECUTABLE}" "${GAME_STATE_GENERATOR}"
        --schema "${GAME_STATE_SCHEMA}" --out-dir "${GAME_STATE_GENERATED_DIR}" --fragment game
    DEPENDS "${GAME_STATE_SCHEMA}" "${GAME_STATE_GENERATOR}"
    WORKING_DIRECTORY "${GAME_REPO_ROOT}"
    VERBATIM)

if(EMSCRIPTEN)
    set(GAME_SAVE_PLATFORM_SOURCES
        "${GAME_STATE_SRC}/game_save_platform_web.c"
        "${GAME_STATE_SRC}/game_storage_backend_web.c"
        "${GAME_STATE_SRC}/game_storage_web.c")
else()
    set(GAME_SAVE_PLATFORM_SOURCES
        "${GAME_STATE_SRC}/game_save_platform_native.c"
        "${GAME_STATE_SRC}/game_storage_backend_native.c")
endif()
target_sources(${GAME_TARGET} PRIVATE
    "${GAME_STATE_GENERATED_SOURCE}"
    "${GAME_STATE_SRC}/game_state_json.c"
    "${GAME_STATE_SRC}/game_save_writer.c"
    "${GAME_STATE_SRC}/game_save_text.c"
    "${GAME_STATE_SRC}/game_save_text_json.c"
    "${GAME_STATE_SRC}/game_storage.c"
    "${GAME_STATE_SRC}/game_save.c"
    ${GAME_SAVE_PLATFORM_SOURCES})
target_include_directories(${GAME_TARGET} PRIVATE "${GAME_STATE_GENERATED_DIR}")
