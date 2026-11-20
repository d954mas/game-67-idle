cmake_minimum_required(VERSION 3.25)

add_library(net_bits STATIC src/net_bits.c src/net_bits_record.c)
target_include_directories(net_bits PUBLIC include)
target_compile_features(net_bits PUBLIC c_std_11)
# Both ends must compute the same values: a fused multiply-add or fast-math on one side only
# moves a reconstructed rotation by a rounding step. Target options follow a consumer's
# global flags, so these win over them.
if(NOT MSVC)
    target_compile_options(net_bits PRIVATE -ffp-contract=off -fno-fast-math)
endif()
if(UNIX)
    target_link_libraries(net_bits PUBLIC m)
endif()
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(net_bits)
elseif(NOT MSVC)
    target_compile_options(net_bits PRIVATE -Wall -Wextra -Werror)
endif()
if(COMMAND nt_set_sanitizer_flags)
    nt_set_sanitizer_flags(net_bits)
endif()
