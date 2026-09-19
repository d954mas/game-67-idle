# network-core

WebSocket transport for authoritative game rooms, compiled in place by every
consumer.

## What it does

- **Server** (`net_ws_server.h`, native only): one `lws` context, one thread.
  `net_ws_server_service(server, timeout_ms)` runs accept, reads, writes and
  callbacks, and returns by the timeout so a fixed simulation tick can share
  the thread. No thread per connection, no lock around the world.
- **Client** (`net_ws_client.h`): the same header on native (libwebsockets
  client on a thread of its own) and web (`emscripten/websocket.h`, the
  page's own `WebSocket`). In both the socket is read the moment the
  network delivers, so every message carries its exact arrival time
  (`received_at`, on `net_ws_client_clock()`), pongs go out during a
  loading stall and sends never wait for the next frame. Callbacks still
  fire only from `net_ws_client_service()`, on the caller's thread.
- **Codec** (`net_codec.h`): bounded little-endian reader/writer. A read past
  the end clears `ok` and returns zero; the caller checks once at the end.
- **Session rules every room inherits**: the first frame must be HELLO with
  the configured protocol version; text frames, oversize messages, reserved
  message types, a flooding sender and a client that cannot drain its queue
  are all closed with a `NET_CLOSE_*` code (4001-4006) before the application
  hears about them. A socket that never sends HELLO, or never acknowledges a
  close, is dropped after `handshake_timeout_ms`. Client ids are never reused
  within a server lifetime. HELLO may carry a ticket of up to
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
the socket is choked. TLS is deliberately absent: rooms sit behind a
TLS-terminating reverse proxy on the public domain.

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
windows platform layers, misc helpers, `win32port/win32helpers`. Removed:
TLS, HTTP/2, secure streams, extensions, plugins, mqtt/dbus/cgi/netlink
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
  game's; the transport never interprets them.
- `net_ws_server_close(..., code)` carries any application code; 4100-4999 is
  the suggested game range.
- `overflow_policy` on the client and `net_ws_server_queued_bytes` on the
  server are where a game plugs its own backlog policy (coalescing, deltas
  with resync, or a hard close).
- TLS is intentionally absent; put a TLS-terminating reverse proxy in front
  of the room.
