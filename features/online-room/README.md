# Online Room

## Purpose

The C half of a room-based game's online layer: both sides of the
room-metaserver contract (`CONTRACT.md`; the Go half is `features/metaserver`).
The game keeps everything that knows its world: the room loop and host policy,
profile documents and savers, its wire, perf and sample reports.

## Public surface

- `online_room` CMake target: every source below except the transports, with
  PUBLIC `ONLINE_ROOM_PLAYERS_MAX`, `ONLINE_ROOM_PARTING_MAX` and
  `ONLINE_ROOM_PROFILE_TEXT_MAX`. The consumer sets them as CMake variables;
  there are no defaults (`INSTALL.md`).
- Room: version 2 tickets and the replay memory (`online_room/ticket.h`), the
  metaserver's profile store and status calls (`online_room/meta.h`), the
  profile store contract (`online_room/profile_store.h`) and its in-memory
  store for tests and tools (`online_room/memory_store.h`).
- Client: the connection ladder (`online_room/entry.h`), entry through the
  metaserver (`online_room/meta_entry.h`), account calls
  (`online_room/meta_account.h`), the round trip (`online_room/rtt.h`).
- The room's close codes (`online_room/close.h`).
- Transports over the engine's `nt_http`: `ONLINE_ROOM_META_NT_HTTP_SOURCE`
  (`online_room/meta_nt_http.h`) and `ONLINE_ROOM_META_ENTRY_NT_HTTP_SOURCE`
  (`online_room/meta_entry_nt_http.h`), compiled into the consumer's own
  targets beside the `nt_http` build it sizes.
- `tools/room_monitor.mjs`: `localRoom({ binary })`, the `live` hook a room
  game passes to the Studio packaged web smoke. It starts the room on loopback,
  points the page at it with `?room=`, and at every capture requires a live
  room process, an open page WebSocket and a seated player in the newest
  `plw_room_stats` line (`node --test features/online-room/tools/room_monitor.test.mjs`).

## Validation

`tests/online_room_*.c` are core CTest harnesses. Consumers include
`cmake/OnlineRoomTests.cmake` and call `online_room_add_tests()`; they then run
with that game's sizes.

```sh
ctest --test-dir games/<game-id>/build/<preset> -L core -R ^online_room_ --output-on-failure
node features/validate_contracts.mjs
```

## Compatibility

PATCH changes preserve exported names, include spelling, the ticket layout,
kinds and routes, and store semantics. MINOR may add an independent API or a
field with a safe default. MAJOR may rename or remove a symbol, or change the
wire, a route or a store verdict; it ships together with `features/metaserver`
and every live game (`node ai_studio/game_tools/live_games.mjs`).

## Extension points

The game binds the room's transport (`online_meta_transport`) and the
client's (`online_meta_entry_http`, `online_meta_entry_storage`); the shipped
ones are the `nt_http` sources above. A game supplies its ladder knobs
(`online_entry_knobs`) from its content and decides which seats a ticket of
each kind may take.
