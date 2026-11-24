# Body sync

## Purpose

Two things for co-op physics on Box3D, both without any game in them:

- **The sync seam** (`body_sync`): the narrow interface through which one
  endpoint's bodies are written to bytes and applied from bytes. Bodies carry
  network ids with a generation, an authority (the seat that simulates them)
  with a number that grows on every change, an optional holder and a few state
  bits. The game moves the bytes, usually over `coop-net`, and keeps the
  policy: what may be grabbed, who wins a dispute. Two implementations keep
  the one contract, and a program links one:
  - **`body_sync`, the real seam**: one bit-packed state packet per tick and
    seat under a byte budget, bodies picked by priority, rest poses and rare
    fields held until acknowledged, and a naive receiver that applies the
    newest pose (see "The real seam" below);
  - **`body_sync_stub`, the baseline**: every bound body's raw pose,
    velocities and motion type to every seat, every tick, in as many packets
    as it takes. It is the reference for the contract and what the stand
    measures the real seam against.

  Both decide authority with the same code (optimistic claims, each with the
  claimant's sequence, answered one by one by the host).
- **The stand** (`body_sync_stand`): a host and up to three guests in one
  process, each with its own Box3D world built from one scene, its own seam and
  its own `coop-net` session, joined by a seeded `net_link_sim`. A text
  scenario schedules player actions and network conditions; the run writes
  metrics as JSON and checks invariants at its end. Without a window it runs in
  CTest; a drawer (a window, a game's own target) walks one endpoint's bodies
  through the stand's view calls.

Worlds step at 1/60 s with 4 sub-steps and gravity 12 m/s², single-threaded.
The network runs on virtual time and the host's random draws come from the
seed, so one scenario with one seed on one build gives byte-equal metrics, in
one process or in two.

## Public surface

| Header | What it gives |
|---|---|
| `features/body_sync/body_sync.h` | The seam: `create` with the box and speeds a peer's numbers must stay in, seats joined and left, `bind`/`unbind` an id to a `b3BodyId` with its starting authority and number, authority (`claim`, `release`, `poll_claim`, `answer`, `assign`, `set_holder`, `set_flags`), the host's gate on contact claims (`check_contact`, with the claimant's `contact_number`), the tick (`set_peer_tick`, `receive_state`, `receive_record`, `apply`, `collect`, `write_state`, `state_sent`, `unresolved`/`resolve`, `peek_record`/`pop_record`), `render_offset`, `traffic` by kind, the second layer's `is_predicted`, `is_pull_paused`, `is_layer_copy`, `lost_take` and `round_trip`. Includes Box3D. |
| `features/body_sync/body_sync_scene.h` | Scenes as C data: body descriptors (shape, size, mass or density, friction, restitution, linear and angular damping, pose, motion, role, flags, model tag, routes, breakable templates), player spawns, the character's tuning, `validate`, the 8-scene catalog and lookup in it or in a caller's table. |
| `features/body_sync/body_sync_scenario.h` | The scenario grammar (top comment), the parsed struct, `parse` and `load`, which refuse with a line and a reason. |
| `features/body_sync/body_sync_metrics.h` | Metric definitions and names, the report structs, schema `body_sync.metrics.v2`. |
| `features/body_sync/body_sync_stand.h` | `create`, `tick`, `run`, `report`, world hashes, the view a drawer walks (`body_count`, `body`: shape, size, drawn pose, model tag, flags, a character's activity), JSON, `print_report` (the command line's text report), the host rules, the checks and `body_sync_stand_main`. |

What a caller of the seam relies on:

- An endpoint that becomes local for a body continues it from the newest state
  it received: pose, velocities, motion type, motion locks, flags and hold are
  the last owner's. A claim, a grant and a forced move start with no holder; only a seat
  that left hands its bodies back still naming their carrier. The owner changes
  a body's motion type through Box3D; the seam reads it at collect and sends it.
- A claim is told apart by its sequence. The host polls every claim once,
  answers it with `body_sync_answer` (granted with the claim's hold, or
  refused; UNKNOWN means the claim is gone and the event is dropped), and moves
  a body without a claim with `body_sync_assign`; a move to the host's own seat
  takes over within the call, so what the host does to the body next (a recall
  puts it home at rest) is never undone by the next apply. Only the claimant's newest
  claim of a body counts; a released claim is withdrawn; a release that crosses
  a grant leaves the body with the host; a release older than a forced move is
  stale. Matching relies on seam records arriving reliably and in order.
- Every endpoint creates its seam with the same box and speeds (the caller's
  handshake checks that). What the seam writes is clamped into them, so an
  honest body never closes a session; a body leaving the box is the game's
  event to handle.
- The tick order is: peer ticks, receive, apply, the game's actions, the step,
  the game's contact judging, collect (which ends settled holds), sends.
- While a body is a copy the seam owns its type, transform, velocity and
  motion locks: a copy is unlocked, so a rotation-locked body (a character)
  still turns to its owner's rotation.
- How a copy is held and how a body's class is honoured are the
  implementation's; the stub drives kinematic copies and ignores the class.

The stand's headers name no Box3D type: a drawer or a game's scene table needs
no physics headers. The parts of the stand that need no physics (scenes as
data, the catalog, the scenario parser, the action schedule, the host rules'
verdicts, the stand's records, metric arithmetic and JSON) are the separate
library `body_sync_stand_logic` and are tested alone.

Scene catalog: `floor_wall`, `stack10`, `pile60`, `cabinet_mug`, `mannequin`
(one standing, one walking a route), `two_chars_item`, `breakables` (glass,
a crate with contents), `load300` (300 bodies, four players), `dummy_wall`
(a wall and a standing dummy to throw at), `walker_lane` (a mannequin walking
across a thrower's line), `glass_door` (glass and a heavy door, both broken by
an impact claim).

The test character mirrors a game's mechanics: a dynamic capsule moved by
setting its velocity, rotation locked, its facing written into its rotation
as a yaw (so it travels with the pose) and its activity into the seam flags'
bits 1..3 (bit 0 is the knock); carrying makes the body kinematic with
collision mask 0 and teleports it to the hold point each step; a throw makes it
dynamic again with a ballistic velocity and a settled hold; a punch is a cone
test in front, with no physics shape. Host rules stand in for a game's: first
grab claim wins, the loser is refused; a hit claim knocks a mannequin down,
breaks a breakable or pushes an item; an impact of a flying body breaks a
breakable at its `break_speed` and fells a standing mannequin at its
`fall_speed` (5 m/s by default: past a punched item's push, under a throw);
breaking removes the body and spawns its contents and debris from the host's
random draws; what a player who left carried falls where it is.

## The real seam

Six parts decide; the Box3D shell (`src/body_sync_real.c`) only reads bodies,
calls them and moves bodies as they say. None of the parts includes Box3D
(`body_sync_types.h` holds the seam's plain types), so their tests run alone.

- **Codec** (`sync_codec`): record tables on `net-bits`, ranges from the
  seam's limits, precisions fixed. A packet is a run of records, each after a
  1 "more" bit, then a 0 and zero padding; the session's own prefix carries
  the packet number, tick and acknowledgements, so the seam adds no header.
- **Sender** (`sync_sender`): per seat, priorities, the budget, what was sent
  and acknowledged.
- **Receiver** (`sync_receiver`): the newest record per body, newest wins per
  sender tick: what a take-over continues from and what the host relays. A
  copy record has no velocities: a body taken over from a copy continues with
  a velocity estimated from its last two poses.
- **Playback** (`sync_playback`): where each copy is drawn and how it is held;
  see "Playback" below.
- **Policy** (`sync_policy`): every handover the caller does not ask for (a
  flight's end, and under authority following interaction the takes, the
  host's answers, its agents first and the returns at rest); see "Authority
  follows interaction".
- **Authority** (`sync_authority`): claims, answers, releases, forced moves,
  the reliable records, shared with the stub; also where each body's state
  goes and in which record kind, when a settled hold ends, what a holder
  names, and which motion a take continues with.

The shell decides nothing that needs no Box3D; the seam suite
(`test_body_sync_stub.c`) runs against both implementations.

One record, bits:

| Part | Fields | Range and precision |
|---|---|---|
| head | slot 10, generation 4, sim 1, rest 1, rare 1, relayed 1 | slot below 1024 |
| origin (relayed only) | owner seat 2, age 6 | host ticks since the host took the owner's state, 0..63 |
| copy pose | position x y z, rotation | 1/512 m over the limits' box; smallest three at 9 bits |
| sim pose | position x y z, rotation | 1/4096 m over the box; smallest three at 15 bits |
| motion (sim, not at rest) | velocity, spin | 1/32 m/s in ±max_speed; 1/16 rad/s in ±max_spin |
| rare (when its bit is set) | 5-bit group mask, then the groups present | |
| - motion | kinematic 1, locks 6 | |
| - flags | the caller's 4 bits | |
| - authority | seat 2, number 16, hold 2 | |
| - holder | present 1, slot 10, generation 4 | |
| - data | size 4, then that many bytes | 0..8 bytes, the caller's |

On the stand's limits (box 80 x 65 x 80 m, 100 m/s, 200 rad/s) a copy is 96
bits, a moving sim record 201, a rest record 123, a relayed record 8 more,
every rare group 51 more and the data group 4 plus 8 a byte.
The generation goes as its low **4 bits**: a stale record whose generation
matches modulo 16 reads as current; records are newest-wins per sender, so
only a pose sent before a respawn and read after it could pass. The grids hold while every coordinate of the box is below 2048 m in
magnitude (a float cannot keep 1/4096 m apart past it), max_speed below
262144 m/s and max_spin below 524288 rad/s; `body_sync_create` refuses limits
past them. A position grid ends at its last point inside the box and a
velocity grid has no point past its limit; the writer keeps a velocity or spin
vector no longer than its limit on the grid, and a reader refuses a longer one,
so nothing decoded lies outside the limits. Decoded and encoded again,
positions, velocities and rare groups give the same bits and a rotation the same
rotation within its step (near a tie of its two largest components the dropped
one may change). A packet of the end bit alone is legal and empty.

Which record: a guest sends what it simulates as sim records (the host may
take any of them back at any tick, with velocities); the host sends each
guest every body that guest does not simulate as copy records, relaying other
guests' bodies from what it decoded. A rest pose goes out as the precise
record (sim at rest, no velocities) both ways: it is what the worlds agree on
and where a new owner starts. A guest declares its authority number in the
authority group; the host takes its poses only under the current number, and
a guest's records count in that guest's tick order: a late packet neither
counts nor moves the declared number back. The reliable records that move
authority (a grant, a forced move, a release) carry the body's flags, motion
type and locks, so a handover never waits for the state that would carry
them; a new owner that changed them itself since its claim keeps its own.

Priority, per seat: each tick `acc += rate`; a body is due at `acc >= 1` and
a sent body's `acc` goes to 0. A packet takes, within min(budget, capacity):
characters, every packet; bodies unsent for 30 ticks or more, oldest first;
due bodies by `acc`. A record that does not fit is left out whole and counted
(`deferred`); nothing is split.

| Body | rate |
|---|---|
| held, flying (hold until settled), just hit (speed changed by 1 m/s in a tick), or a rare group unacknowledged | 1 |
| any other awake body | 0.25 + 0.75 min(1, (abs(v) + 0.25 m abs(w)) / 2 m/s) |
| asleep, its rest pose not acknowledged | 0.5 |
| asleep and acknowledged | 0, not sent until it wakes |

Asleep means at rest: Box3D keeps a body asleep while the game moves it by
transform (a carried body, kinematic, set to its hold point every tick), so
the seam counts a sleeping body as awake once it leaves the pose it fell asleep
at by the stillness bounds (2 mm, 0.004 rad). Otherwise a carried body would
go out no more once its rest pose was acknowledged, and the peers would show
it left behind its carrier (`carry_gap.max` measures it).

Staleness bound: with M awake bodies and room for N records a packet after
the characters, an awake body goes at most max(4, 30 + ceil(M / N)) ticks
between two packets that carry it, overflow or not. `stale_ticks.max` and
`deferred.count` measure it in a scenario.

Acknowledgement: a packet's rest poses and rare groups are kept for 64
packets per seat under the session's 16-bit number. ARRIVED acknowledges a
rest pose while the body is still in that sleep, and a rare group while it
still has the version the packet carried; LOST, a packet never handed to the
session (no `body_sync_state_sent` before the next write) and one that leaves
the window acknowledge nothing. Numbers match exactly inside the window, far
shorter than the wrap. "Changed" is on the wire: a body that sleeps again on
the quantized pose already acknowledged is not sent again, unless a moving
record of it went out since. A body's priority and age reset when its packet
is handed to the session, not when it is written. Without fates (no
`body_sync_resolve`) nothing is acknowledged: 200 sleeping bodies then cost
about 1 KB a tick for ever, against nothing once acknowledged.

The budget is 1000 bytes (`state_budget`, 0 takes it), clamped by each
write's capacity. `body_sync_create` refuses a budget without room for one
widest record per seat (one character each, the rule's assumption: a game
with more characters a seat sets a budget with room for them) and one more:
the staleness bound
needs room for at least one record after the characters, and that minimum
gives it. A guest's bodies reach the other guests one tick after the host took
them, stamped with their age. One real seam takes about 2.7 MB (1.4 MB of it the
playback buffer) and a stub about 400 KB, fixed at create. The capacity a caller passes is what the transport carries
after its own prefix: `coop_session_state_max`, which is the link's
`max_unreliable` minus the session's header. The seam's reliable records go
on their own lane and are not in the budget; on the stand's heaviest scenes a
state packet and its tick's records together never passed that capacity
(`flush_over_limit` 0), which matters on a transport that splits a message
sharing a flush with others.

Measured on `pile_throw` (60 light bodies and a ball, one guest, seed 5), mean
bytes a second to the guest, stub then real: copy 125 899 / 6 210, sim 88 870
/ 3 387, held 14 812 / 0 (the stub books a fixed share of every record as held;
the only carried body here is the guest's own, which the host does not send back), authority 14 823 / 843, header 720
/ 117; 245 KB/s in 4 packets a tick against 10.6 KB/s in one, divergence p95
0.0096 m against 0.0126 m.

## Playback

A copy is drawn at a playback time behind its owner, from a buffer of what the
owner sent, so a late or reordered packet within the delay changes nothing
drawn, and a lost one costs at most a chord across its gap (a lost snapshot at
a sudden change of motion starts that change a tick early); a cut, a hitch or a
starved body shows as a catch-up the render offset or a dynamic hold smooths
(`src/sync_playback.h` holds every rule and constant).

- **Timelines.** One per owner seat: on a guest the host's bodies play on the
  host's, another guest's (relayed) on that guest's, so a character, what it
  carries and what it threw move together. A relayed record carries its owner
  seat and an age; its sample tick is the packet's tick less the age, the tick
  at which the owner's state lay on the host's clock line for that owner, so
  the first leg's jitter does not reach the second. A state older than 63 host
  ticks is not relayed. Only the host relays: a relayed record from a guest, or
  one naming the host or the receiving seat, is FORMAT.
- **What feeds the clock.** Only records the seam takes (a stale, rejected or
  unknown body's record moves no timeline), and on a guest the host's records of
  a body it still simulates, which are buffered for when it is a copy again: a
  claimant refused plays the owner's states from before and during its claim,
  while a body whose take was confirmed drops the snapshots from before it (it
  simulated on since) and stays where it is until the owner's next state. The
  rare groups those records carry are kept while the claim waits (its hold,
  holder, flags, motion and data, not the pose): the host counts each group a
  packet carried as known once the packet arrived and never sends it again
  unchanged, so a refused claimant dropping them would hold the groups from
  before its take, and take a body the host's agents set going again and again.
- **The far rule.** From a timeline's first sample on, a sample further ahead of
  the clock line than 75 ticks (`delay_max` 15 plus the jump's 60) disagrees with
  it. Alone it is dropped and counted (`rejected`): a forged or corrupt tick would
  otherwise move every copy of that owner. Eight consecutive far samples that
  agree with each other (within `delay_max`) restart the clock on them: an
  owner's time that really moved (a latency drop, a skipped tick, the end of a
  silence) is followed within 133 ms, and the copies catch up by a jump the
  render offset absorbs (`latency_drop`: 1.5 s of delay dropping to 50 ms).
  Time is counted from each timeline's base with
  wrap-aware differences, so the owner's 32-bit tick may wrap.
- **The clock.** Over the last 64 new samples of a timeline: the rate (the
  least-squares slope of sample ticks over local ticks, in [0.5, 2], 1 until
  32 local ticks of samples), the sender's "now" on the line of the fastest
  arrivals, and the jitter as the 95th percentile of the lag behind it. The
  delay is ceil(jitter) + 1 ticks within [`delay_min`, `delay_max`] (4 and 15
  by default, 1..60 in the config). Each tick playback moves by rate x pace,
  pace = 1 + (target - playback - rate) / 20 in [0.9, 1.25], and never passes
  the newest sample: it waits there, it never extrapolates. A duplicate sample
  changes nothing, not even the jitter; a reordered one counts for the jitter.
- **Before the delay is known.** From a timeline's first sample until 16 have
  arrived the delay is `delay_max`: playback starts at newest - 15 and the
  copy holds its first snapshot's pose until playback reaches it. A seat that
  rejoins starts a new timeline the same way, its old snapshots forgotten (a
  seat that leaves keeps them; its clock starts over).
- **Bounds.** Playback never decreases, never passes the newest sample, and
  grows at most rate x 1.25 a tick, except at a jump: more than 60 ticks behind
  its target it jumps to the target (a cut, a long hitch) and says so. A body
  whose newest snapshot is 8 ticks behind playback freezes there until its next
  snapshot comes, then plays on.
- **The buffer.** 16 snapshots a body. Position by cubic Hermite between two sim
  snapshots (their velocities as tangents), linearly otherwise; rotation by
  normalized lerp on the shorter arc; a Hermite point stays inside the box of
  its two snapshots, so a fast body that stopped dead between them (a wall) is
  never drawn past where it stopped. A rest snapshot gives its pose to the bit.
  A full buffer drops the oldest once the next one is played, else thins its
  densest stretch, so a long delay plays coarser, never shorter. Rare groups
  (flags, motion, authority, holder, data) take effect at their snapshot's time
  and stay; a late record keeps only groups newer than those played.
- **Two ways a copy is held.** On a guest, and the host's own bodies anywhere:
  kinematic, driven to the playback pose. On the host, a guest's body that its
  owner simulates dynamically (its character, what it carries or threw): dynamic,
  its velocity the owner's plus the error closed over 3 ticks, at most 5 m/s and
  10 rad/s, so it pushes what the guest's body would push and is held by what
  would hold it; an error past 2 m is a placement. It keeps its owner's locks as
  they change (a character cannot tip over); the part of its owner's rotation
  about its locked axes is set once it is off by half a degree (a spin cannot
  turn them), and the free axes follow the spin. It is kinematic while its owner made the body
  kinematic. The correction on top of the owner's velocity lets it run about
  20 % faster than its owner while it closes an error (2.43 against 2.0 m/s
  measured); the pushes it gives match a local character's (a light box pushed
  to 4.61 m against 4.54 m, a heavy one stopping the character at 0.70 m
  against 0.69 m). A body local again continues from the newest state received;
  one already local keeps its own velocities.
- **The speed cap.** A copy is never driven faster than `max_speed` or turned
  faster than `max_spin`: a pose it cannot reach in one step (a teleport, a
  jump of the timeline, a starved body whose next snapshot is many ticks ahead)
  is a placement, and the render offset takes the move.
- **The render offset.** The drawn pose is physics plus the offset
  (`body_sync_render_offset`); physics never sees it. A placement, a take-over
  and a local body turning back into a copy hand it the jump so the drawn pose
  stays; a body that turned local keeps it until it has decayed. It gives the move back
  at 0.95 a tick up to 25 cm and 0.85 from 1 m, by distance between; past 2 m
  it is cleared and the body is seen to jump.
- **Data and characters.** `body_sync_set_data` gives a local body up to 8
  opaque bytes (the stand's character activity), sent as the fifth rare group
  until acknowledged; `body_sync_data` reads them, on a copy as of its playback
  time; the handover records (grant, forced move, release) carry them as they
  carry flags. With `character_category` set, a LIGHT copy's shapes stop
  colliding with that category (this endpoint's own characters): the seam takes
  those bits off each shape once and puts back exactly the bits it took, by
  shape id, when the body is local again, unbound or the seam destroyed; the
  caller may change the rest of a copy's filters and add shapes meanwhile.
  Bind refuses a body of more than 4 shapes then. `body_sync_playback` reports the owner tick a copy shows,
  for measurement, and none while it has nothing to play.

The stand's host rule grants a guest's grab within the reach plus 1 m of slack
(`STAND_CLAIM_SLACK_M`): the host sees the claimant's character one way plus the
playback delay late, and 1 m covers 280 ms at walking speed. A game's own rule
should judge the reach at the claimant's tick instead (from the claim, or the
claimant's state at that tick), not on the host's late copy.

## The second layer

`body_sync_config_t.light_mass_max` (kilograms, 0..1e6; 0, the default, is off)
turns on local prediction of light bodies on a guest. With 0 the seam equals
0.6.0 apart from the version string, the new fields and one host-side change
below; it is not byte-for-byte the same code. Each guest may choose its own
knob; the host ignores it, and nothing of a soft copy is ever sent.

A copy of the host's body that is LIGHT, weighs at most the knob, has no
holder, is not held until settled and whose owner is not kinematic has two
holds on a guest (`src/sync_soft.h`):

- **Rest hold.** Its owner rests (asleep there, or within a copy-grid step of
  one pose for 15 ticks, a step being how far two copy records of one still
  pose may decode apart: a cell's diagonal, 3.4 mm, and twice the 9-bit
  rotation's error bound, 0.0195 rad; a pile leaning on a character that is driven every
  tick never sleeps; once resting, an owner that creeps rests on while each step takes it 8
  ticks or more, 27 mm/s, half the stillness so creep near the speed that lets it rest does
  not flap; a host body's own creep is not bounded, one slipping against a character reached
  48 mm/s, and a faster one is a move that takes its copy out of the hold, which the stand
  counts as a host wake) and nothing of this side's moves at it: it is driven
  kinematic on the owner's pose like any copy, exact at rest, outside every
  island, and keeps the character bits. A copy of a still but awake owner
  follows it only past that step, so it does not creep with the owner's
  rounded pose. A copy a body of this side's only leans on (a contact that
  acted, closing slower than a push) rests in the hold too: held on its
  owner's pose it pushes her only where that pose lies inside her, which No
  overlap below keeps soft; kept soft under her lean, it could follow its
  creeping owner only by a pull her lean pins, which stalls into a placement
  while the world rests. A body this side
  returned at rest rests in the hold where it shows it until the host's first
  state of it comes, and a state within a step of that pose does not place it. On the host, a light copy of a guest's body whose owner
  rests within that step stays as the host's world settled it, uncorrected.
- **Soft.** Every body of this side's moving faster than a lean (0.25 m/s at
  any point, spin times its extent counted: its own character and bodies, soft
  copies a hit set going, a copy in flight; not a copy in the rest hold, whose step
  of the copy grid is its owner's creep) marks the light copies within its
  travel in three steps plus 5 cm, and through contacts every light copy
  touching them. Copies held kinematic share no contacts, so the walk also
  passes from a kinematic copy to every copy within 2 cm (Box3D's speculative
  margin) of it, and so wakes a resting heap whole as the host's island wakes;
  only a walk this side's own body or a copy in flight started does so. A soft
  copy moves by the pull or by settling as it leaves the hold, and a heap it
  woke whole would settle the same way again, never at rest. Those, and every
  copy whose owner moves, are dynamic and
  simulated here, so its own character pushes them in the same step. A copy
  pulled back by the layer marks nothing. The game moves its bodies after
  `body_sync_apply`, so a push from standstill is seen at collect instead: a
  contact of a body of this side's (bound or not) that closed faster than
  0.05 m/s before the solve marks the copy, and through contacts the copies
  behind it (not its impulse: a lean pushed out of an overlap, or a weight
  borne, has a large one and no approach);
  pushed or pressed, a copy's pull waits. A resting copy is a wall for one step
  at most. A body of this side's dragged or shoved by a held copy's step marks
  nothing: one touching that copy and moving no faster than the copy's fastest
  point, its speed plus its spin times its own half-diagonal, however large the
  body. A copy that turns soft from
  kinematic starts from its owner's motion, never from its drive's sweep.

A soft copy is steered to the owner's playback pose:

- **Pause.** A hit (the step changed its velocity by 1 m/s or its spin by
  4 rad/s beyond the apply and gravity), a touch by a moving body of this
  side's or a push stops the pull for `sync_soft_pause`: the round trip to the
  host as the packet fates measure it (`body_sync_round_trip`), plus the
  playback delay and 2 ticks, 24 to 60 ticks: the host's answer lands before
  the pull starts up to a round trip of about 700 ms (60 = round trip +
  delay + 2). Past that the pull starts first: the copy is taken back toward
  the host's older pose, then follows the host once its answer plays. A chain of
  pauses runs at most 60 paused ticks and lasts across the gaps between
  touches: each tick with neither a pause nor a touch takes one off it, and a
  spent chain pauses nothing until it has drained to 0. A touch every other
  tick, which a chain reset on any free tick let pause 582 ticks of 600, is
  paused at most (W + 60) / 2 of any W ticks; the stand's `pause_window.max`
  (paused ticks of one prediction in any 120, between two take-backs) checks
  it at 90.
- **Snap.** Past 1 m from the owner's pose it is placed, the render offset
  taking the jump.
- **Dead zone.** It is pulled only past 1 cm or 2 degrees, and the pull stops
  under half of each.
- **Pull.** `v += 0.3 * (v_owner + clamp(error / 0.25 s, 2 m/s) - v)`, spin
  likewise with 4 rad/s. A pull still running after 30 ticks (pinned, packed)
  is a placement. Knocked 2 cm off its path on a frictionless floor, a copy is
  back within the dead zone in 12 ticks (`test_body_sync_real`, where the two
  worlds step alike: the pull's own lag). On a floor, friction takes up to
  μ g dt of velocity a tick from a copy at rest, so an error under
  0.25 s · μ g dt / 0.3 (10 cm at μ 0.6) never sets it sliding: such a pull
  stalls into the placement, which is how most corrections in the stand's
  piles end.
- **Speed.** Its speed never passes the seam's `max_speed` and `max_spin`.
- **Flight.** While the owner moves it at 4 m/s or more it is driven
  kinematic along the owner's path, soft again under 1 m/s; the change to
  driven is a placement, never a sweep that would bat the soft copies near it.
  It stays soft in flight while a body this side simulates (its character,
  what it carries) presses it or marks it within reach: the owner moves it
  that fast because that body's copy pushes it there, and placed on the
  owner's older path it would land inside that body and fling it. Marks from
  copies, the flying copy's own among them, do not count. Another seat's
  throw, held until it settles, is no free copy but flies by this rule too:
  driven along its owner's path at any speed with the character bits, soft
  only while this side's body presses it or marks it within reach, so it
  strikes this side's character instead of passing through her.
- **No overlap.** A driven copy is never placed or swept onto a pose that
  lies inside a body this side simulates deeper than Box3D's linear slop
  (shape against shape, where the two collide), the depth Box3D's contact
  manifold reports: that step it stays soft, the contact parts them, and it
  is driven again once its pose is clear. A copy resting against that body,
  the two pictures of their contact a few millimetres apart, is no overlap
  and stays in the rest hold.

Going back to the rest hold inside the dead zone sets the copy on the pose
exactly; further off it is placed and the offset takes it. The layer never
puts a body to sleep (that sleeps its whole island, this side's own bodies
with it). Grab and release of a soft copy keep its pose.

**The host-side change.** A dynamic hold (the host's copy of a guest's
character or carried body) of an owner standing still leaves an error under
1 mm and 0.2 degrees alone instead of correcting it every tick: any velocity
set wakes the island, so a pile against a standing heroine never slept. With
the layer the guest's heroine is pressed by her soft copies by a hair, which
is what kept the host's pile awake.

Layer copies keep the character bits; `body_sync_is_layer_copy` names them,
so a game that sets its own filters does not take the bits off them. On the
host a guest's throw, held dynamic until it settles, keeps them too: the
host's characters meet its flight as the thrower's copy of them does. The stand
counts a light copy its character walks into without contact as a missed hit.

A throw (released at the flight speed, 4 m/s, or faster) leaves the hand inside its thrower's
reach, and on another side her copy and its copy play from different states: every side keeps
the two apart with a filter joint until the throw has travelled her half-width and its own
half-diagonal (Box3D's bounds, speculative distance included) at its release speed, a copy
reading its owner's speed; about 0.61 m and 10 ticks at 4 m/s for a 0.3 m capsule and a 0.3 m box. A drop
is met as any body.

The layer scenes bound `rest_lag_ticks.max` at 98 ticks, derived for a 250 ms
round trip (one-way 8 ticks); each further ~33 ms of round trip adds a tick.

Cost at -O2 (`load_four`, 300 bodies, three guests): the layer adds 0.39 ms a
guest-tick, of which marking is 0.25 ms.

What it claims, measured on the stand: hits answer within 0-2 ticks (`response_ticks`
at 100 to 300 ms; the 2 is a copy resting in the hold met from standstill), worlds rest and agree within 1 cm and 2 degrees with
the heroine standing in a pile (`layer_stand`), no jitter at rest. The cost is
a moving pile that differs from the host's until it rests (`pause_divergence`,
divergence).

## Authority follows interaction

`body_sync_config_t.authority` picks who owns a free body; every endpoint uses
the same mode and `light_mass_max` (the caller's handshake). HOST (0, the
default) is 0.7.0's rule. INTERACTION (`src/sync_policy.h`, the one place its
rules live) gives a light body (class LIGHT, at most `light_mass_max`, not
kinematic at its owner, carried by nobody) to the seat whose action set it
moving, and through it whatever that body sets moving, until they rest; the
others show it through playback. The thrower then has nothing to correct: the
host shows the thrower's result. Corrections remain where two players touch the
same bodies, and there the second layer is the loser's correction. The stub
ignores the mode.

- **Agents.** A seat's character, a body it carries or threw, and a body it
  owns by interaction. An agent takes a light body it touches while it moves
  faster than a lean (0.25 m/s at the contact, spin times its extent counted),
  or, a character or what it carries, pushes it from standstill (the second
  layer's push rule). Between bodies in a pile a push would count their weight
  on each other, so an owned body takes only by its motion, and only a body
  that rests at its owner: one its owner moves was set going by someone else,
  and taking it on would hand a contested heap between two guests through the
  host. A character standing in a pile owns nothing.
- **The take.** On a guest it is optimistic: at collect after the step of the
  contact the body becomes local (a claim with the hold until rest), simulated
  on from what this side shows (a take never moves the body; the newest state
  received may be older than the copy's pose), and goes out from the next tick.
  The host answers the claim itself, never through `body_sync_poll_claim`:
  granted when the body is the host's, free and not set going by the host's
  own agents, the seat has take budget left (16 a tick, up to 8 ticks of it
  kept for a hitch of the record lane, counted on the host's own ticks, never a
  record's stamp), and the host's own world shows one of
  the claimant's agents within reach of the body, by the same rule as a contact
  claim's (below): the agent along the path the host's own taken states of it
  show, never by the velocity they declare, its last state standing while it
  rests and its seat is heard, within the claim's lag (below). The reach adds what the body travels and what the agent travels at its
  fastest step taken within the grace over a round trip, a playback delay and a tick: the body
  the guest touched was its copy of the host's, a one-way trip and a delay old,
  with the claim's way back on top, and the second layer may have pushed that
  copy ahead with the agent (`src/sync_policy.h` derives the slop). A take whose agent the host refused, or a forged claim of
  a body no agent is near, is refused. While its take waits, a body's state declares the number the
  grant will give, so the host takes its states from the grant on, not a round
  trip later. A refusal moves no number, so another guest's take granted next
  gets the number its states declared, and refusals, which spend no take
  budget, move nothing at all. The host keeps one bit per seat and body
  instead: its next move of that body to the refused seat adds two, and the
  guest, told of the refusal, declares two ahead for its next take. A state of
  the refused take declared the claimant's number then plus one; that number
  never passes the host's, N at the later move, so the state carries at most
  N + 1 and never matches the move's N + 2. Only the claimant hears the grant; the other guests read the new
  owner from the state the host relays.
- **A flight.** A thrown or dropped body that first touches a body its thrower
  may take stays the thrower's, owned by interaction; one that touches a body
  it may not take (heavy, a character, owned elsewhere) goes back to the host
  as under HOST.
- **Conflicts.** The host first: a body the host's agents touch is the
  host's until it rests (a hold until rest there), and no guest may take it;
  the host's agent touching a guest's body owned by interaction takes it back
  at once, the host going on from its own simulation of it. Then the earliest
  confirmed toucher: a take of a body another guest owns is refused, and no
  body moves between guests. The loser's body turns back into a copy that the
  second layer corrects toward the winner's at once, as any copy that strays:
  no handover pause, which would let the loser's picture run on a round trip
  more; past 1 m it is placed and its render offset takes the jump, up to 5 m
  rather than the offset's usual 2 m (the random three-participant sweep's
  farthest was 3.1 m), closing 0.15 m a tick beyond 2 m: the loser's picture
  glides to the winner's instead of jumping (`body_sync_lost_take`; the stand counts it in
  `lost_takes.count`, the hand-back in `lost_takes.max`, bound by the glide's 5 m reach, and
  leaves the glide out of `aligned_gap`). A guest takes no
  body whose last known hold is the host's own agents' (the rare groups kept
  while a claim waits make a refused claim's hold known). Where bodies two guests own this way
  touch, the host takes both back, as its own agents' touch does: each guest
  would lean on a copy of the other's that is a round trip through the host
  late, and the heap would creep and never rest (`authority_two_guests_far`).
  The priority is one constant in the policy (`SYNC_POLICY_PRIORITY`).
- **The host's hold.** A guest's body owned by interaction is held on the host
  as any dynamic copy (owner's velocity and the closed error), and keeps
  `character_category`'s bits there, so the host's character meets it; that
  touch is how the host's agents come first. The claim reaches the host one
  way, its first state one playback delay before the host plays the contact,
  so the body holds its first snapshot until the thrower's ball, on the same
  timeline, reaches it.
- **The return.** A guest releases a body once it is quiet (asleep, or within
  2 mm and 0.004 rad of one pose for 15 ticks: the second layer's stillness),
  every body it owns by interaction that touches it is quiet, and the host has
  acknowledged its rest pose and every rare group: the host then continues from
  the very pose the guest has, or, when its own copy lies within the second
  layer's dead zone of it (1 cm, 0.035 rad), from its copy's pose with the
  guest's velocities: placed a fraction of a millimetre off its neighbours
  there, the body would drop or be pushed out and wake the heap. A quiet body
  goes out as a rest pose though awake. The host ends its own hold the same way, without the
  acknowledgement, and only once its agents have been off the body for 15 ticks plus the farthest
  guest's measured round trip (`sync_policy_host_hold_ticks`): an agent leaving at the slowest
  speed that marks (0.25 m/s) is inside the marking walk's reach (three steps of travel plus 5 cm)
  for 15 ticks, and a guest's take sent on the end's first state lands a round trip later, so a
  walk that brushes a body twice keeps one hold instead of ending it and refusing the take.
  A body this side gave back while its own character or a body she carries pressed it is not
  taken again by this side until that press ends: its copy under the press would be taken by the
  push, quiet and go back again, over and over.
- **Bounds.** A body owned by interaction goes back after 600 ticks (10 s)
  without an agent touching it however it moves (a body rolling for ever), and
  at once when it leaves the box; one this side carries neither rests nor ages
  out until it is let go. A guest owns at most 64 such bodies, its
  takes still waiting for the grant counted (over a 300 ms round trip a heap's
  contacts would otherwise send hundreds, every one past 64 refused): the
  pile scenes' 60 and the ball, and 64 moving sim records (25 bytes each) are
  already 1.6 times a 1000-byte budget, so each waits about two packets
  (README "Staleness bound"). It sends at most 16 takes and 16 releases a
  tick: 128 and 320 bytes on the reliable lane, the host's answers 432 bytes
  (27 a record); a heap's contacts spread over the next ticks.
- **A guest that leaves** hands its bodies back as any seat does (from its
  newest state, velocities included); they are the host's until they rest.
- **Untrusted bytes.** A claim with a hold past the hold until rest is FORMAT;
  a take of a heavy, carried, kinematic or foreign body, past the 64, or of a
  body the caller gave the guest under another hold, is refused, never asserted;
  a duplicate take of a body the guest owns this way is answered and moves
  nothing. A caller's claim with the hold until rest is
  INVALID.

- **Contact claims.** The owner of a body judges its contacts and the host
  checks the claim with `body_sync_check_contact(seat, agent, number, target)`;
  the claim names `body_sync_contact_number` (the number the body's states
  declare, so a waiting take names its grant's, a take right after a release
  the host has not answered yet included; a claim that is not a take names
  none until it is granted, since the host takes none of its states before).
  The game sends its claim after that tick's seam records of the seat, on the
  same ordered lane: the contact that ends a flight (always under HOST, against
  a heavy body under INTERACTION) sends its release first, so the host has the
  body back before it reads the claim, and a gate on "owns it now" alone would
  refuse every honest one. The host keeps, per seat and body, that seat's newest
  ended ownership: its number, how it ended, the host's tick then and the
  seat's last state the host took. The verdicts:
  - OK while the seat owns the body under the number.
  - OK within the grace after the seat's own release (a let-go, a flight's
    return) or the host's arbitration of a contest (its take-back or claim, a
    grant or take to another seat), STALE after. The guest simulates on as owner
    until it hears of the end, so its honest claims of that time land within a
    round trip of it. The grace is the evidence's span: a round trip, a
    playback delay and a tick, capped at `SYNC_POLICY_EVIDENCE_AGE_MAX_S` (1 s;
    the stub, which plays no delay: its round trip plus four mean deviations of
    it, for a state and its claim crossing on packets of their own, and a
    tick). Past it, the claim would credit a landing of whoever
    flies the body since.
  - REVOKED for good after a recall: `body_sync_assign` to any seat (its own
    included), the seat leaving. The recall drops the seat's late claims.
  - STALE for anything else: an old generation, a number the seat never held
    (another seat's, or the one a refused take's states declared, which the
    host's next move to that seat skips), a claim older than the seat's newest
    ended ownership, any ownership from before a seat joined again after a
    leave, a guest calling.
  The host's world comes with it, measured only on what the host itself took
  of that ownership, never on the velocity its owner declares (a guest that
  sent a faster velocity than its body showed would widen both limits). The
  host keeps the owner's last 12 taken states of each body (position, the
  sender's tick, its own), past the ownership's end until another ownership's
  states replace them. A gap in them is no evidence, never a wider bound: it
  may cost an effect, never forge one. All of it stands only while the seat is
  heard within the claim's grace (a seat gone silent leaves no evidence). The
  claim names the contact's tick on the claimant's clock, the one its states
  are stamped with (a take: the take record's stamp). That tick is the
  claimant's word, so the host dates the evidence by its own ticks: a state
  counts only while the host took it at most `SYNC_POLICY_CLAIM_LAG_GRACES` (2)
  graces before it judges the claim (at most 1 s), one for the claim's own trip
  and one for the record lane resending it. Over the claims sweep the latest
  honest claim came 1.33 graces after its state (two resends at 25 % loss), a
  take 8 ticks. A state older than that is no evidence, however the claim names
  its tick: an agent that rests sends nothing, and its last states would
  otherwise stand for any contact named on them for as long as it owns. The contact lies on from
  a taken state stamped at most `SYNC_POLICY_CHORD_TICKS_MAX` (4) ticks before
  it, for the ticks between: along the step that led to that state when that
  step spans as few ticks (only the contact turns the body), or on the chord to
  the next state when the contact comes before it and they are as close. A
  contact further on from every state has no evidence; the time it may grow
  over is those ticks, never a delay. A state's span is the sender's, kept
  within the playback delay of the host's own ticks between the two: stamps far
  apart buy no time, stamps squeezed together neither speed a step up nor turn a
  gap into a chord. The gap is the agent's box from the target's at the nearest
  of these, the target where the claimant's own states, or the host at its take
  of it, put it at the contact (its states only under an ownership of it whose
  claims the seat may still make: never once the host took it back and the
  grace ran out, or recalled it). A target the host shows as it is now is put
  back along its own velocity by the time since the contact (at most the grace),
  so a body that flew into the agent's old place is judged where its path had
  it. The reach is the take slop, the target's bend off that path (sized as the
  agent's, never more than its travel at its speed now over that time, so a
  resting target gets none; a target at a claimant's own state gets that
  travel) and a bend of the agent's path of `SYNC_POLICY_CONTACT_BEND_MS2`
  (20 m/s^2): 4.4 cm over 4 ticks. The
  approach is the fastest step of those very states (the step into each, and
  the chord's) plus the target's speed, never a step elsewhere in the ring: a
  fast throw that rolls on to a slow touch claims the touch's speed. A step
  faster than the box's speed is a teleport, never capped: the states before it
  are dropped, and it starts the evidence over as a point with no step, so the
  agent is where the host shows it, with no speed and no push until its next
  step.
  With no state that close before the contact the gap is INFINITY: a throw whose every flight state
  the network lost, or that ended before the host heard of it, is a missed
  effect, never a guessed one. The seam's records carry no pose, so a release
  cannot stand in. A guest that acknowledges late raises its measured round trip:
  the cap keeps it from widening a take's or a contact's reach and the grace
  past 1 s. A seat that left gets nothing of a grace. The seam sets no
  threshold and judges no effect: the game dedupes per target effect across
  seats, not per claim key (seat, agent, number, target). After an arbitrated
  end both seats may get OK for the same contact for up to the grace (1 s at
  most), and after any end the host's own simulation may meet the same target
  again. A guest that released a body it only claimed or took cannot know the
  host's number (a withdrawal or refusal moves none, a crossed grant two) and
  names none for a re-take until it hears a newer one.

What the gate promises: a contact claim (or a take) is consistent with what the
host itself saw of the claimant's agent at the named contact, its position, step
and time. It does not stop an owner that really moves its own bodies into a
target: under INTERACTION the owner simulates them, and the host shows that
motion. A gap in the evidence is a missed effect, never a false one. A finding
that breaks this is a bug; anything outside it is out of scope.

`body_sync_traffic` counts the takes, refusals, take-backs and returns
(`takes`, `take_refusals`, `reclaims`, `returns`); the stand reports them per
guest.

The stand plays the game's side of contact claims. The owner of a flying body
(a throw's settling hold, a take's hold until rest) judges each contact with
the host's own rule (`stand_rules_impact`). The host applies its own bodies'
effects at once. A guest sends an impact record (target, agent, the agent's
`body_sync_contact_number`, the contact's approach speed, the contact's tick)
on its record lane after that tick's seam records; the parser refuses a tick
after the record's own stamp or more than `STAND_CONTACT_AGE_MAX_TICKS` (120)
before it. A body that names no number (a claim not yet
granted) sends nothing: a missed effect, never a false one. The guest sends
each key (agent, number, target) once, so a body bouncing on a target claims it
once and a new throw, under a new number, claims a target that survived again.
The host refuses a record its parser refuses (counted, dropped, the seat
stays), a key it judged before, a verdict other than OK, a gap past the
seam's reach, and a claimed speed past the approach it saw times
`STAND_IMPACT_SPEED_SCALE` (1.25) plus `STAND_IMPACT_SPEED_SLACK` (0.5 m/s).
These limits are the stand's policy, not the seam's contract. The target's
state dedupes across seats: a broken body is gone, a felled one carries the
knock flag. The host holds each seat's claims of a tick in that seat's own
queue (64), so one seat's flood costs only its own claims, and it keeps each
seat's judged keys apart. A guest sends its stand records only once its seam
records of the tick are all out, so a claim never overtakes its release. Every
claim the host parses lands in one of `claims_overflow`, `claims_repeat`,
`claims_rejected`, `claims_idle` (accepted, nothing left to do: no effect, a
push of a body the host does not simulate) and `claim_effects`, which add up to
`claims`; `claims_format` counts what the parser refused, and `claims_ended`
the claims the gate passed after the agent's ownership had ended (a flight a
heavy door ends sends its release first: `throw_glass_door`). The sweep
(`test_body_sync_claims --sweep`) throws at the glass from rest and after a
walk, in both modes: at 100 ms, 20 ms of jitter and 2 % loss, and at 150 ms,
60 ms and 10 %, every honest claim applies; at 200 ms, 80 ms and 25 % over 24
seeds, 80 of 89 apply (a contact more than 4 ticks after the last state the
host took, or a claimed speed above the steps it holds at the contact).

## Running the stand

The feature consumer builds the stand on each implementation, and its tests;
run the owner command, then a scenario with the command line it built:

```powershell
node ai_studio/feature_testing/run.mjs
ai_studio/feature_testing/consumer/build/feature-verify/_body_sync/body_sync_stand.exe features/body-sync/scenarios/two_walk.scn --out two_walk.json
ai_studio/feature_testing/consumer/build/feature-verify/_body_sync/body_sync_stand_stub.exe features/body-sync/scenarios/two_walk.scn --out two_walk_stub.json
```

`body_sync_stand` runs the real seam, `body_sync_stand_stub` the stub; the
metrics name which (`implementation`: "real 0.6.0" or "stub 0.6.0"), so one
scenario and seed compare the two. A game's own stand target links
`body_sync_stand` (the real seam) or `body_sync_stand_stub`.

`body_sync_stand <scenario> [--seed N] [--out metrics.json] [--no-series] [--quiet]`
prints a line per guest and per check, writes the metrics (default
`<scenario>.json` in the current folder) and exits 0 when every check passed,
1 when one failed, 2 when the input was refused, the run could not start or
the metrics could not be written. `--list-scenes` names the scenes. `--seed`
overrides the scenario's seed for sweeps.

A run has three phases. Before tick 0 every guest connects and is seated in
seat g; that is not measured. Then the scenario's items run, within 600 s of
virtual time: a schedule that cannot fit is refused. The run ends at the hard
`end`, or when every endpoint's world has been at rest for 0.5 s after the
last item ended, or when `settle` has passed without rest.

## Scenario format

One statement per line, `#` starts a comment, ASCII outside comments. The
full grammar and every range are in `body_sync_scenario.h`. In short:

```text
scenario <name>                      # required
scene <name>                         # required: a catalog scene or a caller's
seed <n>                             # default 1
guests <0..3>                        # default 1; player p belongs to endpoint p, p0 to the host
settle <time> | end <time>           # default settle 5s
rate <host|gN> <x>hz                 # that endpoint steps x times a second of virtual time; its seam keeps 60

[at <time>] <p0..p3> walk <x> <z> [speed <v>] | walk <body> [as <label>]
[at <time>] <player> grab <body> | throw <x> <y> <z> [speed <v>] | throw <body> | drop | punch [<body>] | wait <time>
[at <time>] p0 recall <body> [to <x> <y> <z>] | remove <body>   # the host's rule over a body whoever owns it
[at <time>] net <g1..g3> [to_host|to_guest] rtt|delay|jitter|reorder_delay <time> loss|dup|reorder <share> rate <B/s>|off
at <time> cut <guest> for <time>     # nothing passes; 10 s or more disconnects
at <time> stall <host|gN> for <time> # a long frame, then the missed ticks, 8 a frame
at <time> deaf <host|gN> for <time>  # steps and sends, reads nothing

expect agree_at_rest <length> [<angle>] | no_stuck | effects_once | left <guest>
expect result <label> <done|granted|refused|hit|missed|rejected|failed>
expect one_of <outcome> <label> <label> ...
expect moved <body> <=|>= <length>  # the body's travel on the host, first tick to last
expect effect <body>                # a knockdown or break took the body on the host, shown once on every guest
expect pace <label>                 # a walk to a point keeps up with a local reference run
expect <metric> <=|>= <value>
```

Units are required: `250ms`, `1.5s`, `90t` (ticks), `5cm`, `0.05m`, `3deg`,
`2%`, `64KB/s`; `speed` is a bare number in m/s. A positive time that rounds
to zero ticks is refused. An action with `at` starts then and cuts off what the
player was doing; one without it starts when the player's previous action
ends. `recall` and `remove` are the host's rule, p0's alone: a recall is
`body_sync_assign` of the body to the host, its carrier letting go, then a
placement at rest at the point or at its spawn pose; a removal unbinds and
destroys the body on the host and tells every guest. Both are done at once. A
grab is optimistic: the action ends at once, and its label's outcome
becomes `granted` or `refused` when the host's answer arrives. A grab out of
reach fails; scenarios walk first. An angle written in `agree_at_rest` is
always a bound, `0deg` included. With `guests 0` the checks that compare a
guest are refused.

`expect pace <label>` holds a walk to a point to what physics alone does with
it: once the run ends, the stand runs the same scenario, seed and modes again on
the host's world alone, every player's character local there and no network,
and compares the walk's progress (its start distance less the nearest it came)
tick by tick from its start. At every tick the walk must be no more than 0.2 m
(a walk's arrival distance) behind the reference's progress L ticks earlier,
where L is what the network may hold back of what the walker meets: two of the
run's slowest one-way paths (another guest's body crosses to the host and on to
her), her playback delay at most (15 ticks), and the longest cut, stall or deaf
spell. What the others do reaches her world up to L ticks late, and a body met
later is met elsewhere on her path: a guest's throw the host plays late can
leave debris in her way that the same throw on time had cleared, or line chairs
up against a wall that it would have piled over. So the reference is run with
every other player acting 0, 1, ... L ticks late, and the walk passes when it
keeps pace with one of them (`wall_plank`: a ball that clears a crate from her
lane on time, cut off long enough that she pins the crate to the wall first;
held to the on-time reference alone it fails). A runner pushing chairs into a
wall does so in the reference too; a copy that stops her where every
reference's body gave way fails (planted in `test_body_sync_checks`: her copies
of the pile's near face held kinematic). An
absolute `expect result <walk> done` stays where it is derived, a free walk. The whole of `scenarios/edge_throw_cut.scn`:

```text
# A guest throws, and its link is cut while the body flies.
scenario edge_throw_cut
scene floor_wall
seed 11
guests 1
settle 6s

net g1 rtt 100ms jitter 20ms loss 2%

p1 walk crate
p1 grab crate as grab
at 3s p1 throw wall speed 9 as throw
at 3050ms cut g1 for 3s

expect result grab granted
expect result throw done
expect agree_at_rest 5cm 5deg
expect no_stuck
expect effects_once
expect host_ticks.max <= 120t
```

## The viewer

`example/` holds the stand in an engine window, on debug shapes: the
"guest's view". It is never built by default and never runs in CTest; it is
skipped, with a status line, where the engine's window and renderer are
absent. In the Studio's feature consumer the program is the target
`body_sync_stand_view`, outside the default build; a game adds its own target
(INSTALL.md) and builds it with its own `node tools/game.mjs build`.

`body_sync_stand_view <scenario> [--view host|g1|g2|g3] [--ghost] [--seed N] [--frames N] [--size WxH] [--camera ex,ey,ez,tx,ty,tz[,fov]] [--record file.mp4] [--shot file.png] [--shot-tick N] [--hash-every N]`
draws one endpoint's world (default g1) from `body_sync_stand_body` alone,
with a camera that frames the scene's bodies from outside and above, or the
one `--camera` places (eye, target, vertical field of view in degrees, 50 by
default). `--size` sets the window, 800x450 by default, from 320x180 to
1920x1080; it is in points, so the framebuffer and a recording are that size
times the display's pixel ratio (1280x720 on a display at ratio 1). `--ghost` draws the host's
bodies as red outlines over a guest's view, so the copy's lag and any
divergence show. Keys: Space pauses, V cycles the view, R restarts, Esc quits;
during a recording only Esc works, so a take is one view from tick 1 to its
end. The window title names the scenario and the view; the console prints the
tick each second. A bad argument is named with what it accepts.

Pace: the stand keeps its own clock. Viewing runs the ticks the wall clock
has reached, 60 a second, several in one frame when a frame came late (a stall
over a second is dropped), and prints the measured rate after the first tick's
frame at the end; recording runs one tick per frame, as fast as the encoder
takes frames. A tick that `--hash-every` or `--shot` asks for is drawn on its
own frame either way, so its pixels, and its hash, are the same in both.

End: a run that reaches the scenario's end prints the report as the command
line does and exits as it does (0 passed, 1 a check failed, 2 refused). A run
cut short (`--frames` before the end, Esc, the window closed) prints
`cut at N ticks`, evaluates no check and exits 4; a `--shot` it never reached
says `never taken: cut at N ticks`. 3 (it wins over 4) means a
recording or a screenshot could not be written, or a full run ended before
`--shot-tick`.

Colours: dark grey static, with a light outline, and a floor shows a line every
metre over its top face, a heavier one every five; green a body this endpoint
simulates; blue a copy; yellow carried; magenta flying (thrown, its hold not
yet settled); orange knocked down; a darker shade of any of them asleep.
Characters have a white outline, items a dark one.

`--record` writes an MP4 through the engine's recorder (`nt_recorder`,
Windows Media Foundation) on the stand's own clock: one frame per tick,
whatever the machine's speed, with a silent audio track. Frames go straight to
the encoder, so no frame files are ever written. A take whose recorder started
but wrote nothing is removed; a file the recorder never opened is left alone.
The take of a cut run is kept as `<name>.partial.mp4` (an older one of that
name is replaced), so a full-length name never holds a short run. Keep
recordings outside the repository (a scratch folder). `--shot` saves one PNG
at `--shot-tick`; `--hash-every N` prints a hash of every N-th frame's pixels,
which is how two takes are compared: the pixels repeat exactly, the MP4 bytes
do not (the encoder's output differs between takes).

A game draws the same views with its models from its own target: it links
`body_sync_view` and calls `body_sync_view_main` (`example/stand_view.h`) with
its scenes and a `body_sync_view_drawer_t` of its own. The viewer owns the
window, camera, stepping and present; the drawer owns its passes, lights and
shaders between `begin_frame` and `end_frame`, and the first pass it begins
clears. `configure_gfx` edits the renderer's description before it starts;
`ready` holds tick 0, the recording and `--shot-tick` until the game's packs
are loaded (30 s at most, then the run is refused); `camera` may move the
viewer's proposal each frame, for example behind the viewed endpoint's own
character, which it is handed with the simulated seconds since the previous
frame (0 when no tick ran, so easing stops while paused); `shutdown` runs whenever `init` was called. The frame carries the view
and projection matrices and the camera's eye, target, vertical field of view,
near and far. A character's view carries its facing in its rotation and its
`activity` (idle, walk, carry, throw, punch) on every endpoint.

## Metrics

Per guest: divergence from the host's world per tick (max, p95, mean, the
largest rotation), jerk of the copies it draws per frame, bytes per second by
kind in both directions (the seam's `header`, `copy`, `sim`, `held`,
`authority` and the stand's `claim`, `verdict`, `spawn`, `remove`), the
link's own lagging rate estimate (`link_estimate_bytes_per_s`, not a count),
the session's RTT and loss, and the seam's refused, stale and rejected counts.
Per action: its outcome and its delay in ticks until the host's world shows
it, the host's answer reaches the actor, and the last other endpoint shows it;
a started action never seen reads -1 and fails any delay bound. Per check:
passed, value, limit and the first offender.

**Divergence includes the copy's latency.** It compares what a guest draws
with the host's physics at the same tick, so a moving body diverges by its
speed times the path's delay however good the sync is; read it as what the
player sees, not as an error of the sync. The definitions are in
`body_sync_metrics.h`.

**Same-tick divergence grows with the playback delay, by design.** A copy now
plays 4 to 15 ticks behind its newest snapshot, so a body moving at v diverges
by v times that delay more than under the old receiver: on `two_walk` (walking
at up to 3.6 m/s, 100 ms) the p95 went from 0.24 m on the stub to 0.58 m on the
real seam, 0.34 m, which is 3.6 m/s over 5 or 6 ticks. It is not a regression:
the playback measures take the delay out. Per guest, for each copy its owner
sends to directly, at the owner tick P the copy shows (`body_sync_playback`):
`lag_divergence` (max, p95: the copy against the owner's own position at P),
`path_error` (max, p95: the distance to the owner's path over [P - 30, P + 30],
a cut corner free of any time shift), `step.count` (ticks where the drawn move
exceeds 1.25 times the owner's move over the same playback interval plus
1 cm, counted in body-ticks; those while the copy's render offset gives a
move back are `offset_step.count`, a slide by design), `jump.max` (the drawn
move less the owner's, largest), `snap.count` (drawn moves past 2 m, the
offset's reach, not counted as jumps) and `behind_ticks` (max, p95: how many
owner ticks a copy plays behind its owner's newest; a frozen or late clock shows
here). A check of a playback metric on a guest with no copy measured fails: no
sample is no evidence. Each scenario says which of its bounds are derived and
from what, and which were measured. Most of `step.count` on the real seam is a
dynamic hold on the host closing its error at its correction speed (after a cut
in `cut_short`, at a walk's start in `two_walk`). On the stub
P is its newest received tick, so those read near zero there: its stutter is
in time and shows in `jerk`. A guest's body relayed to another guest plays on
a clock the stand cannot map back and is left out of them.

**The lag-aligned gap** (`aligned_gap.max`, `.p95`) compares two pictures of one
moment while a body moves: a copy, and a predicted copy once its pull runs (the
pull steers it to the playback pose), against what its owner drew at the owner
tick the copy shows. Its bound is derived. At the apply the copy's physics lies
within 2 m of its owner's physics at that tick: a guest's soft copy is placed
past 1 m, a driven copy reaches its pose in the step or is placed, and the host's
dynamic copy of a guest's body is placed only past the render offset's 2 m. The
step then moves a dynamic copy with its owner's velocity plus a correction that
closes, never passes, the error, so it ends at most a tick of its owner's travel
further: the owner's speed on the wire is within max_speed, 100 m/s on the
stand, 1.67 m a tick. The copy's render offset adds at most 2 m, and the owner's
drawn picture carries the owner's own offset, at most 2 m more (a placement on
the owner, as the host's take-over of a guest's grab, glides there as well):
6 m and a tick of travel, 7.7 m. A host-mode grab in the 100-throw sweep reached
4.79 m, a copy's 1.26 m glide against the host's own. Against the host at the
guest's moment a copy would be off by its speed times the round trip and two
delays however true it plays. A predicted copy in its pause
(`body_sync_is_pull_paused`) is the guest's own answer to its touch, drawn ahead
of the tick it plays; read there, its lead would count as an error the player
never sees, so it leaves the gap for two reported-only figures (max, p95; no
check). `prediction_gap` takes a pause that began with the host's body at rest at
the tick the copy showed then, against the host's picture when the host shows
that guest's moment (its playback of the guest's character reaches the guest's
tick). Both sides simulate one contact from one rest pose; past it they differ by
chaos, not lateness, and no bound derives that. `flight_pause_gap` takes a pause
that began while its owner moved the body, which has no shared moment, against
the owner at the playback tick, its lead included. Another guest's body (a
relayed clock, and each side meets that guest's motion at its own stage: 3.06 m
on a random three-participant pile at 300 ms) is in none of the three.
A copy with nothing to play (one just turned back from
local, before its owner's first state since) is not measured. It is the policies' common measure: same-tick divergence
(and `pause_divergence`) counts the shower's latency, which under authority
following interaction sits on the host. `local_correction.max` is what the seam
moved a body a guest simulates beyond its own step (its move at the apply plus
its render offset's change): the thrower's own corrections, 0 by construction
under interaction outside a contest; `local_turn.max` is the same of its
rotation, in degrees. A guest that threw and simulated no body has no sample
of either, and a check of them fails: a thrower simulates what it throws. `own_ticks.max` is the other half of a throw's zero
delay: the ticks from a throw's start until its thrower draws the body 5 cm on.
An offset left from the body's time as a
copy only decays by the seam's law, that copy's correction ending, and is not
counted; once the seam sets the offset while the guest simulates the body,
every change counts. Planted in `test_body_sync_checks`: a move of a local body
outside its step and a render offset set on one fail `local_correction`, a turn
of that offset fails `local_turn`, and a copy knocked after rest fails
`rest_breaks`.
A guest's claim of a copy takes over within the claim, from the pose the
claimant shows (as a take does): a placement at the newest state would leave its
jump in the offset, and the game's carry would take that offset off the hand
(0.09 to 0.23 m on the game's `openspace_wall` and `openspace_carry`, 0.05 m on
`throw_catch`). The measure replays the seam's offset decay on the previous
tick's offset, so an offset dropped or grown counts, and it counts from the
tick a body turns local, against its pose once the tick's actions are done.
`rest_breaks.count` counts the times a guest's whole world, at rest for 30
ticks after the scenario's last action, stops resting. A rest that ends only
because host bodies moved (a pile creeping against a character slips) is the
host's physics: it counts in `host_wakes.count`, and `creep_max` reports the
fastest an awake host body moved while the world rested. A driven (kinematic)
body rests by its mean speed over 15 ticks, a tick's move past 6.8 mm counting
whole: a copy of a still or creeping owner steps on the copy grid, 3.4 mm in a
tick (`src/stand_logic.h`). Planted in `test_body_sync_checks`: copies
shown a tick early and a guest drawing its own bodies 10 cm off what it sends
each fail a bound the clean run holds.

A guest whose session ended fails `agree_at_rest`, `no_stuck` and
`effects_once` with "gN left at tick T" unless the scenario says `expect left
gN`; a check never passes on a guest it did not compare.

Network situations covered by the shipped scenarios: a same-tick grab dispute
(`grab_dispute`), a throw with the thrower's path cut in flight
(`edge_throw_cut`), a 200 ms frame on a guest and on the host (`edge_stall`),
four throws into one heap from three guests at 50, 100 and 200 ms
(`load_four`), a guest that stops reading for 3 s (`edge_deaf`), a throw at a
wall played from snapshots (`bounce_wall`), a guest's character pushing a brick
and pressing a cabinet (`char_push`, real seam; the stub's kinematic copy
shoving the cabinet is its must-fail contrast), cuts of 0.5 s and 3 s
(`cut_short`, `cut_long`), a 1.5 s delay dropping to 50 ms (`latency_drop`), 200 ms hitches on a guest and on the host
(`hitch_guest`, `hitch_host`), a guest stepping at 59.5 Hz and at 50 Hz
(`rate_mismatch`, `rate_rough`), two guests at 30 and 250 ms (`two_ping`), a
recall of a guest's body in flight and from her hands with her late states and
claim dropped (`recall_flight`, `recall_carry`), a removal of a guest's body in
flight (`remove_owned`), and hostile bytes on every parser (seeded fuzz in the
tests). The stand never reuses a scene body's slot, so an old packet meeting a
reused slot is covered at the seam only (`test_body_sync_stub`, both
implementations: a guest's state and release of the old generation after the
host binds the slot again under her seat and number are stale). Not yet
covered: a grab of a body flying under another guest's authority, a sleeping
body moved by the host, more moving bodies than a budget allows, a record over the unfragmented limit, a reader that stalls for 10 s,
reconnects, joins and the host leaving (the seam has no baseline yet), the
16-bit packet number wrapping, a guest pushing a stack
or standing on a host's body, a hit on a target that already moved on the
host, and a host whose window is minimised.

## Validation

`test_body_sync_codec` (core) round-trips every record kind and rare mask
within half a step, keeps every value readable through the writer's limit,
never splits a record, refuses truncations, padding, out-of-range codes,
seats and speeds, an empty rare mask, a relayed record's seat out of range, a
data group past 8 bytes and too many records, and fuzzes the reader.
`test_body_sync_playback` (core) runs the playback receiver alone: playback
time monotone, never past the newest sample and within its pace over loss,
duplicates, reordering and a 30-tick gap; a duplicate is no jitter; the delay
before the clock knows and after a rejoin; freeze and a single jump; Hermite
against linear, the shorter arc, the exact rest pose; rare groups on time, late
ones and a thinned buffer; the drive limits; the render offset; the rate of a
59.5 Hz endpoint and the line a relay ages by. `test_body_sync_sender` (core) runs the sender against a lossy pure-C
peer: no moving body starves when the budget overflows; a slow body goes out
by the age limit; a rest pose arrives at 10 % loss and then goes quiet; a
packet never exceeds the capacity up to the link's limit; ten seconds of
overflow; a record larger than the room is left out whole; the 16-bit number
wraps; an unsent packet, a lost packet and an older version acknowledge
nothing; a rest on the acknowledged pose is not resent; the receiver.
`test_body_sync_real` (core) runs three real seams with Box3D: rest poses
agree and go quiet, a guest's body comes back moving, an old grant's pose is
rejected, flags, type, holder and locks move with authority, both receive
calls are fuzzed through apply, a step and collect with Box3D's asserts
counted, limits too wide refuse create, a take-over keeps the drawn pose while
physics continues from the newest state, what the caller does to a body it
just claimed (moved to the hand, made kinematic) stays done, a copy is never driven past
`max_speed`, and a relayed state carries its age and stops past 63 ticks. Under the
second layer it also runs the pull's lag on a knocked copy, a held copy's step under
this side's box (no mark, no sweep kept) and a body returned at rest meeting the
host's first states.
Under authority following interaction it runs each rule on its own: a take
keeps the copy's pose, a taken body never goes out kinematic, a return waits
for the host's acknowledgement (the guest's packets arrive, their fates come
back lost) and for the owned bodies it touches to be quiet, the host's
character is stopped by a guest's body, a state from before the grant is
dropped, what the host's agent sets going stays the host's, an owned body
never takes a moving one, and the host takes back two guests' touching bodies. Every scenario, must-fail case and
the two-process run is a test on each implementation (`test_body_sync_stub_*`
for the stub). `test_body_sync_logic` (core) parses every shipped scenario, refuses a corpus
of broken lines with their line numbers, fuzzes the parser (truncations, bit
flips, random bytes), and checks the schedule, the host rules' verdicts, the
stand's records (round trip, direction, truncation, bounds, random bytes),
unbounded delays, histogram quantiles, the JSON's completeness and
repeatability, and the scene validator (bounds, spawn points inside bodies,
spawns per break). `test_body_sync_wire` (core) checks every field's limit of
the stub's codec and fuzzes it. `test_body_sync_stub` (core) runs three stub seams
over bytes: driven copies, stale ticks, claims and answers, a release that
crosses a grant, withdrawn claims, every claim polled, forced moves that keep
the velocity, poses of an old grant rejected, flags, motion type and holder
moving with authority, settled holds ending at a contact and at rest, queues
that never drop, opaque data both ways, a LIGHT copy its own character walks
through with its mask restored exactly at the take, and fuzz of both receive
calls through apply, a step and collect with Box3D's own asserts counted, a recall's placement
that stays where the host put it, and a guest's old generation stale after the host binds the slot
again. `test_body_sync_stand` (core) builds
every catalog scene on four endpoints with equal world hashes, walks the view,
and runs `two_walk` twice in one process for byte-equal JSON.
`test_body_sync_policy` (core) runs the authority policy alone and the takes it
drives through authority: a flight that meets a takable body stays its
thrower's, who takes what (a lean takes nothing, a push does; heavy, carried,
kinematic, host-set, foreign bodies and the two bounds refuse), the host's
grants, its agents first, the contested touch of two guests' bodies, the
return at rest and its bounds, stillness, the optimistic take on the wire with
the predicted number, a guest's count with its takes still waiting, a
refusal, two guests, a take-back, and a claim with an unknown hold as FORMAT.
The scenes `authority_host_push`, `authority_two_guests`,
`authority_two_guests_far` and `authority_leave` and the
second layer's, the throws' and the heap's scenes under `--authority
interaction` (`test_body_sync_interaction_scn_*`, slow) run it on the stand.
`test_body_sync_checks` (core) plants a defect after a clean run (a body only
a guest has, one a guest lacks, a moved copy, a kinematic body nobody holds, a
body under a seat that left, a knock applied twice, a broken body back, a
spawn missing, a break that lost its spawns, a pause held past the cap, a
still copy crept off its host body) and requires the check to fail, holds a
creeping awake host body's end of a rest to a host wake, and checks that the
world hash sees what it names.
`late_drops.count` counts what the host's seam dropped of a guest's bytes, as
stale or rejected, from each recall or removal of a body she simulated until she
stopped simulating it and the run's slowest one-way trip (and longest cut, stall
or deafness) after: her late packets. `recall_flight` and `remove_owned` recall
or remove the rock on tick 84, two ticks after it meets the glass on the
thrower's world: each way takes at least three ticks, so her claim and release
reach the host after the move, and the host's last taken state of the flight
lies within the gate's four ticks of the contact. The claim is refused as
revoked (a recall) or stale (a removal), the glass stands
(`mf_recall_claim` must fail on its refusal count), and on a host planted to
keep the recalled number standing the same claim breaks the glass
(`test_body_sync_claims_mf_recall_keeps_number`).
`test_body_sync_two_processes` (core) runs `two_walk` in two processes and
compares the files; `test_body_sync_two_processes_<name>` (slow) does the same
for the playback's edge scenarios, on both implementations. `test_body_sync_mf_*` (core) are scenarios that must end
with exit 1 on a named check, or 2 for refused input.
`test_body_sync_free_step` (slow) steps two worlds of every scene, all bodies
awake, for 180 ticks and requires equal hashes every tick: Box3D stays
bit-identical across worlds in one process. Every shipped scenario is the case
`test_body_sync_scn_<name>` (`two_walk` core, the rest slow); its `expect`
lines are its invariants, loose bounds rather than design numbers.

```powershell
node ai_studio/feature_testing/run.mjs
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "body_sync" -L core --output-on-failure
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "body_sync" -L slow --output-on-failure
```

The Box3D-free tests also run under ASan and UBSan with gcc and clang on Linux.

## Compatibility

`feature.json.version` is exact SemVer; before 1.0 a MINOR may break. A PATCH
fixes behaviour to match this contract. A MINOR adds calls, scenario
statements, metric names, catalog scenes or JSON fields with a compatible
default, or (before 1.0) changes the seam's calls, as 0.2.0 did: limits in the
config, the bind's number, claim sequences with `body_sync_answer`, and
`body_sync_assign` with a hold; 0.3.0 changed what the seam promises (writes
clamped to the limits, no inherited holder, stale releases); 0.4.0 added a
body's damping, a character's facing and activity, motion locks that travel
with the state, the report printer and the viewer's drawer hook; 0.6.0 the
playback receiver: opaque data, `character_category`, `delay_min` and
`delay_max`, `body_sync_playback`, the relayed origin on the wire, the scenario
words `rate` and `expect moved`, the playback metrics and schema
`body_sync.metrics.v2`; 0.8.0 authority following interaction: `authority`
and the hold until rest (two bits on the wire), the traffic's take counts, the
scenario word `authority`, `>=` in metric checks, the CLI and viewer flag
`--authority`, `body_sync_is_pull_paused`, `aligned_gap`, `local_correction`, `local_turn` and the take counts in the
metrics; 0.7.0 the second layer: `light_mass_max`,
`body_sync_is_predicted`, `body_sync_is_layer_copy`, `body_sync_round_trip`,
the scenario word `light_mass`, `expect moved ...
layer <length>`, the CLI and viewer flag `--light-mass`, and the layer
metrics. A MAJOR changes a call, a struct layout, the
meaning of a result, a scenario statement or the metrics schema name.
The real seam and the stub keep one contract and differ in
`body_sync_implementation()`; neither one's wire format is a compatibility
promise: the codec's hash (`sync_codec_t.hash`) names the real one for a
handshake.

## Extension points

A game adds its own scenes as `body_sync_scene_entry_t` entries (a name, a
summary and a function that fills a `body_sync_scene_t`) and runs the stand
with them through `body_sync_stand_main(argc, argv, extra, count)` from its own
target; an extra scene of a catalog name replaces it. Its drawer walks
`body_sync_stand_body` for one endpoint each frame and maps `model_tag` to its
models. The character's numbers are the scene's `character` tuning. Install:
[INSTALL.md](INSTALL.md).
