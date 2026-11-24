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

# The offline host (tools/host, driven from tools/garment/host.py): the same sources at -O2 in every configuration, so a
# bake runs at the speed of the shipped solver. Engine modules are static and, off Windows, neither position independent
# nor free of Debug sanitizers, so only a Windows build links them into a library Python can load.
if(WIN32 AND NOT EMSCRIPTEN)
    add_library(garment_host SHARED tools/host/garment_host.c src/cloth.c src/contact.c src/surface_transfer.c src/debug.c
        src/actor.c src/asset.c)
    target_include_directories(garment_host PRIVATE include)
    target_compile_features(garment_host PRIVATE c_std_11)
    target_link_libraries(garment_host PRIVATE nt_skeletal nt_core nt_math nt_hash)
    nt_set_warning_flags(garment_host)
    target_compile_options(garment_host PRIVATE $<$<NOT:$<CONFIG:Release>>:-O2>)
endif()
