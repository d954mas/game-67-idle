cmake_minimum_required(VERSION 3.25)

add_library(garment_cloth STATIC src/cloth.c src/contact.c src/pose.c src/render_binding.c)
target_include_directories(garment_cloth PUBLIC include)
target_compile_definitions(garment_cloth PUBLIC FEATURE_GARMENT_CLOTH=1)
target_compile_features(garment_cloth PUBLIC c_std_11)
target_link_libraries(garment_cloth PRIVATE nt_core nt_math)
nt_set_warning_flags(garment_cloth)
nt_set_sanitizer_flags(garment_cloth)
