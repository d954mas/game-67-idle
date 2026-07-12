cmake_minimum_required(VERSION 3.25)
project(audio_core_benchmark LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(NT_REPO_ROOT "" CACHE PATH "Absolute path to the game-67-idle checkout")
if(NOT IS_ABSOLUTE "${NT_REPO_ROOT}")
    message(FATAL_ERROR "NT_REPO_ROOT must be an absolute checkout path")
endif()

set(AUDIO_CORE_DIR "${NT_REPO_ROOT}/features/audio-core")
set(ENGINE_DIR "${NT_REPO_ROOT}/external/neotolis-engine")

add_executable(audio_benchmark_native
    native_latency.c
    "${AUDIO_CORE_DIR}/src/audio.c"
    "${AUDIO_CORE_DIR}/src/audio_backend_miniaudio.c"
    "${AUDIO_CORE_DIR}/src/audio_miniaudio_impl.c")

target_include_directories(audio_benchmark_native PRIVATE
    "${AUDIO_CORE_DIR}/include"
    "${AUDIO_CORE_DIR}/src"
    "${AUDIO_CORE_DIR}/vendor/miniaudio"
    "${ENGINE_DIR}/engine")
target_compile_definitions(audio_benchmark_native PRIVATE
    _CRT_SECURE_NO_WARNINGS
    NT_ASSERT_MODE=1)

find_package(Threads REQUIRED)
target_link_libraries(audio_benchmark_native PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(UNIX AND NOT APPLE)
    target_link_libraries(audio_benchmark_native PRIVATE m)
endif()
