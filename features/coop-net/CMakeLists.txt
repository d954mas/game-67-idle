cmake_minimum_required(VERSION 3.25)

# The consumer adds net-link and net-bits first; this target only names them.
add_library(coop_net STATIC
    src/coop_session.c
    src/coop_session_host.c
    src/coop_session_guest.c
    src/coop_pending.c
    src/coop_wire.c
    src/coop_acks.c
    src/coop_clock.c)
target_include_directories(coop_net PUBLIC include)
target_compile_features(coop_net PUBLIC c_std_11)
target_link_libraries(coop_net PUBLIC net_link PRIVATE net_bits)
if(UNIX)
    target_link_libraries(coop_net PRIVATE m)
endif()
if(COMMAND nt_set_warning_flags)
    nt_set_warning_flags(coop_net)
elseif(NOT MSVC)
    target_compile_options(coop_net PRIVATE -Wall -Wextra -Werror)
endif()
if(COMMAND nt_set_sanitizer_flags)
    nt_set_sanitizer_flags(coop_net)
endif()
