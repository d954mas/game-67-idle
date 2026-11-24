cmake_minimum_required(VERSION 3.25)

add_library(garment_cloth STATIC src/cloth.c src/contact.c src/surface_transfer.c src/debug.c src/actor.c src/asset.c)
target_include_directories(garment_cloth PUBLIC include)
target_compile_definitions(garment_cloth PUBLIC FEATURE_GARMENT_CLOTH=1)
target_compile_features(garment_cloth PUBLIC c_std_11)
target_link_libraries(garment_cloth PUBLIC nt_skeletal PRIVATE nt_core nt_math nt_hash)
nt_set_warning_flags(garment_cloth)
# The web stack is 64 KiB, so per-vertex arrays live in thread-local scratch. Unoptimized and sanitized Debug frames are larger.
target_compile_options(garment_cloth PRIVATE $<$<NOT:$<CONFIG:Debug>>:-Wframe-larger-than=2048>)
nt_set_sanitizer_flags(garment_cloth)

# The runtime stamp every build of tools/host/garment_host.c carries (gh_runtime_sha256): the hash of the runtime
# sources it was compiled from, by the rule host.py checks before it steps anything.
file(GLOB GARMENT_RUNTIME_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/garment/*.h")
set(GARMENT_HOST_STAMP "${CMAKE_CURRENT_BINARY_DIR}/host_stamp/garment_host_runtime.gen.h")
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH garment_features)
cmake_path(GET garment_features PARENT_PATH garment_studio)
add_custom_command(OUTPUT "${GARMENT_HOST_STAMP}"
    COMMAND "${CMAKE_COMMAND}" "-DFEATURE=${CMAKE_CURRENT_SOURCE_DIR}" "-DSTUDIO=${garment_studio}"
        "-DOUT=${GARMENT_HOST_STAMP}" -P "${CMAKE_CURRENT_SOURCE_DIR}/tools/host/runtime_stamp.cmake"
    DEPENDS ${GARMENT_RUNTIME_SOURCES} tools/host/garment_host.c tools/host/runtime_stamp.cmake VERBATIM)
add_custom_target(garment_host_runtime_stamp DEPENDS "${GARMENT_HOST_STAMP}")
add_library(garment_host_stamp INTERFACE)
target_include_directories(garment_host_stamp INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/host_stamp")
add_dependencies(garment_host_stamp garment_host_runtime_stamp)

# The offline host (tools/host, driven from tools/garment/host.py): the same sources at -O2 in every configuration, so a
# bake runs at the speed of the shipped solver. Engine modules are static and, off Windows, neither position independent
# nor free of Debug sanitizers, so only a Windows build links them into a library Python can load.
if(WIN32 AND NOT EMSCRIPTEN)
    add_library(garment_host SHARED tools/host/garment_host.c src/cloth.c src/contact.c src/surface_transfer.c src/debug.c
        src/actor.c src/asset.c)
    target_link_libraries(garment_host PRIVATE garment_host_stamp)
    target_include_directories(garment_host PRIVATE include)
    target_compile_features(garment_host PRIVATE c_std_11)
    target_link_libraries(garment_host PRIVATE nt_skeletal nt_core nt_math nt_hash)
    nt_set_warning_flags(garment_host)
    target_compile_options(garment_host PRIVATE $<$<NOT:$<CONFIG:Release>>:-O2>)
endif()
