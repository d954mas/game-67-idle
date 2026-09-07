# The pack decodes with the engine's vendored stb_image, which the engine builds
# only for native tools — its `deps/stb` subdirectory sits inside a
# `if(NOT EMSCRIPTEN)` block. A web build therefore has the header on disk and no
# target to link, and the failure surfaces as "stb_image.h file not found" in a
# consumer's CMake rather than anywhere near this pack.
#
# Call this before linking `stb_image`; on native trees it finds the engine's
# target and does nothing.
#
#   include("${REMOTE_IMAGE_DIR}/cmake/RemoteImage.cmake")
#   remote_image_ensure_stb("${ENGINE_DIR}")

function(remote_image_ensure_stb engine_dir)
    if(TARGET stb_image)
        return()
    endif()
    add_library(stb_image STATIC "${engine_dir}/deps/stb/stb_image_impl.c")
    target_include_directories(stb_image PUBLIC "${engine_dir}/deps/stb")
endfunction()
