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
