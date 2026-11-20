# Coop net

## Purpose

A co-op session on any `net-link` transport: who sits where, whether two
builds may play together, and three kinds of traffic between the host and each
guest. The host is seat 0; a guest talks to the host only, and what one guest
must know about another the game forwards through the host.

The session knows no world, no physics and no input. Every payload is the
game's bytes; `body-sync` and the game put their records in them. The game owns
the loop: nothing calls back, and nothing is allocated after the session is
created.

Bytes from a peer are untrusted. A packet the session cannot parse, or one that
breaks its rules, closes that peer with `COOP_CLOSE_FORMAT`. It never asserts on
peer data and never reads outside a packet. The session's own framing is
written and read with `net-bits`, so every refusal follows one rule set.

Depends on `net-link` (public) and `net-bits` (private) only.

## Public surface

One header, `features/coop_net/coop_session.h`; its comments are the
contract.

| Group | Calls |
|---|---|
| Create | `coop_session_lanes` (open the link with these), `coop_session_host`, `coop_session_join`, `coop_session_destroy` |
| Seats | `coop_session_is_host`, `_seat`, `_seats`, `_tick_hz`, `_seated`, `_set_open`, `_kick`, `_set_world_key` (host) |
| Time | `coop_session_set_time`, `_tick`, `_end_tick`, `_set_idle_ticks`, `_peer_tick` |
| Events | `coop_session_poll`: `WELCOME`, `SEAT_JOINED`, `SEAT_LEFT`, `CLOSED`, `STATE`, `RECORD`, `BASELINE` (with `records_before`) |
| Sending | `coop_session_send_state` (unreliable, numbered), `_send_record` (reliable, ordered), `_send_baseline` (host), `_baseline_progress`, `_state_max`, `_record_max`, `_packet_fate` |
| Measures | `coop_session_seat_stats` (RTT, its deviation, loss, counters), `coop_session_peer` (for `net_link_stats`) |
| Reasons | `COOP_CLOSE_*` app codes, `coop_close_name` |

| Lane | Kind | Carries | Priority when the path is short of room |
|---|---|---|---|
| 0 `COOP_LANE_STATE` | unreliable | state packets, the common traffic: Steam adds a small overhead to every message on a lane other than 0 | second |
| 1 `COOP_LANE_RECORDS` | reliable | handshake, seat notices, records | first |
| 2 `COOP_LANE_BASELINE` | reliable | baseline pieces | last |

Results a caller can rely on:

- `STATE` packets may be missing and out of order, never twice. `RECORD`s of one
  seat come once and in the order sent. `SEAT_JOINED` precedes anything from a
  seat and `SEAT_LEFT` is its last event.
- Event `data` stays readable until the next `coop_session_poll`.
- Send results: `INVALID` for no bytes or a call the role lacks; `NO_SEAT` for
  an empty seat, the host's own seat as a target, a guest's seat other than 0,
  or a guest not yet welcomed; `TOO_LARGE` past the limits (a record and a
  baseline piece never exceed `COOP_RELIABLE_CAP`, 16 KiB, whatever the link
  allows; a baseline never exceeds either side's `baseline_max`); `FULL` and
  `DROPPED` as the link says. A `DROPPED` state packet still takes its number
  and ends `LOST`.
- Each side's tick is its own count from `config.tick`, never adjusted;
  `WELCOME` reports the host's tick only as information.

What two builds share: they must step at the same `tick_hz` and read each
other's record tables and baselines; fold all of it, with the content, into
`build_key`. `seats` is the host's alone. Each side's `baseline_max` is its own:
a guest's travels in its hello and the host refuses a larger baseline for it.

## The tick a game writes

```c
coop_session_set_time(session, now_s);            /* the game's clock, never going back */
coop_event_t event;
while (coop_session_poll(session, &event)) {
    /* WELCOME, SEAT_JOINED, SEAT_LEFT, CLOSED, STATE, RECORD, BASELINE */
}
step_world();
for each seat the game sends to:
    coop_session_send_state(session, seat, bytes, size, &number); /* as often as the game likes */
    coop_session_send_record(session, seat, bytes, size);         /* when something must arrive */
coop_session_end_tick(session);  /* empty state packet to every seat idle too long, flush, tick + 1 */
```

Nothing a game sends leaves before `coop_session_end_tick`. The session sends
its own handshake and notices from inside `poll`; a refusal or a kick goes out
at once through `net_link_close`. Because `end_tick` sends an empty state
packet to every seated path this side sent no state packet on for the idle
ticks, acknowledgements, fates and round-trip samples flow whatever the game
sends; the receiver delivers no event for an empty packet. The idle ticks are 1
by default (every tick) and the game's to change with
`coop_session_set_idle_ticks`, 1..16: a game on a weak channel that sends every
other tick sets 2 and the session adds nothing. The bound keeps the peer's
packets since the last acknowledgement, even two a tick, within the 33 one
acknowledgement names; the longer the wait, the fewer acknowledgements name each
packet (at 16 and 5% loss about one delivered packet in 200 reads `LOST`).

The loop must poll every tick for the round-trip measure to hold: a sample
taken in a poll that came more than 50 ms (or three ticks) after the last poll
that emptied the queue is skipped, since that side measured its own wait.

## Wire format

Bit order is net-bits': the first bit is the low bit of byte 0, a value of
several bits goes low bits first. Every packet starts with a 4-bit kind. Fields
are listed in order with their width in bits; `int[a..b]` is a net-bits whole
number in that range (`value - a` in `net_bits_width(b - a + 1)` bits). After the
fields come zero bits to the next byte boundary, then the kind's bytes. 64-bit
values go as two 32-bit halves, low half first.

| Kind | Lane | From | Fields after the kind | Header bytes | Then |
|---|---|---|---|---|---|
| 1 HELLO | 1 | guest | magic 32 = `0x504F4F43`, protocol 16, build_key 64, player_key 64, baseline_max 32, world key size `int[0..256]` 9 | 28 | world key, exactly its size |
| 2 WELCOME | 1 | host | seat `int[0..7]` 3, seats `int[1..8]` 3, host tick 32, tick_hz 16, world key size `int[0..256]` 9 | 9 | the host's world key |
| 3 JOINED | 1 | host | seat `int[0..7]` 3, identity 64 | 9 | nothing |
| 4 LEFT | 1 | host | seat `int[0..7]` 3, reason 32 | 5 | nothing |
| 5 RECORD | 1 | both | sender tick 32 | 5 | payload, 1 byte or more |
| 6 STATE | 0 | both | number 16, sender tick 32, has_ack 1, ack 16, ack mask 32, ack wait 16 | 15 | payload; none in the session's empty packet |
| 7 BASELINE_FIRST | 2 | host | total bytes 32, host tick 32, records_before 32 | 13 | first piece, 1..total bytes |
| 8 BASELINE_PIECE | 2 | host | none | 1 | next piece, 1 byte or more |

The magic is "COOP" as little-endian bytes (`43 4F 4F 50`), but it sits
behind the 4-bit kind, so on the wire every byte of it is shifted by four bits:
a HELLO of protocol 1 starts `31 F4 F4 04 15` (kind 1 in the low nibble of byte
0 and the magic's low nibble above it; the magic's top nibble and the
protocol's low nibble in byte 4). Lanes 1 and 2 are reliable, lane 0
unreliable.

A packet is malformed (the peer is closed with `FORMAT`) when: the kind is 0 or
past 8; it came on another lane or with the other reliability; it came from the
wrong side (a guest sending kinds 2-4 or 7-8, a host sending 1); a field is out
of its range or a read runs past the end; a padding bit is set; JOINED or LEFT
has bytes after its header; a world key's bytes differ in count from its size;
a payload is empty (STATE excepted). Field rules: HELLO's magic must match;
WELCOME needs `seats >= 2`, `1 <= seat < seats`, `tick_hz >= 1`; LEFT never
names seat 0; STATE with `has_ack = 0` carries zero ack, mask and wait;
BASELINE_FIRST needs `total >= 1` and a piece no larger than `total`.

Kind, magic and protocol are the frozen prefix of every protocol version: a
HELLO whose protocol is not `COOP_SESSION_PROTOCOL` is read no further and
refused with `VERSION`.

Rules of the session on top of a well-formed packet, also `FORMAT`:

- Host: a peer's first packet is a HELLO; a seated peer never sends one again.
- Guest: lane 1 opens with exactly one WELCOME; JOINED names a seat below
  `seats` that is not the guest's own and was not announced; LEFT names an
  announced seat; RECORD only after the welcome. A BASELINE_FIRST while a
  baseline is open, a total above the guest's `baseline_max`, a PIECE with no
  baseline open or past the total.
- Lanes do not wait for each other, so a STATE or a baseline may overtake the
  welcome. A STATE before the welcome is dropped as if lost; a baseline is put
  together and handed out right after the welcome.

## Handshake

1. The guest's link reports `CONNECTED`; the guest sends HELLO and starts its
   handshake timer.
2. The host keeps peers that connected and have sent no valid hello in a table
   of 32. One non-zero identity holds one entry: a newer connection replaces the
   older, which is closed with `HANDSHAKE`. With the table full, the oldest
   waiting peer is closed with `HANDSHAKE` and the newcomer takes its place;
   `FULL` means only that every seat is taken. What remains: a storm of
   connections with fresh identities, arriving faster than the table turns over,
   can still push an honest peer out before its hello lands (on Steam that needs
   that many accounts); and on a transport where clients name their own
   identity, a connection can replace another player's waiting connection.
3. On the HELLO the host checks, in this order, and closes the peer with the
   first reason that applies: protocol (`VERSION`), build key (`BUILD`), open
   flag (`SHUT`, unless the hello's link identity and player key match a seated
   player), world key when the guest sent one (`WORLD`, byte comparison with the
   host's current key), a seat (`FULL`). The seat is the one of that same
   non-zero identity and player key (a takeover: the old connection is closed
   with `REPLACED`, the host reports `SEAT_LEFT` and the other guests get LEFT),
   else the lowest free seat. Identity zero never takes a seat over. A takeover
   trusts the link identity: safe where the transport authenticates it
   (Steam), not where a client names its own.
4. Accepted: WELCOME to the newcomer, then one JOINED per other occupied seat,
   the host's included (its identity travels as zero; the guest reads the
   host's identity from its own link). Every other guest gets JOINED for the
   newcomer; the host reports `SEAT_JOINED` with the player key. Guests are not
   told each other's player keys and read 0.
5. A refused guest reads the reason as `CLOSED`. A guest that gets no WELCOME
   within `handshake_timeout_s` closes with `HANDSHAKE`.
6. A seat that ends is reported to the host's game as `SEAT_LEFT` with its
   reason and announced to the other guests as LEFT. A guest's own close code
   is relayed as LEFT unless it is LEFT or a game code (a guest cannot pass
   itself off as kicked); link reasons (`TIMEOUT`, ...) and the host's own
   closes (kick, `FORMAT`, `STALLED`, `REPLACED`) are relayed as they are.
   `coop_session_destroy` closes with `HOST_LEFT` or `LEFT` through
   `net_link_close`, which lingers, and never destroys the link.
7. When a session message (a welcome, the roster, a notice) does not fit a
   guest's reliable queue, that guest is closed with `STALLED`: its view of the
   seats would differ from the host's from then on.

## Acknowledgements and packet fates

Per seat and direction, the receiver keeps the newest state number it got, `R`,
and a 32-bit mask: bit `i` set when `R - 1 - i` arrived. Numbers wrap at
2^16 and are compared modulo 2^16 (`a` is newer when `(int16_t)(a - b) > 0`). A
packet is taken when it is newer than `R` (the mask shifts), or within the 32
before `R` and not yet in the mask. `R` itself or a packet with its bit set is a
duplicate, and one more than 32 behind `R` cannot be placed: both are dropped
and counted in `state_stale`.

Every state packet carries the sender's tick, and the receiver keeps the newest
tick of any packet it took (compared as a signed 32-bit difference). A packet it
cannot place by its number restarts the window only when its tick is newer than
that: after a long one-way silence the sender's numbers may have come round, and
its fresh packets would otherwise read as old. A packet ahead by its number but
stamped before that tick is a copy from more than 2^15 packets ago and is
dropped. A late copy always carries an old tick, so the network can never make a
packet come twice, whatever the send rate; a peer back from a long freeze
carries newer ticks and is taken at once. The promise is against the network: a
peer that forges its own stamps only disturbs its own path.

Every state packet carries `has_ack`, `ack = R`, the mask, and the ack wait: how
long `R` waited on this side before this packet left, in quarter milliseconds,
`0xFFFF` when it does not fit (no round-trip sample then).

The sender keeps the fate of its last `COOP_FATE_WINDOW` (256) packets. On an
acknowledgement that names a packet it sent within that window:

- `ack` and every packet named in the mask become `ARRIVED`, if still
  `PENDING`;
- the newest `ack` seen only moves forward (one that aged out of the window
  counts as none); every `PENDING` packet more than 32 behind it becomes
  `LOST`. A reordered, older acknowledgement could still name such a packet;
  calling it `LOST` errs on the safe side;
- a packet that ages out of the window answers `LOST`.

`ARRIVED` and `LOST` are final. A bit is set only for a packet handed to the
other side's game (or an empty one taken by its session), so `ARRIVED` is never
wrong; a delivered packet whose every acknowledgement was lost reads `LOST`,
the safe direction. So does each packet beyond the newest 33 that a receiver
took in one poll (after a stall): one redundant resend. `loss` in the seat stats
is lost over resolved packets in the window, the session's empty ones too.

## Round trip

The first acknowledgement that names packet `p` as its `ack` gives one sample:
`now - sent_at(p) - ack_wait`. Smoothing is RFC 6298's: the first sample sets
`rtt = s`, `dev = s / 2`; then `dev = 3/4 dev + 1/4 |rtt - s|` and
`rtt = 7/8 rtt + 1/8 s`. Time is only what `coop_session_set_time` gave. What
the estimate reads:

- Each one-way delay rounds up to the next poll of the side it reaches, so the
  estimate reads up to about two frames above the path: a 50 ms path at 60 Hz
  reads 66.7 ms.
- Under jitter, samples come only from packets that became the newest, which
  are the early ones, so the estimate can read a few ms under the path's mean.
- A side whose poll came more than 50 ms (or three ticks at a slow tick rate)
  after its last poll that emptied the queue measured its own stall: samples
  taken in that poll are skipped.
- A guest reports its hello-to-welcome round trip until its first state
  sample.

## Peer tick

`coop_session_peer_tick` exists on both roles, one clock per path: the host
plays guest-owned bodies on the guest's timeline. Every state packet taken
(empty ones too) stamped with tick `T` and polled at local time `t` gives an
offset `T - t * hz`, with `hz` the host's `tick_hz`. The least-delayed packet
gives the largest offset. The clock keeps the largest offset of each of the last
eight buckets of 1/8 s and reads that maximum plus `now * hz`: the tick of the
newest packet that has had time to arrive, carried forward. It adds no
round-trip term, so it trails the sender by the path's shortest one-way delay.
Stamps are unwrapped against the previous one, so the value keeps counting past
2^32.

Measured limits (`test_peer_tick_on_both_sides`, against the sender's tick one
delay ago): within about a tick on a steady path, a little more with jitter,
and up to one more local frame since a packet waits for this side's poll; a
sender stepping at another rate than `tick_hz` drifts by up to one second times
the difference; a faster path lands at once as a jump forward, a slower one
shows only after up to one window (about a second) and then lands as one step
back; when the sender stops stepping the value runs on, and steps back by the
length of the pause once its packets resume. A forged stamp never makes it
negative.

## Baselines

`coop_session_send_baseline` copies the bytes into the seat's buffer (taken at
create: `baseline_max` per guest seat on the host, one on a guest). Each
`end_tick` hands the next pieces, each at most `COOP_RELIABLE_CAP` with its
header (less on a link with a smaller reliable limit), to the link while it
keeps less than 64 KiB of reliable bytes queued for that guest (always at least
one piece). The baseline has left when the last piece is handed over; until then
a second one returns `FULL`. A seat that leaves drops its baseline in flight.
The guest puts the pieces together in its own buffer and hands out one
`BASELINE` event with the host tick of the send. `coop_session_baseline_progress`
reports bytes handed over (host) or received (guest).

Records overtake a baseline in flight, since lanes do not wait for each other,
and the session does not hold them back. BASELINE_FIRST carries how many records
the host had sent to that seat before the call; the event reports it as
`records_before`. The guest counts the RECORD events from the host since
WELCOME: the first `records_before` of them precede the baseline's cut and are
already inside it, the later ones follow it and must be applied after it.
Between WELCOME and BASELINE, state packets describe a world the guest does not
hold yet.

## Memory

All of it is taken in `coop_session_host` / `coop_session_join`: about 24 KB of
fixed state for 8 seats (2.6 KB per seat, mostly the fate window, and the
table of 32 waiting peers), one scratch
buffer of the larger of the state limit and `COOP_RELIABLE_CAP`, and the
baseline buffers above.

## Validation

All four suites run on `net_link_sim` in virtual time, label `core`:

- `test_coop_wire`: every kind round trips; header sizes against their bits;
  every truncation, wrong lane, reliability and direction, trailing byte, set
  padding bit and field rule is refused; the empty state packet is accepted;
  another protocol's HELLO is read to its number only; 40 000 seeded buffers
  with guard bytes and exact-size heap copies.
- `test_coop_session`: four seats and the roster on every side, a fifth guest
  refused with `FULL`; `BUILD`, `WORLD`, `VERSION` (hand-built HELLO), a guest
  with no world key gets the host's byte for byte, a new world key; takeover,
  identity zero never taking over, kick with a game code, shut host with a
  returning player; host and guest destroy, link timeout, handshake timeout on
  both sides, a guest that cannot connect; waiting peers, the table alone (full,
  a repeated identity, identity zero) and two nameless guests in one tick;
  `STALLED` for a notice and for a welcome; the event queue at its worst case (a
  takeover whose notices stall six guests, then a kick while they are handed
  out; every seat kicked between two polls; a stall cascade; a full link batch);
  records refused before a baseline not counted in `records_before`; the
  leave-code relay; time that goes back; nothing leaves
  without `end_tick`; exact size limits; one baseline at a time and the guest's
  limit; role and argument errors; configs out of range; every close name.
- `test_coop_traffic`: receive and send windows (the stamp rule for a restart,
  several packets a tick, a long one-way silence),
  loss over resolved packets, RTT smoothing, the clock alone (best per bucket,
  pause, wrap); 13 000 records once and in order at 5% loss with jitter,
  duplicates and reorder, three seats, both ways; 70 500 state packets across
  the 16-bit wrap where every `ARRIVED` was delivered, every delivered one ends
  `ARRIVED`, no fate flips; fates that resolve with a game that sends no state;
  a `DROPPED` packet that keeps its number; no packet twice on a network with
  30% duplicates, reorder by 2.5 s and a host that pauses; a receiver frozen
  past the 16-bit wrap that takes fresh packets at once; idle ticks that leave
  the packet rate to the game; RTT within its band on six path
  settings, with a side that steps ten times a second and at a 10 Hz tick rate;
  the handshake seed; a stalled side and a loop that stops reading; the peer tick on both roles within bands at 59.5, 60 and 60.5
  Hz and jitter up to 60 ms; a 150 KB baseline whole under loss and a rate limit
  behind records; a 500 KB baseline on Steam-sized link limits with records
  flowing; `records_before` and progress; a baseline of a leaving seat dropped.
- `test_coop_hostile`: a raw link throws garbage, cut packets and well-formed
  illegal ones at a host (before and after its handshake) and at a guest (from
  a raw host link); each offender is closed with `FORMAT`, the session goes on
  and the seats that behave keep their traffic; a peer that replays its numbers
  and stamps is heard once, and a forged stamp never makes its peer tick
  negative.

`node features/validate_contracts.mjs` checks this contract.

## Compatibility

`feature.json.version` is exact SemVer. A PATCH fixes behaviour to match this
contract without changing a byte on the wire. A MINOR adds calls, events at the
end, close codes or config fields whose zero keeps the old behaviour. A MAJOR
changes the wire (any layout above, the kind codes, the lanes, the frozen HELLO
prefix, the ack rule), the meaning of an event, a result or a close code, or a
call. A change to the wire also raises `COOP_SESSION_PROTOCOL`, so builds on
different protocols refuse each other with `VERSION`.

## Extension points

A game chooses the transport, the seats, its close codes
(`COOP_CLOSE_GAME_FIRST..LAST`), the build key (content hash mixed with its
net-bits table hashes and tick rate), the world key, the baseline size and the
handshake timeout, and decides what to forward between guests, what to put in
each state packet and what to do with a fate. Install: [INSTALL.md](INSTALL.md).
