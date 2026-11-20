# Install net link

## Link

Add the in-place module after defining the consuming target. It needs only libc.

```cmake
set(NET_LINK_DIR "${GAME_REPO_ROOT}/features/net-link")
add_subdirectory("${NET_LINK_DIR}" "${CMAKE_BINARY_DIR}/net-link")
target_link_libraries(game PRIVATE net_link)
```

Record the feature source and version from `feature.json` in the consuming
game's `dependencies.json`.

## Use

1. Open links from a transport: `net_link_sim_host` / `net_link_sim_guest` in
   tests, a platform transport in the game.
2. Each tick: `net_link_poll` until it returns zero, handling each batch
   before the next call, because a payload from `poll` is readable only until
   the next `poll` of the same link. Then send, and `net_link_flush` once at
   the end of the tick.
3. With the simulator, call `net_link_sim_advance` with the tick length; set
   conditions per guest and direction with `net_link_sim_set_conditions`.
4. To capture, open a file with `net_link_capture_open`, attach links with a
   tag, call `net_link_capture_set_time` each tick, detach the links and close
   the file. Read it back with `net_link_capture_reader_open`.

## Test a new transport

Link `tests/net_link_contract.c` into the transport's test, fill a
`net_link_contract_env_t` and call `net_link_contract_run_all` between
`UNITY_BEGIN` and `UNITY_END`. The env opens the host and guests and lets time
pass, and it gives the settle and fail times. It also says how many guest links
it holds at once (cases that need two are ignored at one) and may give a cut
hook with its timeout (the cut cases are ignored without one).

## Verify

```powershell
cmake --build ai_studio/feature_testing/consumer/build/feature-verify --target test_net_link_sim test_net_link_capture
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "^test_net_link" --output-on-failure
node features/validate_contracts.mjs
```
