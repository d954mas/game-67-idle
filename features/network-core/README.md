# network-core

WebSocket transport for authoritative game rooms, compiled in place by every
consumer.

## What it does

- **Server** (`net_ws_server.h`, native only): one `lws` context, one thread.
  `net_ws_server_service(server, timeout_ms)` runs accept, reads, writes and
  callbacks, and returns by the timeout so a fixed simulation tick can share
  the thread. No thread per connection, no lock around the world.
- **Client** (`net_ws_client.h`): the same header on native (a client on
  a thread of its own: libcurl's WebSocket where the engine built it in
  with `NT_HTTP_WEBSOCKETS`, libwebsockets otherwise) and web
  (`emscripten/websocket.h`, the page's own `WebSocket`). In all the
  socket is read the moment the
  network delivers, so every message carries its exact arrival time
  (`received_at`, on `net_ws_client_clock()`, the performance counter on
  Windows and CLOCK_MONOTONIC elsewhere), pongs go out during a loading
  stall and sends never wait for the next frame, or for 100 ms at most
  when the wake pipe is unavailable. Callbacks still fire only from
  `net_ws_client_service()`, on the caller's thread, in the order the
  socket saw them.
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
  hears about them. A socket that never sends HELLO, or never acknowledges a
  close, is dropped after `handshake_timeout_ms`, and so is a socket that
  stalls in the TLS or HTTP handshake. Accepts are budgeted at `max_clients`
  per second (burst of twice that) and sockets short of a seat at
  `4 * max_clients + 8`, so a connect flood costs the service thread one
  handshake per seat per second and never the process's fd limit. A slot
  (and the client id) is earned by HELLO: sockets upgraded but not yet past
  HELLO wait in a room of `max_clients`, at most four of them from one
  address, so holding open connections without HELLO takes no seat and
  one machine cannot fill the waiting room. A message in more than eight
  fragments is closed as malformed. Client ids are never reused within a
  server lifetime. HELLO may carry a ticket of up to
  `NET_HELLO_TICKET_MAX` opaque bytes (a seat to resume, a join code); the
  transport hands it to `on_connect` and attaches no meaning to it.
- **Liveness is the server's**, because browsers cannot send pings: with
  `ping_idle_s` set, a protocol ping goes out every `ping_idle_s` after the
  last pong, and a peer whose pong is still missing `hangup_idle_s` after
  that pong is dropped as `NET_WS_CLOSE_PEER`. Only pongs count as life, so
  the window for the pong is the difference of the two. Browsers answer
  pings natively, even from a hidden tab, so a backgrounded page stays a
  client while a dead network does not. The native client keeps lws' own
  40 s ping / 50 s hangup toward the server; the server answers inside its
  service loop.
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

No permessage-deflate (extensions are compiled out), binary frames only,
`TCP_NODELAY` (lws default), one preallocated `LWS_PRE` scratch buffer per
side, per-connection byte-bounded queues, and a drain loop that writes until
the socket is choked. TLS is opt-in: built with `NETWORK_CORE_WITH_TLS`
(OpenSSL on the box) a server given PEM files speaks wss:// itself, which
takes the proxy out of the game path: TLS 1.2 and 1.3 only, forward-secret
AEAD suites, the full chain from the certificate file; built without, the
same config refuses to create, so a plain room never poses as a secure one.
The native client speaks wss:// through libcurl, with the box's own trust
store (Schannel on Windows, OpenSSL elsewhere) and no second TLS library;
`tls_insecure` in its config accepts a stand's own CA. The lws client, the
fallback of a build without libcurl's WebSocket, stays plain ws://;
browsers speak wss:// by themselves.

## Layout

```text
include/      public headers
src/          net_codec.c, net_ws_server_lws.c, net_ws_client_lws.c,
              net_ws_client_web.c, net_queue.h (internal ring)
tests/        roundtrip.c (server + native client in one process),
              integration.test.mjs (builds a bare consumer, checks UPSTREAM.json)
vendor/libwebsockets/  pruned v4.5.8, MIT; see UPSTREAM.json
```

## Origin

libwebsockets v4.5.8, https://github.com/warmcat/libwebsockets, MIT. The
vendored tree keeps only what the option set in `CMakeLists.txt` compiles:
core, core-net, poll event loop, ws/h1/http/listen/pipe/raw-skt roles, unix and
windows platform layers, misc helpers, `win32port/win32helpers`, and
`lib/tls` (common + OpenSSL backend) for the opt-in TLS build. Removed:
mbedTLS, HTTP/2, secure streams, extensions, plugins, mqtt/dbus/cgi/netlink
roles, jose/cose, display-list and image decoders, test apps and examples.
`UPSTREAM.json` records the pinned revision and per-file hashes.

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
Studio root (it builds a bare consumer, runs `tests/roundtrip.c` and checks
`UPSTREAM.json` against the vendored bytes), then
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
