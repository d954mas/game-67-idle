cmake_minimum_required(VERSION 3.25)

if(NOT SKELETAL_ANIMATION_OZZ_SOURCE_DIR)
    message(FATAL_ERROR "SKELETAL_ANIMATION_OZZ_SOURCE_DIR is required for skeletal_animation_extension.")
endif()
if(NOT EXISTS "${SKELETAL_ANIMATION_OZZ_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "SKELETAL_ANIMATION_OZZ_SOURCE_DIR does not point to an ozz-animation source directory: ${SKELETAL_ANIMATION_OZZ_SOURCE_DIR}")
endif()

enable_language(CXX)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
if(WIN32)
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:>")
endif()
set(ozz_build_tools ON CACHE BOOL "" FORCE)
set(ozz_build_fbx OFF CACHE BOOL "" FORCE)
set(ozz_build_gltf ON CACHE BOOL "" FORCE)
set(ozz_build_data OFF CACHE BOOL "" FORCE)
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
set(ozz_build_msvc_rt_dll OFF CACHE BOOL "" FORCE)
add_subdirectory("${SKELETAL_ANIMATION_OZZ_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/external/ozz-animation")

add_library(skeletal_animation_extension STATIC
    src/nt_skeletal_animation_ozz.cpp
    src/nt_skeletal_mesh_cpu.cpp
    include/skeletal_animation/nt_skeletal_animation.h
    include/skeletal_animation/nt_skeletal_mesh.h
)
target_include_directories(skeletal_animation_extension
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(skeletal_animation_extension PRIVATE ozz_animation)
target_compile_features(skeletal_animation_extension PUBLIC cxx_std_17)
target_compile_definitions(skeletal_animation_extension PRIVATE _CRT_SECURE_NO_WARNINGS)
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(skeletal_animation_extension)
endif()
if(COMMAND nt_set_sanitizer_flags)
    nt_set_sanitizer_flags(skeletal_animation_extension)
endif()
target_compile_options(skeletal_animation_extension PRIVATE
    -Wno-error=format-nonliteral
    -Wno-error=sign-conversion
    -Wno-format-nonliteral
    -Wno-sign-conversion
)

add_executable(skeletal_animation_ozz_probe
    tools/skeletal_animation_ozz_probe.c
)
target_link_libraries(skeletal_animation_ozz_probe PRIVATE skeletal_animation_extension)
target_compile_definitions(skeletal_animation_ozz_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(skeletal_animation_ozz_probe)
endif()
if(COMMAND nt_set_sanitizer_flags)
    nt_set_sanitizer_flags(skeletal_animation_ozz_probe)
endif()
set_target_properties(skeletal_animation_ozz_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/tools/${NT_PRESET_NAME}"
)

add_executable(skeletal_mesh_contract_probe
    tools/skeletal_mesh_contract_probe.c
)
target_link_libraries(skeletal_mesh_contract_probe PRIVATE skeletal_animation_extension)
target_compile_definitions(skeletal_mesh_contract_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(skeletal_mesh_contract_probe)
endif()
if(COMMAND nt_set_sanitizer_flags)
    nt_set_sanitizer_flags(skeletal_mesh_contract_probe)
endif()
set_target_properties(skeletal_mesh_contract_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build/tools/${NT_PRESET_NAME}"
)
if(WIN32)
    set_target_properties(skeletal_animation_extension skeletal_animation_ozz_probe skeletal_mesh_contract_probe PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:>"
    )
endif()
