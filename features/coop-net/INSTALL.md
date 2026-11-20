# Install coop net

## Link

Add the three in-place modules, the two it stands on first, after defining the
consuming target:

```cmake
set(NET_LINK_DIR "${GAME_REPO_ROOT}/features/net-link")
set(NET_BITS_DIR "${GAME_REPO_ROOT}/features/net-bits")
set(COOP_NET_DIR "${GAME_REPO_ROOT}/features/coop-net")
add_subdirectory("${NET_LINK_DIR}" "${CMAKE_BINARY_DIR}/net-link")
add_subdirectory("${NET_BITS_DIR}" "${CMAKE_BINARY_DIR}/net-bits")
add_subdirectory("${COOP_NET_DIR}" "${CMAKE_BINARY_DIR}/coop-net")
target_link_libraries(game PRIVATE coop_net)
```

`coop_net` links `net_link` publicly (the config takes a `net_link_t *`) and
`net_bits` privately. Record the source and version of all three from their
`feature.json` in the consuming game's `dependencies.json`.

## Use

1. Open the link with `coop_session_lanes()`: state on lane 0 (Steam's cheapest
   lane), records on 1, baselines on 2; records first when the path is short of
   room, baselines last. On the simulator that is
   `net_link_sim_config_t.lanes`; a Steam link maps the same lanes.
2. Build one `build_key` that names everything two builds must share: a
   content hash mixed with every net-bits table hash, the tick rate and the
   game's own protocol number (`net_bits_hash_mix`, fixed order). `seats` is
   the host's alone; each side's `baseline_max` is respected by the other.
3. Host: `coop_session_host` with `seats`, `tick_hz`, the starting `tick`, the
   world key and `baseline_max`. Guest: open a guest link to the host, then
   `coop_session_join` with its `player_key` (what tells two windows of one
   account apart), its starting `tick`, its world key or none and the largest
   baseline it accepts.
4. Each tick, in this order: `coop_session_set_time`, `coop_session_poll`
   until false, the game's step, the sends, `coop_session_end_tick`. The game
   sends state as often as it likes: `end_tick` sends an empty state packet to
   every seat left without one, so acknowledgements always flow.
5. Handle `SEAT_JOINED` / `SEAT_LEFT` (roster), `WELCOME` (seat, the host's tick
   and world key), `CLOSED` (show `coop_close_name(reason)`), `BASELINE`. Count
   the RECORD events from the host since `WELCOME`: the first `records_before`
   of them are inside the baseline, the later ones apply after it. Play a peer's
   bodies on `coop_session_peer_tick`.
6. Shut down: `coop_session_destroy`, then `net_link_destroy`. The close
   lingers, so the other side reads `HOST_LEFT` or `LEFT`.

## Verify

```powershell
cmake --build ai_studio/feature_testing/consumer/build/feature-verify --target test_coop_wire test_coop_session test_coop_traffic test_coop_hostile
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "^test_coop" --output-on-failure
node features/validate_contracts.mjs
```
