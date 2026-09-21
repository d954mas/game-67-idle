cmake_minimum_required(VERSION 3.25)

# network-core: WebSocket transport for authoritative rooms (native server and
# client on a pruned libwebsockets, browser client on the Emscripten WebSocket
# API) plus the bounded little-endian message codec shared by both sides.
#
# Consumers add this directory and link nt::network_core. The feature has no
# engine dependency, so a bare CMake project can be its second consumer.

set(NETWORK_CORE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(NETWORK_CORE_LWS_DIR "${NETWORK_CORE_DIR}/vendor/libwebsockets")

add_library(network_core_codec STATIC
    "${NETWORK_CORE_DIR}/src/net_codec.c"
    "${NETWORK_CORE_DIR}/src/net_hmac.c")
target_include_directories(network_core_codec PUBLIC "${NETWORK_CORE_DIR}/include")
add_library(nt::network_core_codec ALIAS network_core_codec)

if(EMSCRIPTEN)
    add_library(network_core STATIC
        "${NETWORK_CORE_DIR}/src/net_ws_client_web.c")
    target_link_libraries(network_core PUBLIC network_core_codec)
    # The browser owns the socket; emscripten's websocket.h is a thin binding.
    target_link_options(network_core INTERFACE "SHELL:-lwebsocket.js")
else()
    # The library builds without HTTP/2, extensions (no permessage-deflate),
    # file serving or any of the optional subsystems; the vendored tree only
    # carries what remains. TLS is opt-in: a room that terminates its own
    # TLS (no proxy in the game path) builds with NETWORK_CORE_WITH_TLS and
    # OpenSSL on the box; everything else, tests included, stays plain.
    option(NETWORK_CORE_WITH_TLS "Serve wss:// from the room itself (needs OpenSSL)" OFF)
    if(NETWORK_CORE_WITH_TLS)
        set(LWS_WITH_SSL ON CACHE BOOL "" FORCE)
        set(LWS_WITH_GENCRYPTO OFF CACHE BOOL "" FORCE)
        set(LWS_WITH_TLS_JIT_TRUST OFF CACHE BOOL "" FORCE)
        # Digest auth is an HTTP client feature that TLS switches on and the
        # pruned header table cannot serve; nothing here authenticates.
        set(LWS_WITH_HTTP_DIGEST_AUTH OFF CACHE BOOL "" FORCE)
    else()
        set(LWS_WITH_SSL OFF CACHE BOOL "" FORCE)
    endif()
    set(LWS_WITH_SHARED OFF CACHE BOOL "" FORCE)
    # One address may hold only so many sockets of a server, handshaking
    # ones included: the rooms face the internet with no proxy in front.
    set(LWS_WITH_PEER_LIMITS ON CACHE BOOL "" FORCE)
    set(LWS_WITH_STATIC ON CACHE BOOL "" FORCE)
    set(LWS_STATIC_PIC ON CACHE BOOL "" FORCE)
    set(LWS_WITHOUT_TESTAPPS ON CACHE BOOL "" FORCE)
    set(LWS_WITHOUT_TEST_SERVER ON CACHE BOOL "" FORCE)
    set(LWS_WITHOUT_TEST_PING ON CACHE BOOL "" FORCE)
    set(LWS_WITHOUT_TEST_CLIENT ON CACHE BOOL "" FORCE)
    set(LWS_WITHOUT_EXTENSIONS ON CACHE BOOL "" FORCE)
    set(LWS_WITH_HTTP2 OFF CACHE BOOL "" FORCE)
    set(LWS_ROLE_RAW_FILE OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SYS_SMD OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SYS_STATE OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SYS_FAULT_INJECTION OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SYS_ASYNC_DNS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SYS_METRICS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SECURE_STREAMS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_LEJP OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_LEJP_CONF OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_LWSAC OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_LHP OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_DLO OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_UPNG OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_JPEG OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_GZINFLATE OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_JSONRPC OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_WOL OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_LWS_DSH OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_CUSTOM_HEADERS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_HTTP_BASIC_AUTH OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_HTTP_UNCOMMON_HEADERS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_HTTP_PROXY OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SOCKS5 OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_UDP OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_TLS_SESSIONS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_CACHE_NSCOOKIEJAR OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_DIR OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_UNIX_SOCK OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_NETLINK OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SPAWN OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_THREADPOOL OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SUL_DEBUGGING OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_CONMON OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_ACCESS_LOG OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_SERVER_STATUS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_RANGES OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_ZIP_FOPS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_EVLIB_PLUGINS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_PLUGINS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_LWSWS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_EXPORT_LWSTARGETS OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_MINIMAL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LWS_WITH_FILE_OPS ON CACHE BOOL "" FORCE)
    set(LWS_LOGS_TIMESTAMP OFF CACHE BOOL "" FORCE)
    set(LWS_SUPPRESS_DEPRECATED_API_WARNINGS ON CACHE BOOL "" FORCE)
    # Upstream compiles with -Werror; the vendored copy is not ours to fix.
    set(DISABLE_WERROR ON CACHE BOOL "" FORCE)
    add_subdirectory("${NETWORK_CORE_LWS_DIR}" "${CMAKE_BINARY_DIR}/_network_core_lws" EXCLUDE_FROM_ALL)

    # The native client rides libcurl's WebSocket when the engine built it
    # in (NT_HTTP_WEBSOCKETS: Schannel on Windows, OpenSSL elsewhere), so
    # wss:// comes with the box's own trust store and no second TLS
    # library; any other build keeps the lws client, plain ws://.
    get_property(network_core_curl_ws GLOBAL PROPERTY NT_HTTP_WEBSOCKETS)
    if(TARGET CURL::libcurl AND network_core_curl_ws)
        set(network_core_client "${NETWORK_CORE_DIR}/src/net_ws_client_curl.c")
    else()
        set(network_core_client "${NETWORK_CORE_DIR}/src/net_ws_client_lws.c")
    endif()
    add_library(network_core STATIC
        "${NETWORK_CORE_DIR}/src/net_ws_server_lws.c"
        "${network_core_client}")
    target_link_libraries(network_core PUBLIC network_core_codec PRIVATE websockets)
    if(network_core_client MATCHES "curl")
        target_link_libraries(network_core PRIVATE CURL::libcurl)
    endif()
    target_compile_definitions(network_core PRIVATE NET_WS_TLS=$<BOOL:${NETWORK_CORE_WITH_TLS}>)
    if(WIN32)
        target_link_libraries(network_core PUBLIC ws2_32)
    else()
        # The native client services its socket on a thread of its own.
        find_package(Threads REQUIRED)
        target_link_libraries(network_core PUBLIC Threads::Threads)
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
