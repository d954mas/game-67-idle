cmake_minimum_required(VERSION 3.25)
option(NT_ENABLE_EXPERIMENTAL_SKELETAL_ANIMATION
       "Build the incomplete experimental skeletal-animation proof" OFF)

if(NOT NT_ENABLE_EXPERIMENTAL_SKELETAL_ANIMATION)
    message(FATAL_ERROR
        "skeletal_animation is incomplete experimental code; configure with "
        "-DNT_ENABLE_EXPERIMENTAL_SKELETAL_ANIMATION=ON to opt in explicitly.")
endif()

project(ai_studio_experimental_skeletal_animation LANGUAGES C CXX)

if(NOT SKELETAL_ANIMATION_OZZ_SOURCE_DIR)
    message(FATAL_ERROR "SKELETAL_ANIMATION_OZZ_SOURCE_DIR is required for skeletal_animation_extension.")
endif()
if(NOT EXISTS "${SKELETAL_ANIMATION_OZZ_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "SKELETAL_ANIMATION_OZZ_SOURCE_DIR does not point to an ozz-animation source directory: ${SKELETAL_ANIMATION_OZZ_SOURCE_DIR}")
endif()

set(ozz_build_tools OFF CACHE BOOL "" FORCE)
set(ozz_build_fbx OFF CACHE BOOL "" FORCE)
set(ozz_build_gltf OFF CACHE BOOL "" FORCE)
set(ozz_build_data OFF CACHE BOOL "" FORCE)
set(ozz_build_samples OFF CACHE BOOL "" FORCE)
set(ozz_build_howtos OFF CACHE BOOL "" FORCE)
set(ozz_build_tests OFF CACHE BOOL "" FORCE)
set(ozz_build_msvc_rt_dll OFF CACHE BOOL "" FORCE)
add_subdirectory("${SKELETAL_ANIMATION_OZZ_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/external/ozz-animation")

add_library(experimental_skeletal_animation STATIC
    src/nt_skeletal_animation_ozz.cpp
    src/nt_skeletal_mesh_cpu.cpp
    include/skeletal_animation/nt_skeletal_animation.h
    include/skeletal_animation/nt_skeletal_mesh.h
)
target_include_directories(experimental_skeletal_animation
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
target_link_libraries(experimental_skeletal_animation PRIVATE ozz_animation)
target_compile_features(experimental_skeletal_animation PUBLIC cxx_std_17)
target_compile_definitions(experimental_skeletal_animation PRIVATE _CRT_SECURE_NO_WARNINGS)
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(experimental_skeletal_animation)
endif()
if(COMMAND nt_set_sanitizer_flags)
    nt_set_sanitizer_flags(experimental_skeletal_animation)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(experimental_skeletal_animation PRIVATE
        -Wno-error=format-nonliteral
        -Wno-error=sign-conversion
        -Wno-format-nonliteral
        -Wno-sign-conversion
    )
endif()

add_executable(experimental_skeletal_animation_ozz_probe
    tools/skeletal_animation_ozz_probe.c
)
target_link_libraries(experimental_skeletal_animation_ozz_probe PRIVATE experimental_skeletal_animation)
target_compile_definitions(experimental_skeletal_animation_ozz_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(experimental_skeletal_animation_ozz_probe)
endif()
if(COMMAND nt_set_sanitizer_flags)
    nt_set_sanitizer_flags(experimental_skeletal_animation_ozz_probe)
endif()
set_target_properties(experimental_skeletal_animation_ozz_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)

add_executable(experimental_skeletal_mesh_contract_probe
    tools/skeletal_mesh_contract_probe.c
)
target_link_libraries(experimental_skeletal_mesh_contract_probe PRIVATE experimental_skeletal_animation)
target_compile_definitions(experimental_skeletal_mesh_contract_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(experimental_skeletal_mesh_contract_probe)
endif()
if(COMMAND nt_set_sanitizer_flags)
    nt_set_sanitizer_flags(experimental_skeletal_mesh_contract_probe)
endif()
set_target_properties(experimental_skeletal_mesh_contract_probe PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin"
)
if(WIN32)
    set_target_properties(experimental_skeletal_animation
                          experimental_skeletal_animation_ozz_probe
                          experimental_skeletal_mesh_contract_probe PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    )
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME experimental_skeletal_mesh_contract
        COMMAND experimental_skeletal_mesh_contract_probe)
    add_test(NAME experimental_skeletal_quarantine_contract
        COMMAND "${CMAKE_COMMAND}"
            -DEXTENSION_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/verify_quarantine.cmake")
    set_tests_properties(
        experimental_skeletal_mesh_contract
        experimental_skeletal_quarantine_contract
        PROPERTIES LABELS "experimental")

    set(SKELETAL_EXPERIMENTAL_SKELETON "" CACHE FILEPATH
        "Optional skeleton .ozz fixture for the experimental sampling probe")
    set(SKELETAL_EXPERIMENTAL_ANIMATION "" CACHE FILEPATH
        "Optional animation .ozz fixture for the experimental sampling probe")
    if(SKELETAL_EXPERIMENTAL_SKELETON AND SKELETAL_EXPERIMENTAL_ANIMATION)
        add_test(NAME experimental_skeletal_ozz_sampling
            COMMAND experimental_skeletal_animation_ozz_probe
                --skeleton "${SKELETAL_EXPERIMENTAL_SKELETON}"
                --animation "${SKELETAL_EXPERIMENTAL_ANIMATION}")
        set_tests_properties(experimental_skeletal_ozz_sampling PROPERTIES
            LABELS "experimental"
            REQUIRED_FILES
                "${SKELETAL_EXPERIMENTAL_SKELETON};${SKELETAL_EXPERIMENTAL_ANIMATION}")
    elseif(SKELETAL_EXPERIMENTAL_SKELETON OR SKELETAL_EXPERIMENTAL_ANIMATION)
        message(FATAL_ERROR
            "Set both SKELETAL_EXPERIMENTAL_SKELETON and "
            "SKELETAL_EXPERIMENTAL_ANIMATION, or neither.")
    else()
        message(STATUS "No explicit Ozz fixtures; sampling CTest is not registered")
    endif()
endif()
