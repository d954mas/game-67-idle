cmake_minimum_required(VERSION 3.25)
project(limb_ik LANGUAGES C)

include(CTest)

add_executable(test_limb_ik tests/test_limb_ik.c)
target_compile_features(test_limb_ik PRIVATE c_std_99)
target_include_directories(test_limb_ik PRIVATE include)
if(MSVC)
    target_compile_options(test_limb_ik PRIVATE /W4 /WX)
else()
    target_compile_options(test_limb_ik PRIVATE -Wall -Wextra -Werror)
endif()
if(UNIX)
    target_link_libraries(test_limb_ik PRIVATE m)
endif()

add_test(NAME test_limb_ik COMMAND test_limb_ik)
set_tests_properties(test_limb_ik PROPERTIES LABELS "core")
