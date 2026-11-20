# Net link

## Purpose

One endpoint of a message transport: a host that accepts peers or a guest that
reaches one host. A link moves opaque bytes on numbered lanes, reliable or
unreliable, and reports peers coming and going with a reason. Seats, sessions,
ticks and game data live in the layers above (`coop-net`, `body-sync`). The
shape is `ISteamNetworkingSockets`', so a Steam link is a thin mapping.

The feature has three parts and no dependency beyond libc:

- the contract every transport answers (`net_link.h`, `net_link_transport.h`);
- an in-process network simulator for tests and stands (`net_link_sim.h`);
- a capture file written by the front calls, so it works on any transport
  (`net_link_capture.h`).

## Public surface

| Header | What it gives |
|---|---|
| `features/net_link/net_link.h` | `net_link_send`, `poll`, `flush`, `close`, `destroy`, size limits, lane count, `stats`, `identity`. Events: `CONNECTED`, `MESSAGE`, `DISCONNECTED` with a reason; each carries the peer's identity. |
| `features/net_link/net_link_transport.h` | For a transport author: embed `net_link_t` first, fill it with `net_link_init` and an ops table. The front calls check lanes, flags, empty messages, size limits and null arguments before an op runs. |
| `features/net_link/net_link_sim.h` | `net_link_sim_create` (one allocation), a host link, guest links, `advance` (time belongs to the caller), per-direction conditions and a cut. |
| `features/net_link/net_link_capture.h` | `NLCP` file, version 1: one record per message sent or polled (time, direction, identity, tag, lane, kind, size, optional bytes) and a reader that treats the file as untrusted. |
| `tests/net_link_contract.h` | The contract suite: cases written against `net_link.h` alone, run through an environment of function pointers. A Steam link runs the same cases in real time. |

Behaviour every transport keeps:

- Reliable messages arrive once and in order inside their lane, or the peer
  disconnects; one lane never waits for another. Lanes with the same priority
  number share the path.
- `net_link_poll` may return fewer events than asked while more wait: poll
  until it returns zero.
- `net_link_close` sends what is queued for that peer. The reliable messages
  sent before it still arrive, then the other side reads the app reason
  (`1001..1999`) or `REMOTE_CLOSE`. This holds when the closing side destroys
  its link right after. The range is Steam's without its generic code 1000,
  which Steam sends for a close without a reason.
- `net_link_destroy` does not linger for peers it was not asked to close.
- A peer handle is never handed out twice by a link; a stale or foreign handle
  gives `NO_PEER`, `false` or `0`.
- Each event carries the peer's identity. It is zero when the other side never
  answered (`UNREACHABLE`, `TIMEOUT` while connecting); `REFUSED` carries the
  host's.
- Identity is not a player. A returning guest has the same identity as before,
  and two windows of one account share one identity too (Steam IP mode on one
  PC). A session must not take identity alone as "the same player came back".

## Simulator model

What it models, per direction of each host-guest path:

- one-way delay plus an even jitter draw per packet; independent loss;
  duplicates and a reorder hold-back for unreliable packets;
- reliable retransmission after `retransmit_ms` (repeatable, no backoff), with
  the receiver holding later messages of that lane only;
- a token bucket per direction (20 ms of the rate as burst). Waiting packets
  leave by lane priority; lanes of equal priority share it first in, first out
  by send order;
- a cut: transmissions stop, unreliable packets are lost, reliable ones retry;
  a cut of `timeout_s` disconnects both ends with `TIMEOUT`;
- back-pressure: queues count what was sent and not yet polled; past them an
  unreliable send is `DROPPED` and a reliable one `FULL`.

Guarantees: the same seed, calls and times give the same deliveries. Each
direction of each guest place draws from its own stream, so one guest's
conditions or traffic never change another's. Nothing moves until
`net_link_sim_advance`, and nothing leaves before `net_link_flush`. All memory
is taken at create: about 3.4 MB per guest place with the default queues.

What it does not model:

- burst loss, bandwidth on the receiving side, MTU or fragmentation, packet
  headers, acks, congestion control or Nagle. One message is one packet, and
  the rate counts payload bytes only. Retransmissions are not charged to the
  rate;
- control traffic (connect, accept, close) has the delay and waits out a cut,
  but it is never lost or jittered;
- a timeout comes only from a cut. A reliable message on a path with loss 1
  waits forever, and a close lingering on such a path keeps its guest place and
  the host's peer until the host polls a leave that never comes. A dead path in
  this simulator is a cut, not loss 1;
- a packet already on the wire when a cut starts still arrives.

Simulator rules a test may rely on, and a game must not:

- A host peer keeps its place until the host has polled its `DISCONNECTED`.
- A guest link closed before it is destroyed keeps its guest place until the
  host has polled that close; `net_link_sim_guest` skips the place until then.
- Destroying a guest link without a close drops whatever the host has not
  polled from it yet.
- The host sees `CONNECTED` one to-host delay after the guest's first flush;
  the guest sees it one to-guest delay later.

## Validation

`test_net_link_sim` runs the contract suite twice on the simulator (clean, and
40 ms delay with 25 ms jitter), then the simulator cases: lane independence,
exactly-once reliable delivery under 30% loss with jitter, duplicates, reorder
and a cut, loss share within a band, one seed one trace, flush, delay bounds
for copies and held packets, jitter spread, rate and priority for both kinds,
back-pressure, messages that overtake the accept, a close that lingers past
destroy, cuts and timeouts, stats and handles.
`test_net_link_capture` checks the capture against a recorded session and
refuses cut and garbage files. Both carry the CTest label `core` in the
feature consumer, and `node features/validate_contracts.mjs` checks this
contract.

## Compatibility

`feature.json.version` is exact SemVer. A PATCH fixes behaviour to match this
contract without changing it. A MINOR adds calls, event fields at the end,
simulator conditions or capture flags with a backward-compatible default. A
MAJOR changes a call, the ops table, the event layout, the meaning of a
reason or result code, or the capture format version. The capture reader
reads version 1 only.

## Extension points

A new transport fills `net_link_ops_t` and passes `tests/net_link_contract.h`
with its own environment: `step_s`, `settle_s`, `fail_s`, how many guest links
it holds at once, and an optional cut hook with its timeout. A game sets the lane
count and priorities, its own close codes in the app range, queue sizes and
conditions per guest. Install: [INSTALL.md](INSTALL.md).
