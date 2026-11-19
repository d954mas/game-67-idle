add_library(coop_net STATIC
    "${CMAKE_CURRENT_LIST_DIR}/src/coop_net.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/transport_ws.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/transport_memory.c")
target_include_directories(coop_net PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
# Rooms and clients sit on the WebSocket transport; the world and the rules stay in the game.
target_link_libraries(coop_net PUBLIC nt::network_core)
