# coop-net

## Purpose

Authoritative co-op rooms and their clients, on top of `features/network-core`. It carries the
parts every co-op game repeats: seats and joining, per-seat input mailboxes with a sequence number
and an acknowledgement, one fixed tick that calls the game to step its world, one snapshot per tick
to every seated client, and a client that sends input under its own sequence and hands snapshots
back to the game.

The world, the movement rules, prediction and reconciliation, and the game's payload stay in the
game. This module never knows what a heroine, a prop or a tick of physics is.

Wire, one type byte first:

```text
input     [1][seq u32][game payload]
welcome   [2][seat u8][tick u32]
snapshot  [3][tick u32][count u8][acks u32 x count][game payload]
event     [4][game payload]
```

## Public surface

- `coop_net` CMake target.
- `coop_net.h`:
  - room: `coop_room_create(config, callbacks)` (or the `coop_room_start` shorthand),
    `coop_room_pump(room, dt)`, `coop_room_broadcast(room, data, size)`, `coop_room_stop`,
    `coop_room_port`, `coop_room_seats`, `coop_room_ack(room, seat)`;
  - client: `coop_client_create(config, callbacks)` (or `coop_client_connect`), `coop_client_service`,
    `coop_client_send_input`, `coop_client_seq`, `coop_client_seat`, `coop_client_ready`,
    `coop_client_stop`.
- `coop_transport.h`: the replaceable transport — `coop_server_transport_t`,
  `coop_client_transport_t`, `coop_transport_config_t`, `coop_link_events_t`, and the shipped
  WebSocket factory `coop_transport_ws_server()` / `coop_transport_ws_client()`.
- Callbacks: the room asks the game to join a seat, to receive one input payload, to tick the world,
  and to write the snapshot payload; the client reports welcome, snapshot, event and close.
- Close codes: `COOP_NET_CLOSE_FULL`, `COOP_NET_CLOSE_FORMAT`.

## Transport

The transport is a contract, not a dependency: a room and a client hold a `coop_server_transport_t`
or a `coop_client_transport_t` and call `start/service/send/send_latest/stop/port` through it.
`transport_ws.c` is the shipped implementation and the only file here that includes network-core.
A consumer may pass its own transport in `coop_room_config_t.transport` /
`coop_client_config_t.transport`: a Steam transport, an in-memory one for tests, or a recorded
replay. The rest of the module never learns which one it got.

## Validation

`node features/validate_contracts.mjs` from the Studio root, then the consuming game's
`node tools/game.mjs test` and a loopback run: a room with two clients, both seated, both moving,
acknowledgements advancing (the game's `net_probe`).

## Compatibility

`feature.json.version` is exact SemVer.

- PATCH keeps the public contract: header names, callback order, wire layout, close-code meanings.
- MINOR adds surface with a safe default: a new callback field that may be NULL, a new query, a new
  close code above the current range.
- MAJOR may change the wire layout, the callback signatures, the seat limit rule or the meaning of
  an existing close code; it ships with every consuming game updated and its probe green.

## Extension points

- **Transport**: pass another `coop_server_transport_t` / `coop_client_transport_t`; WS is only the
  default. Steam, memory and recorded transports plug in here.
- **Config**: seats, message and snapshot sizes, protocol version, ping policy — zero means default.
- A game may put any payload in the input and snapshot; versioning the payload is the game's.
- Prediction and reconciliation are deliberately absent: the game owns its movement rules and
  keys its history on `coop_client_seq` and the per-seat acknowledgements.
