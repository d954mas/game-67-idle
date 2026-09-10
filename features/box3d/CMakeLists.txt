cmake_minimum_required(VERSION 3.25)
project(nt_box3d VERSION 0.1.0 LANGUAGES C)

if(TARGET box3d OR TARGET nt::box3d)
    message(FATAL_ERROR "A Box3D target already exists; use box3d_enable for every consumer")
endif()
# A single portable ABI keeps native and WASM consumers on the same physics path.
set(BOX3D_DISABLE_SIMD ON)
set(BOX3D_DOUBLE_PRECISION OFF)
set(BOX3D_PROFILE OFF)
set(BOX3D_VALIDATE OFF)
set(BOX3D_COMPILE_WARNING_AS_ERROR OFF)
set(BUILD_SHARED_LIBS OFF)
# Upstream uses the CXX identity only to choose a C warning in its MSVC branch.
# C-only hosts must supply that comparison operand without enabling C++.
if(NOT CMAKE_CXX_COMPILER_ID)
    set(CMAKE_CXX_COMPILER_ID "${CMAKE_C_COMPILER_ID}")
endif()
add_subdirectory(vendor/box3d/src)
add_library(feature_box3d INTERFACE)
add_library(nt::box3d ALIAS feature_box3d)
target_link_libraries(feature_box3d INTERFACE box3d::box3d)
target_include_directories(feature_box3d SYSTEM BEFORE INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/box3d/include")
target_compile_definitions(feature_box3d INTERFACE FEATURE_BOX3D=1)

if(NOT EMSCRIPTEN)
    find_package(Threads REQUIRED)
    target_link_libraries(feature_box3d INTERFACE Threads::Threads)
endif()
