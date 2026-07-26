cmake_minimum_required(VERSION 3.24)

project(ai_studio_windows_process_loopback LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "windows_process_loopback is Windows-only")
endif()

add_executable(windows_process_loopback windows_process_loopback.cpp)
target_compile_features(windows_process_loopback PRIVATE cxx_std_20)
target_compile_definitions(
  windows_process_loopback
  PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    UNICODE
    _UNICODE
)
target_compile_options(
  windows_process_loopback
  PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
    $<$<CXX_COMPILER_ID:MSVC>:/WX>
    $<$<CXX_COMPILER_ID:MSVC>:/permissive->
)
target_link_libraries(
  windows_process_loopback
  PRIVATE
    mmdevapi
    ole32
    uuid
)
