# network-core

WebSocket transport for authoritative game rooms, compiled in place by every
consumer.

## What it does

- **Server** (`net_ws_server.h`, native only): uWebSockets on uSockets, one
  thread; its event loop wakes for ready sockets only (epoll on Linux,
  libuv on Windows).
  `net_ws_server_service(server, timeout_ms)` runs accept, reads, writes and
  callbacks, and returns by the timeout so a fixed simulation tick can share
  the thread. No thread per connection, no lock around the world.
- **Client** (`net_ws_client.h`): the same header on native (a client on
  a thread of its own on libcurl's WebSocket: the engine's curl with
  `NT_HTTP_WEBSOCKETS`, or a consumer's own `CURL::libcurl` with the same
  global property) and web
  (`emscripten/websocket.h`, the page's own `WebSocket`). In all the
  socket is read the moment the
  network delivers, so every message carries its exact arrival time
  (`received_at`, on `net_ws_client_clock()`, the performance counter on
  Windows and CLOCK_MONOTONIC elsewhere), pongs go out during a loading
  stall and sends never wait for the next frame, or for 100 ms at most
  when the wake pipe is unavailable. Callbacks still fire only from
  `net_ws_client_service()`, on the caller's thread, in the order the
  socket saw them.
- **Workload counters** (`net_ws_server_take_stats`): plain counters of
  what the server handed the OS - messages and bytes each way, WebSocket
  messages read, pongs, sends handed to the socket (one frame each, one TLS
  record each under `wss://`), partial writes, full-queue refusals, writable callbacks,
  service calls and passes, and the fullest queue since the previous take.
  No clock read or lock; they describe transport work, not syscalls.
  `net_ws_library_version()` and `net_ws_tls_library_version()` name the
  linked libraries for run records.
- **Codec** (`net_codec.h`): bounded little-endian reader/writer. A read past
  the end clears `ok` and returns zero; the caller checks once at the end.
  Fixed-point fields: a `net_grid_t` (`min`, `step`, `bits`) names the grid a
  value travels on (`net_write_quantized` / `net_read_quantized`), and
  `net_quantize` is the same value snapped to it, so a simulation that snaps
  its state after every step holds the wire's numbers exactly and a correct
  prediction is never a correction. Headings go as 16 bits of a turn
  (`net_write_angle16`). A power-of-two step with `min` on the grid keeps
  every point exact in float.
- **Session rules every room inherits**: the first frame must be HELLO with
  the configured protocol version; text frames, oversize messages, reserved
  message types, a flooding sender and a client that cannot drain its queue
  are all closed with a `NET_CLOSE_*` code (4001-4006) before the application
  hears about them; a frame past `max(max_message_bytes, NET_HELLO_MAX_SIZE)`
  or a broken frame is closed by uWebSockets with its own standard code. A socket that never sends HELLO, or never acknowledges a
  close, is dropped after `handshake_timeout_ms`, and so is a socket that
  stalls in the TLS or HTTP handshake. Accepts are budgeted at `max_clients`
  per second (burst of twice that) and sockets short of a seat at
  `4 * max_clients + 8`, so a connect flood costs the service thread one
  handshake per seat per second and never the process's fd limit. A slot
  (and the client id) is earned by HELLO: sockets upgraded but not yet past
  HELLO wait in a room of `max_clients`, at most four of them from one
  address, so holding open connections without HELLO takes no seat and
  one machine cannot fill the waiting room. A socket that closes before its
  upgrade gives its address's share back at once. Client ids are never
  reused within a
  server lifetime. HELLO may carry a ticket of up to
  `NET_HELLO_TICKET_MAX` opaque bytes (a seat to resume, a join code); the
  transport hands it to `on_connect` and attaches no meaning to it.
- **Liveness is the server's**, because browsers cannot send pings: with
  `ping_idle_s` set, a peer silent for `ping_idle_s` gets a protocol ping
  (again every `ping_idle_s` while it stays silent), and a peer silent for
  `hangup_idle_s` is dropped as `NET_WS_CLOSE_PEER`. Any frame from the peer
  counts as life, so an active client is never pinged. Browsers answer
  pings natively, even from a hidden tab, so a backgrounded page stays a
  client while a dead network does not. uWebSockets' own idle timeout stays
  on a few seconds past the hangup as a backstop, which also bounds a
  closing socket whose peer stops reading. A peer that pings without
  reading is closed as slow once the pongs pile past its send queue.
- **What stays the application's call**, exposed as config or queries rather
  than decided here: the close code of an application close
  (`net_ws_server_close(..., code)`, 4007 or a game range such as 4100+),
  the subprotocol name (none by default), the client's receive-overflow
  policy (`overflow_policy`: close, or keep the newest messages), and
  coalescing (`net_ws_server_queued_bytes` tells the room how far behind a
  client is; the room decides whether to send another snapshot).
- Both ends may be destroyed from inside their own callbacks; the pump
  finishes the pass and frees afterwards.

Message payloads, snapshots, prediction and everything game-shaped stay in the
game. The feature carries transport and limits only.

## Performance choices

No permessage-deflate (never offered, zlib not linked), binary frames only,
one preallocated scratch buffer per server, per-connection byte-bounded
queues, and a drain that hands the socket one message at a time until it
holds a tail. The event loop wakes for ready sockets only: libwebsockets,
the transport before, walked every socket of the process on each wakeup in
`poll()`, which cost it 28 % more CPU on a room's traffic and 12.7 % more
of a room machine per player (the game's `room-ws-stack-2026-09-28.md`).
`net_ws_server_service()` runs one bounded loop pass (`src/net_us_loop.c`)
and repeats it while a zero-timeout pass still found something ready. A
caller on a fixed tick may sleep to it and service with no wait, so a
busy server wakes once a tick instead of on every packet:
`net_ws_server_receive_age()` still tells when each message came in, and
`net_ws_server_needs_events()` says when it must not wait (a handshake,
a connection before HELLO or closing, output the socket has not taken).
On a 64-room machine this cut the rooms' CPU by a quarter (the game's
`tick-only-reads-2026-10-01.md`). TLS is opt-in: built with `NETWORK_CORE_WITH_TLS`
(OpenSSL on the box) a server given PEM files speaks wss:// itself, which
takes the proxy out of the game path: TLS 1.2 and 1.3 only, forward-secret
AEAD suites, the full chain from the certificate file; built without, the
same config refuses to create, so a plain room never poses as a secure one.
The native client speaks wss:// through libcurl, with the box's own trust
store (Schannel on Windows, OpenSSL elsewhere) and no second TLS library;
`tls_insecure` in its config accepts a stand's own CA; browsers speak
wss:// by themselves.

Every server of a thread shares that thread's uWebSockets loop (it keeps one
per thread): servicing one server runs the others' sockets too, and a
server destroyed from inside a callback is freed after the pass. On Windows
a pass waits at the system timer's resolution (about 15 ms) when idle; a
ready socket still ends it at once.

## Layout

```text
include/      public headers
src/          net_codec.c, net_ws_server_uws.cpp, net_us_loop.c (a bounded
              uSockets pass), net_ws_client_curl.c, net_ws_client_web.c,
              net_queue.h (internal ring)
tests/        roundtrip.c (server + native client in one process),
              idle_ping.c (raw-wire empty PING, PONG and idle disconnect),
              curl.cmake (a bare consumer's libcurl from the engine's deps),
              integration.test.mjs (builds a bare consumer, checks the
              UPSTREAM.*.json records)
vendor/uwebsockets/  uWebSockets v20.80.0 with uSockets, Apache-2.0;
                     see UPSTREAM.uwebsockets.json
vendor/libuv/        libuv v1.53.0, MIT, Windows only; see UPSTREAM.libuv.json
```

## Origin

uWebSockets v20.80.0, https://github.com/uNetworking/uWebSockets, with its
pinned uSockets, Apache-2.0: the headers without HTTP/3 and the client and
cluster helpers, uSockets' core, its epoll and libuv loops and its OpenSSL
layer. `UPSTREAM.uwebsockets.json` records both revisions and per-file
hashes.

libuv v1.53.0, https://github.com/libuv/libuv, MIT: the headers, the common
core and the Windows backend, which is where uSockets runs on Windows; the
Unix backend is left out, Linux runs uSockets on epoll.
`UPSTREAM.libuv.json` records the revision and per-file hashes.

Both trees are byte-for-byte upstream (`-text` in `.gitattributes`).

## Purpose

Provide the reusable WebSocket transport, bounded codec and session limits an
authoritative room and its native and browser clients share, and nothing
game-shaped above them.

## Public surface

The headers and capabilities listed by `feature.json.provides` are public:
`net_ws_server.h`, `net_ws_client.h`, `net_codec.h` and the two CMake
targets. `src/net_queue.h` and the vendored library are private.

## Validation

Run `node --test features/network-core/tests/integration.test.mjs` from the
Studio root (it builds a bare consumer, runs `tests/roundtrip.c` and
`tests/idle_ping.c` with UBSan on supported non-Windows compilers, and checks
the `UPSTREAM.*.json` records against the vendored bytes), then
`node features/validate_contracts.mjs`, then the consuming game's
`node tools/game.mjs test`.

## Compatibility

`feature.json.version` is exact SemVer. PATCH preserves the public contract,
MINOR adds backward-compatible surface (a new config field with a zero
default, a new query), and MAJOR permits breaking changes (a changed callback
signature, a changed close-code meaning, a bumped `NET_HELLO_MAGIC`).
Consumers pin both this version and an exact repository revision.

## Extension points

- `protocol_version` and message types from `NET_MSG_APP_FIRST` up are the
  game's; the transport never interprets them beyond this: after HELLO, the
  first byte of every message a client sends is its type and must be at
  least `NET_MSG_APP_FIRST`. The server closes a client whose message starts
  with 0 (the reserved HELLO type) with `NET_CLOSE_FORMAT` (4004) before
  `on_message` sees it, so a payload that can begin with a zero byte (a
  varint id of 0, a small count) needs a type byte in front of it.
- `net_ws_server_close(..., code)` carries any application code; 4100-4999 is
  the suggested game range.
- `overflow_policy` on the client and `net_ws_server_queued_bytes` on the
  server are where a game plugs its own backlog policy (coalescing, deltas
  with resync, or a hard close).
- TLS is opt-in (`NETWORK_CORE_WITH_TLS`); without it, put a TLS-terminating reverse proxy in front
  of the room.
