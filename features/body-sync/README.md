# Body sync

## Purpose

Two things for co-op physics on Box3D, both without any game in them:

- **The sync seam** (`body_sync`): the narrow interface through which one
  endpoint's bodies are written to bytes and applied from bytes. Bodies carry
  network ids with a generation, an authority (the seat that simulates them)
  with a number that grows on every change, an optional holder and a few state
  bits. The game moves the bytes, usually over `coop-net`, and keeps the
  policy: what may be grabbed, who wins a dispute. The implementation in this
  version is the **stub**: every bound body's raw pose, velocities and motion
  type to every seat, every tick, in as many packets as it takes; optimistic
  claims, each with the claimant's sequence, answered one by one by the host.
  It is the reference for the contract and the baseline the real sender,
  receiver and authority are measured against.
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
| `features/body_sync/body_sync.h` | The seam: `create` with the box and speeds a peer's numbers must stay in, seats joined and left, `bind`/`unbind` an id to a `b3BodyId` with its starting authority and number, authority (`claim`, `release`, `poll_claim`, `answer`, `assign`, `set_holder`, `set_flags`), the tick (`set_peer_tick`, `receive_state`, `receive_record`, `apply`, `collect`, `write_state`, `state_sent`, `unresolved`/`resolve`, `peek_record`/`pop_record`), `render_offset`, `traffic` by kind. Includes Box3D. |
| `features/body_sync/body_sync_scene.h` | Scenes as C data: body descriptors (shape, size, mass or density, friction, restitution, linear and angular damping, pose, motion, role, flags, model tag, routes, breakable templates), player spawns, the character's tuning, `validate`, the 8-scene catalog and lookup in it or in a caller's table. |
| `features/body_sync/body_sync_scenario.h` | The scenario grammar (top comment), the parsed struct, `parse` and `load`, which refuse with a line and a reason. |
| `features/body_sync/body_sync_metrics.h` | Metric definitions and names, the report structs, schema `body_sync.metrics.v1`. |
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
  a body without a claim with `body_sync_assign`. Only the claimant's newest
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
a crate with contents), `load300` (300 bodies, four players).

The test character mirrors a game's mechanics: a dynamic capsule moved by
setting its velocity, rotation locked, its facing written into its rotation
as a yaw (so it travels with the pose) and its activity into the seam flags'
bits 1..3 (bit 0 is the knock); carrying makes the body kinematic with
collision mask 0 and teleports it to the hold point each step; a throw makes it
dynamic again with a ballistic velocity and a settled hold; a punch is a cone
test in front, with no physics shape. Host rules stand in for a game's: first
grab claim wins, the loser is refused; a hit claim knocks a mannequin down,
breaks a breakable or pushes an item; an impact claim from the endpoint that
simulates a flying body breaks what it hit; breaking removes the body and
spawns its contents and debris from the host's random draws; what a player who
left carried falls where it is.

## Running the stand

The feature consumer builds the stand and its tests; run the owner command,
then a scenario with the command line it built:

```powershell
node ai_studio/feature_testing/run.mjs
ai_studio/feature_testing/consumer/build/feature-verify/_body_sync/body_sync_stand.exe features/body-sync/scenarios/two_walk.scn --out two_walk.json
```

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

[at <time>] <p0..p3> walk <x> <z> [speed <v>] | walk <body> [as <label>]
[at <time>] <player> grab <body> | throw <x> <y> <z> [speed <v>] | throw <body> | drop | punch [<body>] | wait <time>
[at <time>] net <g1..g3> [to_host|to_guest] rtt|delay|jitter|reorder_delay <time> loss|dup|reorder <share> rate <B/s>|off
at <time> cut <guest> for <time>     # nothing passes; 10 s or more disconnects
at <time> stall <host|gN> for <time> # a long frame, then the missed ticks, 8 a frame
at <time> deaf <host|gN> for <time>  # steps and sends, reads nothing

expect agree_at_rest <length> [<angle>] | no_stuck | effects_once | left <guest>
expect result <label> <done|granted|refused|hit|missed|rejected|failed>
expect one_of <outcome> <label> <label> ...
expect <metric> <= <value>
```

Units are required: `250ms`, `1.5s`, `90t` (ticks), `5cm`, `0.05m`, `3deg`,
`2%`, `64KB/s`; `speed` is a bare number in m/s. A positive time that rounds
to zero ticks is refused. An action with `at` starts then and cuts off what the
player was doing; one without it starts when the player's previous action
ends. A grab is optimistic: the action ends at once, and its label's outcome
becomes `granted` or `refused` when the host's answer arrives. A grab out of
reach fails; scenarios walk first. An angle written in `agree_at_rest` is
always a bound, `0deg` included. With `guests 0` the checks that compare a
guest are refused. The whole of `scenarios/edge_throw_cut.scn`:

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
player sees, not as an error of the sync. A lag-aligned divergence comes with
the receiver's playback buffer. The definitions are in `body_sync_metrics.h`.

A guest whose session ended fails `agree_at_rest`, `no_stuck` and
`effects_once` with "gN left at tick T" unless the scenario says `expect left
gN`; a check never passes on a guest it did not compare.

Network situations covered by the shipped scenarios: a same-tick grab dispute
(`grab_dispute`), a throw with the thrower's path cut in flight
(`edge_throw_cut`), a 200 ms frame on a guest and on the host (`edge_stall`),
four throws into one heap from three guests at 50, 100 and 200 ms
(`load_four`), a guest that stops reading for 3 s (`edge_deaf`), and hostile
bytes on every parser (seeded fuzz in the tests). Not yet covered: a grab of
a body flying under another guest's authority, a revoke in flight with late
packets, a removal under a guest's authority, a sleeping body moved by the
host, a reused slot meeting an old packet, more moving bodies than a budget
allows, a record over the unfragmented limit, a reader that stalls for 10 s,
reconnects, joins and the host leaving (the seam has no baseline yet), a
tick-rate mismatch, the 16-bit packet number wrapping, a guest pushing a stack
or standing on a host's body, a hit on a target that already moved on the
host, and a host whose window is minimised.

## Validation

`test_body_sync_logic` (core) parses every shipped scenario, refuses a corpus
of broken lines with their line numbers, fuzzes the parser (truncations, bit
flips, random bytes), and checks the schedule, the host rules' verdicts, the
stand's records (round trip, direction, truncation, bounds, random bytes),
unbounded delays, histogram quantiles, the JSON's completeness and
repeatability, and the scene validator (bounds, spawn points inside bodies,
spawns per break). `test_body_sync_wire` (core) checks every field's limit of
the stub's codec and fuzzes it. `test_body_sync_stub` (core) runs three seams
over bytes: driven copies, stale ticks, claims and answers, a release that
crosses a grant, withdrawn claims, every claim polled, forced moves that keep
the velocity, poses of an old grant rejected, flags, motion type and holder
moving with authority, settled holds ending at a contact and at rest, queues
that never drop, and fuzz of both receive calls through apply, a step and
collect with Box3D's own asserts counted. `test_body_sync_stand` (core) builds
every catalog scene on four endpoints with equal world hashes, walks the view,
and runs `two_walk` twice in one process for byte-equal JSON.
`test_body_sync_checks` (core) plants a defect after a clean run (a body only
a guest has, one a guest lacks, a moved copy, a kinematic body nobody holds, a
body under a seat that left, a knock applied twice, a broken body back, a
spawn missing, a break that lost its spawns) and requires the check to fail,
and checks that the world hash sees what it names.
`test_body_sync_two_processes` (core) runs `two_walk` in two processes and
compares the files. `test_body_sync_mf_*` (core) are scenarios that must end
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
with the state, the report printer and the viewer's drawer hook. A MAJOR changes a call, a struct layout, the
meaning of a result, a scenario statement or the metrics schema name.
Replacing the stub with the real sender, receiver and authority keeps the
seam's contract and changes `body_sync_implementation()`; the stub's wire
format belongs to the stub and is not a compatibility promise.

## Extension points

A game adds its own scenes as `body_sync_scene_entry_t` entries (a name, a
summary and a function that fills a `body_sync_scene_t`) and runs the stand
with them through `body_sync_stand_main(argc, argv, extra, count)` from its own
target; an extra scene of a catalog name replaces it. Its drawer walks
`body_sync_stand_body` for one endpoint each frame and maps `model_tag` to its
models. The character's numbers are the scene's `character` tuning. Install:
[INSTALL.md](INSTALL.md).
