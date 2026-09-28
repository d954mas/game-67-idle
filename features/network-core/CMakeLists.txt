cmake_minimum_required(VERSION 3.25)

# network-core: WebSocket transport for authoritative rooms (native server on
# uWebSockets, native client on libcurl, browser client on the Emscripten
# WebSocket API) plus the bounded little-endian message codec both sides share.
#
# Consumers add this directory and link nt::network_core. The feature has no
# engine dependency, so a bare CMake project can be its second consumer.

set(NETWORK_CORE_DIR "${CMAKE_CURRENT_LIST_DIR}")

add_library(network_core_codec STATIC
    "${NETWORK_CORE_DIR}/src/net_codec.c"
    "${NETWORK_CORE_DIR}/src/net_hmac.c")
target_include_directories(network_core_codec PUBLIC "${NETWORK_CORE_DIR}/include")
# Both sides of a wire replay the quantizer's arithmetic bit for bit; a fused
# multiply-add on one of them would put the same value on different grid
# points.
if(NOT MSVC)
    target_compile_options(network_core_codec PRIVATE -ffp-contract=off)
endif()
if(UNIX)
    target_link_libraries(network_core_codec PUBLIC m)
endif()
add_library(nt::network_core_codec ALIAS network_core_codec)

if(EMSCRIPTEN)
    add_library(network_core STATIC
        "${NETWORK_CORE_DIR}/src/net_ws_client_web.c")
    target_link_libraries(network_core PUBLIC network_core_codec)
    # The browser owns the socket; emscripten's websocket.h is a thin binding.
    target_link_options(network_core INTERFACE "SHELL:-lwebsocket.js")
else()
    # The server runs on uWebSockets and uSockets: epoll on Linux, libuv on
    # Windows (dev rooms and tests). TLS is opt-in: a room that terminates
    # its own TLS (no proxy in the game path) builds with
    # NETWORK_CORE_WITH_TLS and OpenSSL on the box; everything else, tests
    # included, stays plain.
    option(NETWORK_CORE_WITH_TLS "Serve wss:// from the room itself (needs OpenSSL)" OFF)
    enable_language(CXX)
    set(network_core_us "${NETWORK_CORE_DIR}/vendor/uwebsockets/uSockets/src")
    add_library(network_core_usockets STATIC
        "${network_core_us}/bsd.c" "${network_core_us}/context.c" "${network_core_us}/loop.c"
        "${network_core_us}/socket.c" "${network_core_us}/udp.c")
    target_include_directories(network_core_usockets SYSTEM PUBLIC "${network_core_us}")
    target_compile_features(network_core_usockets PUBLIC cxx_std_20)
    if(WIN32)
        set(network_core_uv "${NETWORK_CORE_DIR}/vendor/libuv")
        file(GLOB network_core_uv_sources "${network_core_uv}/src/*.c" "${network_core_uv}/src/win/*.c")
        add_library(network_core_libuv STATIC ${network_core_uv_sources})
        target_include_directories(network_core_libuv SYSTEM PUBLIC "${network_core_uv}/include"
            PRIVATE "${network_core_uv}/src")
        target_compile_definitions(network_core_libuv PRIVATE WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00
            _CRT_DECLARE_NONSTDC_NAMES=0)
        target_link_libraries(network_core_libuv PUBLIC psapi user32 advapi32 iphlpapi userenv ws2_32
            dbghelp ole32 shell32 synchronization)
        target_sources(network_core_usockets PRIVATE "${network_core_us}/eventing/libuv.c")
        target_compile_definitions(network_core_usockets PUBLIC LIBUS_USE_LIBUV)
        target_link_libraries(network_core_usockets PUBLIC network_core_libuv)
    else()
        target_sources(network_core_usockets PRIVATE "${network_core_us}/eventing/epoll_kqueue.c")
    endif()
    if(NETWORK_CORE_WITH_TLS)
        find_package(OpenSSL REQUIRED)
        target_sources(network_core_usockets PRIVATE
            "${network_core_us}/crypto/openssl.c" "${network_core_us}/crypto/sni_tree.cpp")
        target_compile_definitions(network_core_usockets PUBLIC LIBUS_USE_OPENSSL)
        target_link_libraries(network_core_usockets PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    else()
        target_compile_definitions(network_core_usockets PUBLIC LIBUS_NO_SSL)
    endif()

    # The native client rides libcurl's WebSocket (NT_HTTP_WEBSOCKETS on the
    # engine's curl, or a consumer's own CURL::libcurl with WebSockets):
    # wss:// comes with the box's own trust store and no second TLS library.
    get_property(network_core_curl_ws GLOBAL PROPERTY NT_HTTP_WEBSOCKETS)
    if(NOT TARGET CURL::libcurl OR NOT network_core_curl_ws)
        message(FATAL_ERROR "network-core's native client needs CURL::libcurl with WebSockets "
            "and the global property NT_HTTP_WEBSOCKETS (the engine's NT_HTTP_CURL + NT_HTTP_WEBSOCKETS)")
    endif()
    add_library(network_core STATIC
        "${NETWORK_CORE_DIR}/src/net_ws_server_uws.cpp"
        "${NETWORK_CORE_DIR}/src/net_us_loop.c"
        "${NETWORK_CORE_DIR}/src/net_ws_client_curl.c")
    target_link_libraries(network_core PUBLIC network_core_codec PRIVATE network_core_usockets CURL::libcurl)
    target_include_directories(network_core SYSTEM PRIVATE "${NETWORK_CORE_DIR}/vendor/uwebsockets/src")
    # No permessage-deflate is ever offered, so zlib is not linked. The
    # peer's address is the one accept() gave, kept in user space: a closed
    # socket still has it, and no getpeername runs per read.
    target_compile_definitions(network_core PRIVATE UWS_NO_ZLIB UWS_REMOTE_ADDRESS_USERSPACE
        NETWORK_CORE_UWS_VERSION="20.80.0" NET_WS_TLS=$<BOOL:${NETWORK_CORE_WITH_TLS}>)
    if(WIN32)
        target_link_libraries(network_core PUBLIC ws2_32)
    else()
        # The server is C++ behind a C API: a C consumer's link step, which
        # knows nothing of C++, still needs its runtime. The native client
        # services its socket on a thread of its own.
        find_package(Threads REQUIRED)
        target_link_libraries(network_core PUBLIC stdc++ Threads::Threads)
    endif()
    # The engine's CRT pin, where one is set: the C++ targets carry the
    # runtime and iterator level in every object, so they take the C++ pin.
    if(COMMAND nt_set_static_crt_cxx)
        nt_set_static_crt_cxx(network_core)
        nt_set_static_crt_cxx(network_core_usockets)
        if(TARGET network_core_libuv)
            nt_set_static_crt(network_core_libuv)
        endif()
    endif()
endif()
target_include_directories(network_core PUBLIC "${NETWORK_CORE_DIR}/include")
foreach(network_core_target network_core_codec network_core)
    if(COMMAND nt_set_warning_flags)
        nt_set_warning_flags(${network_core_target})
    elseif(NOT MSVC)
        target_compile_options(${network_core_target} PRIVATE -Wall -Wextra -Werror)
    endif()
    if(COMMAND nt_set_sanitizer_flags)
        nt_set_sanitizer_flags(${network_core_target})
    endif()
endforeach()
add_library(nt::network_core ALIAS network_core)
