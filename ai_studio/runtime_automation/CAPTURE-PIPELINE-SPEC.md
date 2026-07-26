# Universal Game Capture Pipeline Specification

Status: APPROVED V1 PRODUCT AND ARCHITECTURE SPECIFICATION. Implementation has
not started; backend-specific proof gates are defined in sections 16 and 19.

Target consumers:

- `templates/template`;
- one designated private second consumer, whose identity and evidence remain
  inside its private repository;
- all subsequent games created from the template.

The existing recorder and private-game capture catalog are prototypes and may be
replaced completely. This document specifies the intended product contract, not
compatibility with that draft.

V1 is a Windows-first Studio capability. The command and backend contracts are
portable, but macOS, Linux, and Web capture parity are not V1 acceptance gates.
Studio owns one CLI and one copy of the recorder adapters. The template gives
each game only the declarative launch/window/audio-topology wiring needed by
that CLI; games add optional stages and shots for authored recording.

## 1. Product outcome

A developer or agent can record manual gameplay or define a reusable game shot
once, preview it cheaply, and record complete videos with selected audio in
several sizes,
aspect ratios, frame rates, and delivery formats without writing a new recorder
for each reel. The same shot may also use a slower frame-sequence backend when
pixel-exact sampling matters more than real-time playback.

The system supports:

- one-command manual gameplay recording with game sound;
- different game scenes and seeded content;
- production gameplay or dedicated presentation stages;
- portrait, landscape, square, and custom exact output sizes;
- 24/25/30/50/60 FPS profiles where the runtime timebase can represent them;
- shot controls, actions, keyframes, and curves;
- externally recorded real-time gameplay and authored shots with game audio;
- fresh-process deterministic offline frame takes where required;
- batch queues and multiple takes;
- interruption-safe audiovisual masters or lossless source frames, plus derived
  delivery files;
- explicit provenance, diagnostics, comparison, and failure state;
- separate claims for deterministic choreography, semantic reproduction, and
  media/pixel reproduction.

The two primary user journeys are:

```text
# Play normally; start after a countdown; stop with the configured hotkey.
capture live --target landscape-1080p60

# Launch a game-owned deterministic presentation and record it with sound.
capture shot record showcase.motion --target vertical-social-1080p60
```

Both journeys resolve the same recording job and use the same recorder backend
interface. A game does not implement recording, containers, or encoding. It
declares its process/window/audio topology; `audio=game` is available only when
that consumer passes the application-audio capability contract.

## 2. Terminology

- **Stage**: a game-owned, runtime-discoverable controllable content provider.
  It may present a normal game scene, a prepared gameplay state, or a dedicated
  debug presentation setup.
- **Shot**: game-owned choreography against one stage, authored on a simulation
  timeline and independent of output resolution and FPS.
- **Framing variant**: shot overrides for one or more compatible aspect/layout
  classes, typically camera, framing, HUD, safe-area, or presentation controls.
- **Capture profile**: exact master size, FPS, backend kind, source policy,
  quality, and backend-specific frame/audio settings.
- **Target**: the user-facing named choice that resolves a compatible framing
  variant, capture profile, safe-area policy, and optional delivery preset.
- **Delivery preset**: a versioned encoder/container/audio policy applied to an
  accepted master take.
- **Recording job**: an internal resolved snapshot of live/scripted source +
  framing + target + real-time audiovisual profile.
- **Render job**: an internal resolved snapshot of shot + framing + offline
  frame profile. Users normally author neither job directly.
- **Encode job**: an internal resolved snapshot of master take + delivery
  preset.
- **Queue**: an ordered list of jobs.
- **Attempt**: one execution that may fail and retain bounded diagnostics.
- **Master take**: one successfully promoted immutable audiovisual recording or
  offline frame result.
- **Delivery artifact**: one successfully promoted immutable encode result that
  references a master take hash.

These identities are separate. Editing an H.264 bitrate never changes shot
choreography; changing output FPS never moves an action in game time.

## 3. Ownership and boundaries

### 3.1 `capture-stage-core`

The reusable in-place L1 runtime module owns:

- fixed-capacity stage catalog validation and lookup;
- caller-owned active-stage state, generation, seed, lifecycle, and terminal
  status;
- typed control/action descriptor validation;
- validation, storage, arming, and frame-thread execution of one bounded
  precompiled real-time tick-transaction plan;
- strict `game.capture.*` DevAPI adaptation;
- activation and teardown ordering;
- structural compile guards.

It does not own:

- scene navigation or history;
- world update, rendering, cameras, entities, UI, or resources;
- shot JSON, curve interpolation, capture transport, files, FFmpeg, or evidence
  bundles;
- general gameplay replay.

Studio compiles curves and events into the bounded tick plan. The runtime module
does not parse shot JSON or interpolate curves; it only applies validated tick
transactions at the game update boundary and reports receipts.

### 3.2 Game

Each game owns:

- its static stage catalog and stage callbacks;
- stage preparation and teardown;
- seeds, fixtures, content variants, cameras, HUD choices, and semantic state;
- shot and framing documents;
- production systems reused by capture stages;
- stage-specific tests.

A stage may call `scenes-core` through the game's normal host adapter. It must
   not duplicate scene lifecycle inside capture-stage-core.

### 3.3 Studio Runtime Automation

`ai_studio/runtime_automation` owns:

- schema validation and live contract discovery;
- capture-build launch and process isolation;
- manual-time scheduling;
- capture backend selection;
- capture profiles, jobs, queues, previews, takes, comparison, and encoding;
- immutable master takes, delivery artifacts, mutable take-selection catalog,
  and failure diagnostics.

### 3.4 Neotolis

Neotolis already owns public:

- manual fixed-step time;
- exact render-target creation and rendering;
- default-framebuffer readback and pre-swap capture.

Proven gaps are tracked in the capability matrix in section 14. Future
deterministic offline mixing belongs first to `features/audio-core`, not to
Neotolis. Engine deficiencies are handled through a focused issue and PR after
root-cause proof, never by editing the submodule working tree.

## 4. Runtime stage contract

### 4.1 Identity

Stage ids match `[a-z_][a-z0-9._-]{0,126}` and are sorted and unique.
Every descriptor has:

- `id`;
- `contract_version`;
- title/summary;
- supported framing/aspect compatibility tags;
- controls;
- actions;
- callbacks.

The catalog is immutable after initialization. Stage code and descriptors are
game-owned and pointer-stable for the capture runtime lifetime.

V1 jobs require exact `contract_version` equality and store the canonical live
descriptor fingerprint. Any control/action id, kind, bound, default,
mutability, argument, semantic meaning, framing compatibility, or diagnostic
schema change increments the version. Compatibility relaxation may be designed
later; V1 never guesses it from an integer alone.

### 4.2 Controls

V1 control kinds:

- `bool`;
- bounded `int`;
- bounded finite `float`;
- closed `enum`.

Every control declares id, kind, default, mutability, and kind-specific bounds
or values. Mutability is the closed enum `setup_only | timeline`; setup-only
controls are fixed before activation and timeline controls may occur in an
armed plan. Vector/camera values are represented by explicit scalar controls
in V1. Arbitrary strings, JSON blobs, object pointers, and implicit coercion are
not supported.

Controls represent presentation or stable setup knobs, for example:

```text
camera.yaw
camera.pitch
camera.distance
population
presentation
hud.visible
```

### 4.3 Actions

An action has a stable id and zero or more typed arguments using the same scalar
kinds as controls. The adapter validates every key, type, bound, and enum before
calling game code.

Examples:

```text
showcase.trigger {}
demo.impulse {strength: float}
demo.variant.select {item: enum}
```

Actions are semantic game operations, not keyboard coordinates or UI array
indices.

### 4.4 Callbacks

The game-facing behavior is:

```text
prepare(seed, setup controls) -> typed result
activate()                    -> typed result
set_control(id, typed value)  -> typed result
invoke_action(id, typed args) -> typed result
status()                      -> lifecycle, semantic identity/diagnostics
teardown()                    -> typed result
```

The lifecycle is normative:

| State | Allowed operations | Transition |
| --- | --- | --- |
| `inactive` | `prepare` | accepted → `preparing`; rejected → `inactive` |
| `preparing` | `status`, `teardown` | resource-ready → `ready`; terminal error → `failed` |
| `ready` | `activate`, `teardown` | accepted → `active`; rejected → `failed` |
| `active` | `set_control`, `invoke_action`, `status`, `teardown` | callback error → `failed` |
| `failed` | `status`, `teardown` | successful teardown → `inactive`; failed teardown → terminal cleanup required |

`prepare` may initiate asynchronous resource work while positive-dt simulation
is frozen. Readiness polling has a wall-clock timeout and must not mutate causal
game state. `activate` resets the deterministic state from seed and resolved
setup controls. Any required positive-tick settling is an exact declared
`warmup_ticks` count after activation; variable “tick until ready” is invalid
for a deterministic claim.

Every accepted `prepare` requires one teardown attempt on success, failure,
disconnect, or shutdown. Teardown is idempotent for the same generation.
Generation increments only after an accepted `prepare`. Control/action
callbacks are non-reentrant and execute on the game frame thread. The normal
game frame loop updates and renders the stage; capture-stage-core has no update
or draw callback; the host invokes a narrow pre-update tick-plan hook. A failed
teardown preserves terminal diagnostics, forbids reuse of that generation, and
forces process cleanup. The implementation freeze adds exact typed error codes
and a transition test table.

### 4.5 Semantic status

Status includes:

- active stage id and contract version;
- generation and seed;
- lifecycle state;
- engine simulation frame;
- committed plan hash, last applied logical tick, receipt-chain hash, and
  pacing lateness when a plan is armed;
- optional stable semantic hash;
- optional bounded diagnostic fields declared by the stage.

A semantic hash covers causal game state, not camera pixels or memory padding.
Timeline tick is recorder-owned and derived from an engine-frame baseline after
warmup. Comparison reports `match`, `mismatch`, or `not_measured`; absence of a
semantic hash is never treated as a match.

## 5. DevAPI contract

The generic method family is:

```text
game.capture.list
game.capture.describe
game.capture.prepare
game.capture.activate
game.capture.control.set
game.capture.action.invoke
game.capture.plan.begin
game.capture.plan.append
game.capture.plan.commit
game.capture.plan.arm
game.capture.plan.status
game.capture.plan.receipts
game.capture.status
game.capture.teardown
```

Rules:

- live `endpoints` and `command.describe` discovery precede use;
- strict params reject unknown, missing, duplicate, wrong-type, non-finite,
  out-of-range, and unknown-id values before mutation;
- responses include API version, game id, stage contract version, and
  generation where relevant;
- plan upload is chunked, bounded, generation-scoped, hash-verified, and atomic
  only at `commit`; partial uploads cannot be armed;
- an armed plan executes inside the game frame loop, not through per-tick
  network round trips;
- only one stage is active;
- preparing another stage requires explicit teardown;
- the adapter is absent unless both capture tooling and DevAPI are enabled;
- release binaries advertise no `game.capture.*` methods.

Static JSON Schema validates generic document shape. The live stage descriptor
validates game-specific control and action ids and bounds. There is no second
hand-maintained command database.

## 6. Shot document

Shots live under:

```text
<game>/capture/shots/<shot-id>.shot.json
```

The V1 shape is conceptually:

```json
{
  "schema": "ai_studio.capture_shot",
  "version": 1,
  "id": "showcase.motion",
  "game": "example-game",
  "stage": {
    "id": "demo.presentation",
    "contract_version": 1,
    "seed": 42
  },
  "clock": {
    "timebase_hz": 60,
    "warmup_ticks": 30,
    "duration_ticks": 600
  },
  "initial_controls": {
    "hud.visible": false
  },
  "tracks": [
    {
      "kind": "action",
      "at_tick": 240,
      "action": "showcase.trigger",
      "arguments": {}
    }
  ],
  "framing": {
    "landscape": {
      "controls": {
        "camera.distance": 5.0
      }
    },
    "portrait": {
      "controls": {
        "camera.distance": 3.2
      }
    }
  }
}
```

The implementation freeze supplies JSON Schemas for shot, framing, target,
capture profile, render job, master take, encode job, and delivery artifact.
Normalized snapshots use UTF-8 canonical JSON with sorted object keys, no
duplicate keys, normalized finite numbers, and schema-defined array ordering.
Canonical JSON v1 emits mathematically integral finite values with integer
tokens, including both signed zero spellings as `0`; non-integral finite
numbers retain their JSON number type. `1` and `1.0`, and likewise `0.0` and
`-0.0`, therefore have one identity hash.
Hashes cover those normalized bytes. The illustrative maps above do not define
wire order.

Framing variants are versioned canonical subdocuments with independent hashes;
they may be authored inline or in reusable game-local files. Resolution
precedence is:

```text
stage descriptor defaults
< shot setup controls
< selected framing overrides
< tick-0 timeline writes
```

Setup-only controls are fully resolved before `activate`; timeline writes to
them are invalid. A capture profile/target may require compatibility tags but
never changes controls implicitly.

### 6.1 Timeline

Shot time is normalized to integer simulation ticks, never output-frame
numbers or binary floating-point seconds. Authoring may use ticks, exact
rational seconds, `4s`, or `00:04.000`; validation rejects a time that cannot
be represented exactly by the declared timebase.

Track kinds in V1:

- one-time control set;
- one-time action invocation;
- numeric control curve with `linear` or `smoothstep` interpolation.

For every logical tick, the canonical plan applies control writes in
control-id order and then actions in authored array order. A second write to
one control on the same tick, including curve/event overlap, is invalid. Curve
endpoints are inclusive and literal; intermediate values use exact rational
`u=(tick-start)/(end-start)` before the declared interpolation is evaluated.
The complete per-tick plan is compiled and hashed before launch. It is
evaluated on every simulation tick, including ticks that are not output
samples. All ids and values are validated against the live stage descriptor.

### 6.2 Offline output sampling

This section applies only to `offline_frames`. Let the shot interval be the
half-open interval `[0, duration_ticks)`, the timebase be `H`, and reduced
positive FPS be `P/Q`. The offline backend requires exact media duration:

```text
duration_ticks * P % (H * Q) == 0
frame_count = duration_ticks * P / (H * Q)
sample_tick(i) = floor(i * H * Q / P), 0 <= i < frame_count
```

V1 requires `P <= H*Q`, checked 64-bit arithmetic, positive bounded operands,
and `sample_tick < duration_ticks`. `floor` means “latest completed simulation
state at or before the output timestamp.” At 60 Hz this supports:

- 60 FPS: one tick per sample;
- 30 FPS: two ticks per sample;
- 24 FPS: alternating two/three-tick intervals;
- 25 FPS: a deterministic two/three-tick cadence.

Timeline events and curves are applied at every simulation tick, so changing
offline output FPS changes sampling density, not game-time choreography. Events
on unsampled ticks still execute. Semantic samples for comparison are recorded
per simulation tick, not only per output frame.

For `realtime_av`, target FPS configures the external recorder only. It never
changes `H`, the compiled plan, or applied logical ticks. Intended choreography
duration is `duration_ticks / H`; actual media PTS, duration, start offset, and
end drift are measured rather than derived from the offline frame-count formula.

### 6.3 Authored execution protocols

Common preparation is:

1. V1 validates effective `H == 60`; the capture build applies public
   `nt_app_set_step_dt(1/60)` and reports it through the game capability
   contract;
2. enter manual mode and freeze positive-dt simulation;
3. `prepare`, poll resource readiness to a wall-clock deadline, then `activate`;
4. execute exactly `warmup_ticks` fixed updates without recording and freeze;
5. record engine frame as `stage_tick_origin`; logical timeline starts at 0.

#### `realtime_paced`

1. Start the external backend and wait for positive recording acknowledgement.
2. Hold the warmed stage during an explicit profile-defined preroll.
3. Commit/arm the previously uploaded plan with a bounded lead time. The
   game-side host owns monotonic deadlines `t0 + k/H`, applies tick transaction
   `k` in the pre-update hook, and uses public `nt_app_step(1)` for each
   `k > 0`. Tick 0 is applied and presented without a positive-dt update.
4. Never merge or skip logical ticks to catch up. If lateness or backpressure
   exceeds the profile limit, mark timeline execution failed and stop safely.
5. After the final tick, hold the terminal state for explicit postroll, stop the
   backend, and validate the media.

Per-tick DevAPI round trips are forbidden. Receipts record generation, plan
hash, logical tick, engine frame, callback result, and lateness; their canonical
hash chain proves exact tick execution. Metadata records backend media start,
`t0`, preroll/postroll, intended choreography duration, measured media duration,
and A/V offsets. External OBS/FFmpeg frames need no presentation serial.

#### `offline_exact`

1. Apply tick 0, perform a zero-dt present, then capture `P0` when sampled.
2. For each logical tick `k > 0`, apply its transaction, execute exactly one
   fixed update, present `Pk`, and capture only when `k` is the next sample tick.

An offline capture result returns both presentation serial and engine simulation
frame. A permanent template probe proves tick 0 and multi-step ordering. There
is no game-specific fallback probe in the universal recorder.

## 7. Framing, target, and size

### 7.1 Exact surface

A target requests an exact bounded master size. Supported aspect ratios include
portrait, landscape, square, and explicit dimensions within backend capability
limits.

For `realtime_av`, a controlled native client area plus a measured external
window/capture region is a valid backend. Preflight verifies actual captured
dimensions and records monitor, DPI, chrome, source, and region; mismatch fails
before the take. On launch, the target's exact requested client size is passed
through the game capture-launch contract and then measured. Attach never mutates
an existing window by default; a mismatch fails with remediation to relaunch,
choose a matching target, or explicitly use `--resize`. For `offline_frames`,
the stronger contract is an exact render target independent of monitor/window
layout. These are different backends, not
a workaround and its “real” replacement.

### 7.2 Scaling

- no implicit crop;
- no implicit upscale;
- same-aspect integer supersampling and one explicit pinned downsample are
  allowed;
- a shot may declare a default framing valid for named aspect classes;
- an explicit aspect-specific framing variant takes precedence;
- failure to resolve a compatible framing is a validation error.

One high-resolution take may produce smaller same-aspect delivery files without
recording again. Portrait and landscape normally require separate takes because
camera and UI framing differ.

### 7.3 Capture profile

Shared profiles live under Studio tooling; games may add local profiles.
A profile always declares:

- id/version;
- logical framing aspect and exact master width/height;
- rational FPS;
- quality mode and capture backend requirements.

A `realtime_av` profile additionally declares master container, video/audio
codecs, rate control, audio policy/track requirements, preroll/postroll, maximum
timeline lateness, structural/timestamp validation tolerances, and required
backend diagnostics.

An `offline_frames` profile additionally declares exact render-surface size or
integer scale, pinned downsample implementation/filter/color policy, PNG master
format, and opaque `rgb24`, 8-bit, top-left, square-pixel, progressive SDR/sRGB
master pixels.

HUD remains an explicit shot/framing control, never a hidden profile toggle.
A user-facing target may combine platform-safe-area requirements with a
framing tag, capture profile, and delivery preset. It may not silently crop,
pad, reframe, interpolate FPS, or override shot choreography.

### 7.4 Delivery preset

A versioned delivery preset fixes container, video/audio codecs, dimensions and
allowed transform, FPS policy, pixel format, color primaries/transfer/matrix/
range, sample aspect, rate control, GOP, profile/level, audio layout/rate,
metadata policy, and post-encode assertions. Platform policy data records its
source and review date.

Encoding records the FFmpeg binary hash, version/build configuration, exact
arguments, environment, stderr, and `ffprobe` result. A software encoder is the
reproducibility baseline; hardware encoding is explicit opt-in with no silent
fallback. Output is written to a temporary file, fully decoded/probed, then
atomically promoted. Delivery byte identity across different encoder builds is
not promised.

Remux and encode are separate operations. Remux is allowed only when container
codec compatibility, track mapping, timestamps, dimensions, FPS, color, and
audio require no transform; otherwise the delivery is encoded. Both paths run a
full decode plus probe. The compatibility audio mix remains the default track
and all titles/dispositions are asserted after the operation.

### 7.5 Universal vertical social target and safe areas

V1 produces one reusable 9:16 master/delivery for TikTok, YouTube Shorts, and
Instagram/Facebook Reels. It does not record or encode a separate platform copy
when the media requirements are otherwise identical.

`vertical-social-1080p60` is 1080x1920 at 60 FPS and resolves
`safe_area_policy: universal-social-v1`. The policy is the geometric
intersection of the versioned safe masks for every supported surface.
Equivalently, its unsafe mask is the union of all platform UI exclusion masks.
That equivalence is valid only after every input is normalized to the same
1080x1920 frame domain, pixel-edge convention, and safe/unsafe polarity.
Safe-area policy affects framing guidance and validation only; it never crops,
scales, or bakes guides into the master or delivery.

The policy has two semantic regions:

- `critical-safe`: faces, focal gameplay, logos, HUD controls, calls to action,
  and essential text must remain inside the common intersection;
- `background-allowed`: non-essential world/background imagery may fill the
  complete 1080x1920 frame and may be covered by platform UI.

Masks may be raster or polygonal and are not reduced to one guessed rectangle.
Their external-media coordinate space is `[0,1] x [0,1]`, origin top-left,
positive Y down; game/world/UI Y-up data is converted only at this boundary.
Normalized rectangles are half-open `[x0,x1) x [y0,y1)`. Rasterization at
1080x1920 floors each minimum edge, ceils each maximum edge, and treats any
touched unsafe pixel as a failure.

An authored social framing declares non-empty `critical_regions[]` with stable
ids and optional half-open tick intervals `[start_tick,end_tick)`. A static
region covers the complete shot. Dynamic region evidence supplies bounds for
every authored logical tick in its interval; missing ticks, empty declarations,
or incomplete evidence can only produce `not_measured`/`guidance_only`, never
`pass`. Every declared rectangle must fit the combined mask for its complete
interval. Static HUD/text reservations can be validated before recording;
moving subject bounds are validated from stage evidence after recording.
Manual `capture live` uses the same guides but reports `guidance_only` unless
the game supplies equivalent complete bounds.

Every source mask records platform/surface, placement class
(`organic_standard`, `paid_ad`, or `measured_organic`), variant,
locale/direction, caption/UI state, source URL, upstream filename/version when
present, retrieval and review dates, original dimensions, normalization
transform, SHA-256, `origin`, acquisition method, license, and redistribution
status. It also records evidence authority (`platform_official`,
`first_party_measurement`, or `third_party`) and an explicit license-review
state. Non-redistributable upstream assets never enter git. Normalized geometry
is committed only when the license permits it; otherwise the policy stores a
reproducible acquisition/import recipe and treats the local source as an
external prerequisite.

Each source record has `source_record_hash = SHA-256(canonical source-record
payload excluding source_record_hash)`. That payload contains provenance and
license fields, platform/surface/placement,
variant ids, locale/direction, caption/UI state and measurable bounds, original
and normalized dimensions, transform, source hash, and normalized geometry
hash. Inputs are deduplicated only by `source_record_hash` and sorted by
`(platform, surface, placement_class, ui_variant_id, caption_variant_id,
direction, locale, source_record_hash)`. Every derived policy identity/hash is
likewise excluded from its own canonical hash payload. The derived policy hash
includes the canonical list of source-record hashes and the derived safe-mask
hash. Input
enumeration order therefore cannot affect geometry or identity. A platform UI
update creates a new policy version; it never mutates an accepted take.

V1's exact standard-publishing variant matrix is:

| Surface | Placement | UI variant id | Caption variant id | Direction |
| --- | --- | --- | --- | --- |
| TikTok feed | organic standard | `playback-default-visible.v1` | `collapsed-standard-max.v1` | LTR |
| TikTok feed | organic standard | `playback-default-visible.v1` | `collapsed-standard-max.v1` | RTL |
| YouTube Shorts | organic standard | `playback-default-visible.v1` | `collapsed-standard-max.v1` | LTR |
| YouTube Shorts | organic standard | `playback-default-visible.v1` | `collapsed-standard-max.v1` | RTL |
| Instagram Reels | organic standard | `playback-default-visible.v1` | `collapsed-standard-max.v1` | LTR |
| Instagram Reels | organic standard | `playback-default-visible.v1` | `collapsed-standard-max.v1` | RTL |
| Facebook Reels | organic standard | `playback-default-visible.v1` | `collapsed-standard-max.v1` | LTR |
| Facebook Reels | organic standard | `playback-default-visible.v1` | `collapsed-standard-max.v1` | RTL |

Every row is a separate readiness requirement. LTR and RTL rows may reference
one normalized mask only when captured evidence proves identical normalized mask
hashes. Each source record pins the platform/app surface version and gives the
caption variant a measurable bound: maximum visible collapsed lines/state plus
its normalized obstruction geometry and mask hash. A name such as
`collapsed-standard-max.v1` without those pinned bounds is incomplete evidence.

Expanded comments/descriptions, keyboard/share sheets, shopping UI,
paid-ad-only anchors, download cards, interactive add-ons, and transient OS UI
are separate placements. They require explicit stricter policies and are not
silent inputs to `universal-social-v1`.

Only proven `organic_standard` inputs may produce an official standard-policy
`pass`: the source authority must be `platform_official`, its license review
must be complete, its source snapshot hash and non-empty normalized geometry
must exist, and its caption bound must be measurable. A stored
`source_record_hash` mismatch is corruption, not a value to overwrite. If a
platform publishes only an advertising checker, that source is
classified `paid_ad` and belongs to an `ad-*` policy. A measured standard
organic UI mask is classified `measured_organic`, pins app version, device
class, locale, capture evidence, and review date, and keeps the policy status
`measured`, not `official`. Until every row in the matrix has complete eligible
evidence, `vertical-social-1080p60` remains available for recording and guides
but may not be advertised or reported as universally safe.

`capture target describe vertical-social-1080p60` lists all source masks,
variants, hashes, and review dates. Preview and contact-sheet evidence can show
the combined common mask and toggle each source mask independently. The clean
recorded media never contains those overlays. Take provenance stores region
declarations, stage-evidence hash, validator id/version, policy hash, and a
per-region/per-interval result.

## 8. Jobs, queues, and commands

The routine CLI is noun-scoped and generates internal jobs:

```text
capture doctor
capture stage list|describe
capture shot new|validate|preview|record|render
capture target list|describe
capture take list|show|tag|accept|compare|reproduce
capture export
capture queue run|status|cancel|retry|resume
```

Routine recording defaults are intentionally short:

- canonical invocation is
  `python -m ai_studio.runtime_automation.capture`; a repo-local `capture`
  wrapper exposes the short form;
- current game discovery resolves a versioned `<game>/capture/config.json`
  containing game id, base recording build/launch rule, exact client-size
  arguments, process/window selector, and application-audio topology;
- `capture live` launches the production-like base recording build unless
  `--attach` is supplied; it never requires DevAPI or `capture-stage-core`;
- the default target is `landscape-1080p60`; the default target for the
  reels/social workflow is `vertical-social-1080p60`;
- the default audio policy is `game`;
- the default backend is `auto`;
- `Esc` is never stolen from the game; the recorder stop hotkey is configurable
  and shown during the countdown;
- the resolved backend, source, audio policy, output path, and stop control are
  printed before recording begins.

`capture doctor` is the installation and preflight authority. It reports
available backends, versions, source capabilities, audio devices, encoders,
free space, output permissions, and actionable remediation. A recording command
does not silently change backend, audio policy, target, or encoder after
preflight.

V1 `preview` uses the same compiled shot, framing resolution, real-time paced
clock, and tick executor as `record`, with an explicitly linked low-cost quality
variant and/or time range. Preview preserves the selected aspect: landscape is
1280x720, vertical social is 720x1280, square 720x720, all at 30 FPS. It creates a
playable proxy plus a contact sheet and event-boundary frames and is marked
`acceptance:false`; process reuse is allowed.

`capture shot record <shot> --targets <a,b>` fans out required V1 real-time
jobs. The optional offline backend later adds the equivalent
`capture shot render`. `--compare-to <take>` uses current code/current shot;
`take reproduce` uses stored normalized inputs and requires the referenced
executable/source identity to remain available. Comparison and reproduction
never invent a replacement for a missing snapshot.

Running inside a game infers it, while `--game` is an override. Queues are
ordered data, not Python scripts. A frozen executable snapshot may serve a
queue, but each accepted scripted job starts a fresh process. Completed jobs
remain usable when another queue item fails.

## 9. Capture modes

### 9.1 External real-time recording

This is a required V1 backend and the default way to create an ordinary
finished clip:

- `capture live [--attach|--launch] [--target <id>] [--audio <policy>]`;
- `capture shot record <shot> --target <id> --audio game`;
- preserves manual player input for live gameplay;
- can drive an authored seeded shot through the same stage/timeline contract;
- captures the selected window/surface with FFmpeg, OBS, or another external
  recorder;
- captures game/application, system-loopback, and/or selected device audio;
- supports countdown plus explicit stop/hotkey;
- writes an interruption-resilient audiovisual MKV master and automatically
  creates the default validated MP4 delivery unless `--master-only` is set;
- records actual frame pacing, audio source, sync, and dropped-frame
  diagnostics.

Neotolis does not need to expose a video or PCM stream for this backend.
`record_screen_ffmpeg.ps1` and `DevApiClient.start_recording()` already prove
the external video-capture seam; V1 extends that adapter contract with explicit
audio-source discovery/selection and audiovisual validation.

#### Recorder backend contract

All real-time adapters implement the same operations:

```text
probe() -> capabilities
resolve(video_source, audio_policy, target) -> resolved_inputs | error
start(resolved_inputs, staging_path) -> recording_session
status(recording_session) -> timing/drop/audio diagnostics
stop(recording_session) -> stopped_result
inspect(stopped_result) -> validated_master | error
```

The Windows V1 backend policy is:

1. No production adapter is selected until one passes the full measured exit
   gate. The first local comparison proved FFmpeg exact-HWND video and OBS
   Window Capture audiovisual capability, but neither passed every lifecycle,
   interruption, isolation, and performance requirement.
2. `ffmpeg` is the leading measured exact-HWND video adapter and the common
   probe/remux/export tool. The installed build must not claim per-application
   or system-loopback audio unless `probe()` discovers and validates a concrete
   source that supplies it.
3. The primary next audiovisual candidate combines FFmpeg video with a
   Studio-owned Windows process-tree audio helper based on
   `ActivateAudioInterfaceAsync`. The finite-WAV spike has passed process-tree
   routing, real-game activity, and controlled foreign-process isolation, but
   has no streaming clock/sync proof. A follow-up fixed a regression from the
   successful benchmark's exact-HWND input to desktop-region capture: the
   combined path now passes dimensions, CFR, frame-count, duration, and
   active-audio topology, but the current automation run returns nearly black
   exact-HWND pixels for an unverified reason, so the pixel gate rejects it. Its
   helper now binds PID to Windows creation time, retains the process handle,
   reports continuity diagnostics, and atomically publishes qualified finite
   WAVs. The adapter is eligible only after end-to-end process ownership,
   timestamped streaming PCM, A/V sync, activity, stop, interruption, restart,
   cleanup, mute/volume, and unsupported-OS refusal pass.
4. `obs` remains an optional manual/compatibility adapter. OBS eligibility
   requires a pinned current stable version range, Window/Game Capture and
   application-audio runtime probes, bounded preroll readiness, measured
   performance, and clean shutdown. Studio controls only an OBS process whose
   PID, executable, configuration root, and ownership token it created and
   verified. It never attaches to or edits an unmanaged OBS instance.
5. `auto` selects the first installed adapter that satisfies the requested
   video, audio, target, and diagnostic capabilities. Selection is
   capability-based, not a catch-all fallback chain, and remains disabled until
   at least one adapter passes the complete exit gate.

The default game-audio topology is one selected window plus one unambiguously
bound application process. Helper processes or multiple instances require an
explicit `application_set` in game capture config and a passing consumer probe.
The universal guarantee is the CLI and truthful capability contract, not that
every arbitrary process topology can be isolated.

If no installed adapter can satisfy the requested audio policy, recording
fails before countdown with a concrete `doctor` remediation. Recording video
silently, capturing an arbitrary default microphone, or changing `game` to
`system` is forbidden.

The default real-time master is MKV because it is designed to remain recoverable
after a process/power interruption; promotion still requires full validation.
MP4 is a separate immutable delivery artifact produced by a validated remux or
encode. Adapter profiles pin codecs and rate control; V1
ships a broadly compatible H.264/AAC software baseline and may offer explicit
hardware-encoder profiles without promising byte reproducibility.

An authored shot may be deterministic while the recorded media is not
byte/pixel deterministic. Such a take records:

```text
timeline_execution: exact_tick | failed
semantic_trace: measured | not_measured
media_reproducibility: none
```

`exact_tick` means every game-side receipt matches the compiled plan and
generation. It does not by itself prove semantic reproduction. A semantic
result becomes `match` or `mismatch` only when two complete per-tick traces are
compared; otherwise it is `not_measured`. Manual live takes use
`timeline_execution:not_applicable`.

### 9.2 Offline frame-sequence render

This optional high-confidence backend is used for exact sample ticks,
slower-than-real-time rendering, pixel comparison, or higher-quality masters:

- fresh capture build;
- manual fixed-step time;
- exact output sampling;
- lossless PNG master frames;
- no dropped or duplicated frame outside the declared rational schedule;
- immutable frame master;
- `audio_source:none` until an offline deterministic mix source is implemented.

Lack of an offline PCM tap never blocks real-time recording with sound.

### 9.3 Deterministic input replay

Deterministic replay of arbitrary human input is a third lane and is deferred
to V2. Authored semantic shots, manual live gameplay, and input replay are
distinct contracts.

## 10. Master and delivery artifacts

V1 has two immutable master kinds:

1. `realtime_av`: interruption-safe container with recorded video and selected
   audio tracks, timing/drop diagnostics, plus normalized shot/framing/target
   snapshots when the recording was scripted;
2. `offline_frames`: numbered PNG frames, normalized shot/framing/profile/job
   snapshots, and normative `frame-index.jsonl`.

For `offline_frames`, each frame index row records frame number, simulation
tick, rational PTS, path, artifact SHA-256, and canonical-pixel SHA-256. PNG
files are written
temp → decode/validate/hash → atomic rename. Encoding consumes the index rather
than a filesystem glob and rejects holes, duplicates, or unexpected files.
For `realtime_av`, the recorder validates streams, duration, timestamps, and
decodability before promotion. Delivery files are derived artifacts.
Re-encoding/remuxing never reruns the game.

Each successful take is an immutable directory:

```text
<take-id>/
  master.json
  job.json                   when scripted
  shot.json                  when scripted
  profile.json
  stage-describe.json        when scripted
  executable.json
  environment.json
  recording.mkv              realtime_av
  frame-index.jsonl          offline_frames
  frames/                    offline_frames
  contact-sheet.jpg
  boundaries/
  tick-receipts.jsonl         when scripted
  diagnostics.json
  semantic.jsonl
  provenance.json
  handoff.json              written last
```

Delivery artifacts live separately and never mutate a master:

```text
deliveries/<take-id>/<preset-id>/<encode-id>/
  preset.json
  source.json
  command.json
  ffprobe.json
  output.<container>
  provenance.json
  handoff.json
```

The recorder writes an attempt into `.<attempt-id>.staging` and atomically
promotes it to a take only after the master and provenance are complete. A
staging directory is a failed/abandoned attempt, never a take. A failed encode
does not invalidate its referenced master. Each retry creates a new immutable
delivery artifact and never overwrites an earlier result.

Large generated artifacts stay under ignored game-local `tmp/` roots. Accepted
reel projects copy or reference a take explicitly; the recorder never edits an
NLE project.

Take aliases, tags, notes, and accepted markers live in a rebuildable versioned
external catalog; immutable take directories remain the source of truth. The
catalog uses a cross-process lock and atomic replace, supports migration and
rebuild, and never mutates a master. Export is allowed from any valid take;
acceptance affects retention and human selection, not export permission.
`latest` and `latest:<shot>:<target>` are computed aliases.

Queue definitions are immutable. Each queue execution has a separate
versioned append-only run journal with idempotent attempt ids, locking, recovery,
and explicit terminal events. Resume skips completed jobs and starts a new
attempt for interrupted real-time recording; it never appends to a partial
master.

Before rendering, the driver calculates
frame count and conservative master/transient/wire byte estimates, checks free
space and configured `max_frames`, `max_duration`, and `max_master_bytes`, and
uses queue concurrency `1` by default. It streams and hashes one frame at a
time, aborts safely on a low-disk threshold, never auto-deletes accepted or
abandoned data, and exposes explicit retention/prune operations.

## 11. Provenance and reproducibility

Every take records:

- executable and relevant source/build identity;
- game, engine, feature, stage, shot, framing, profile, and job versions
  and hashes;
- capture backend and capability report;
- OS, GPU, driver, framebuffer, color, Python, FFmpeg, and encoder versions;
- seed and timebase; scripted takes also record compiled plan and tick receipts;
  offline takes additionally record the exact sample schedule and pixel hashes;
- every payload artifact path, byte count, and SHA-256;
- terminal successful completion status; failed attempts keep their own
  diagnostics outside the take.

Comparison has three distinct results:

1. **input identity**: normalized inputs match;
2. **semantic reproducibility**: event trace and semantic samples match;
3. **pixel reproducibility**: canonical decoded pixel hashes match.

`artifact_sha256` proves a PNG file's integrity. `pixel_sha256` covers width,
height, pixel/color contract, row order, and decoded RGB bytes; PNG encoding
metadata and compression cannot change it. `provenance.json` inventories
payload artifacts but excludes itself and `handoff.json`; `handoff.json`
contains the provenance hash and is written last. Encoded video hashes are
provenance, not the determinism oracle. Cross-environment pixel equality is
never promised unless measured under a matching environment fingerprint. A
take may be semantically reproducible while pixel comparison is
backend/environment-scoped.

## 12. Audio

Real-time V1 records sound through the external recorder. The capture job
selects one explicit policy:

| Policy | Required streams |
| --- | --- |
| `game` | audio emitted by the selected game process/application |
| `system` | selected system output/loopback |
| `mic:<device-id>` | one explicitly resolved input device |
| `game+mic:<device-id>` | game audio plus the named input |
| `system+mic:<device-id>` | system output plus the named input |
| `none` | intentionally silent recording |

`doctor` enumerates backend capabilities and backend/host-scoped device ids; all
persisted ids are re-resolved before use. The take records the requested policy,
resolved devices/processes, backend, clock model, track map, and measured
timestamps. `none` must be explicit. Silent fallback is forbidden when any
audio stream was requested.

The default sample rate is 48 kHz. A master always includes a compatibility mix
on track 1. Combination policies require multi-track capability: track 2 is the
isolated game/system source and track 3 is the isolated microphone, with titles,
source ids, codecs, dispositions, and routing matrix recorded. A backend without
those tracks cannot satisfy `game+mic` or `system+mic`. Delivery presets state
which tracks they mix or copy; they never guess.

Validation is reported in three independent layers:

1. `structural`: stream count, codec, rate, layout, track routing;
2. `timestamp`: first/last PTS, monotonicity, gaps, start offset, end drift;
3. `content_sync`: decoded flash/impulse correlation for a controlled fixture or
   authored marker, otherwise `not_measured`.

Silence is a warning for arbitrary gameplay and a failure only when the job
declares `audio_expectation:activity_required` with a controlled activity
window. Wrong-source and duplicate-routing failures require a controlled marker
or provably invalid routing; ordinary correlated mix/isolated tracks are not
duplicates.

This is independent of engine PCM. The absence of a Neotolis/audio-core mix tap
does not make ordinary videos silent and does not block scripted real-time
recording.

The offline frame backend declares `audio_source:none` in V1. A future
`deterministic_mix` source belongs to a separate `audio-core` integration
contract and must fix sample rate, sample format, channel layout, start tick,
sample count, and latency. The default proposal is 48 kHz stereo PCM, with:

```text
S(t) = floor(t * sample_rate / timebase_hz)
tick t owns samples [S(t), S(t+1))
```

Offline deterministic-mix acceptance requires start/end sync impulses accurate
to one sample. External real-time audio is validated for presence, decodability,
duration, timestamp monotonicity, and configured sync tolerance; it is not
evidence of deterministic PCM.

## 13. Build and release policy

Add two native targets distinct from:

- the human `native-debug` build;
- the general-purpose `devapi-debug` agent build;
- shipping Release.

1. `recording-native`: optimizations, production assets/presentation, exact
   client-size launch arguments, and stable process/window identity. It has no
   DevAPI or stage dependency and serves `capture live`.
2. `authored-capture`: extends `recording-native` with DevAPI,
   capture-stage-core, the game-owned stage catalog, the 60 Hz public-API host
   wiring, and capture diagnostics. It serves `capture shot`.

Both disable debug overlays unless explicitly requested. The base target and
its template capture config exist before simple recording is implemented.

Normal Release must contain no:

- capture stage sources/catalog;
- `game.capture.*` endpoints;
- recorder launch flags;
- DevAPI transport;
- capture-only assets.

The existing template `--capture`/PPM path and unconditional capture source are
legacy surface. WP0 inventories them; implementation removes them or guards them
behind capture-only build flags before they can be used as V1 evidence.

Suggested flags:

```text
FEATURE_CAPTURE_STAGE_CORE
GAME_CAPTURE_ENABLED
GAME_DEVAPI_ENABLED
```

The final flag spelling is fixed during implementation and recorded in
`INSTALL.md`.

## 14. Capability research matrix

| Capability | Current evidence | Owner and next action |
| --- | --- | --- |
| Manual `step_dt` | Neotolis already exposes `nt_app_set_step_dt`; MANUAL consumes it exactly. DevAPI lacks set/query, and the prototype validates a timebase without applying it | No engine change for real-time: the authored capture host fixes/reports 60 Hz through the public C API. A set/query bridge is only an optional offline ergonomics issue |
| Exact surface | Controlled window/region capture already satisfies `realtime_av`. Public exact render targets exist, but public capture/readback addresses only the default framebuffer | No engine change for real-time recording. Optional `offline_frames` has a narrow Neotolis gap: explicit render-target readback or a host-registered capture source |
| Presented-frame synchronization | Deferred pre-swap capture exists, but paired `capture.frame` + `time.step(N)` captures after the first step, not the Nth, and cannot capture prepared tick 0 | Proven Neotolis DevAPI gap: zero-advance presentation capture or atomic `capture.after_step`; response returns presentation serial and sim frame |
| Deterministic PCM | Neotolis has no audio subsystem; `features/audio-core` owns the mixer/backend seam | Defer to an audio-core integration design unless product scope promotes audio into V1 |

Do not file a generic “add offscreen rendering” or engine-audio issue: those
would assign the wrong root cause. The engine submodule remains read-only; each
proven engine change ships through a focused issue and PR.

Base64 PNG over JSON-lines is a compatibility backend, not the scalable master
transport. It needs a measured 1080p gate and conservative transient-memory,
wire-size, and timeout limits. Dense 4K/high-volume work requires a later local
writer or binary pipe without changing shot/profile schemas.

## 15. Failure behavior

Validation or execution fails loudly for:

- missing endpoints/capabilities;
- stage/profile/framing/target mismatch;
- unknown or invalid controls/actions;
- invalid timebase/FPS/sample schedule;
- framebuffer size/aspect mismatch;
- requested audio track/source missing or undecodable; declared activity/sync
  expectations failing when they are measurable;
- process exit or transport disconnect;
- missing, duplicate, or out-of-order correlated responses;
- dropped/duplicate master frames;
- master artifact hash failure;
- source/build race;
- release-surface leakage.

Failures retain bounded diagnostics in staging but never write a ready handoff.
No partial take is silently selected as the newest candidate. Encode failures
are terminal only for their encode attempt and do not invalidate the source
master.

## 16. Acceptance

### 16.1 Required V1 recorder acceptance

The universal Studio recorder is reusable only after all of the following pass:

1. `capture doctor` proves FFmpeg, Windows process-loopback, and optional OBS
   capability detection without mutating user configuration;
2. a 30-second controlled gameplay take records the selected game window and
   expected application audio into a validated MKV, and both consumers pass
   their declared application-audio topology probe;
3. `game+mic` produces the declared compatibility and isolated track map;
4. missing tracks/sources and invalid routing fail; controlled activity and
   flash/impulse sync failures fail, while unmeasurable arbitrary gameplay is
   reported as `not_measured`;
5. Ctrl+C, configured stop hotkey, game exit, recorder exit, and low disk all
   produce bounded diagnostics and no false-ready take;
6. preview and full recording share shot/framing/compiled-plan hashes while
   recording their explicitly different quality/range inputs;
7. exact 1920x1080 and 1080x1920 takes from one shot with compatible explicit
   framing variants;
8. the 1080x1920 take produces one byte-identical delivery artifact that passes
   four pinned, source-dated delivery constraint sets for TikTok, YouTube
   Shorts, Instagram Reels, and Facebook Reels, including container, codecs,
   dimensions/aspect, FPS, pixel/color metadata, audio, duration, and size where
   the platform publishes a limit;
9. its complete critical-content fixture fits the eligible, fully evidenced
   `universal-social-v1` standard matrix; deliberate violations of every input
   mask fail validation, while empty or incomplete region evidence cannot pass;
10. exact 1080x1080 recording and one same-aspect 1280x720 delivery pass;
11. the designated private consumer records at least two materially different
   stages and one stage with parameterized content/action; its identity and
   evidence stay private;
12. a shot recorded at 30 and 60 FPS produces matching generation, plan hash,
    and game-side applied-tick receipt chains; only external media FPS changes;
13. encoding/remux can be repeated from the master without launching the game;
14. MKV to MP4 remux/export passes full decode, stream, duration, timestamp,
    dimension, frame-rate, audio-rate, and sync assertions;
15. normal native/web Release contains no capture sources, symbols, endpoints,
    flags, or assets;
16. default `capture live` promotes MKV and creates a separate validated MP4
    delivery; `--master-only` suppresses only the delivery;
17. feature contracts and both consumer suites pass;
18. the 30-second 1080p60 functional take has zero recorder-reported drops; the
    10-minute soak has at most 0.1% dropped/duplicated frames, at most 50 ms
    measured end drift, no timestamp gap above two frame durations, stop latency
    below 5 seconds, and zero leaked owned processes. CPU/GPU load,
    bytes/second, and encode speed are recorded for capacity planning.

### 16.2 Optional offline backend acceptance

The optional `offline_frames` backend is available only after:

1. focused catalog/control/action/lifecycle C tests;
2. strict DevAPI schema and runtime envelope tests;
3. integer timeline/sample-schedule tests for 24/25/30/50/60 FPS;
4. two fresh template runs match per-tick semantic traces and canonical pixel
   hashes under the same environment fingerprint;
5. tick-0 and multi-step probes prove returned simulation-frame and
   presentation-serial identity;
6. exact render-target capture, frame indexing, atomic writes, disk preflight,
   low-disk abort, and abandoned-attempt classification pass.

## 17. Out of scope for V1

- non-linear reel editing, titles, subtitles, or publishing;
- a general save/replay/rollback system;
- player-facing replay capture in shipping builds;
- deterministic replay of arbitrary human input;
- arbitrary render passes, object ids, HDR/EXR, alpha, depth, or motion vectors;
- automatic reframing/cropping between aspect ratios;
- multi-camera cuts inside one take;
- distributed render farm execution;
- Web capture parity;
- motion-blur subframe accumulation.

The data model leaves room for these, but V1 adds no speculative hooks beyond
the capture-backend boundary and versioned documents.

## 18. Reviewed and approved decisions

Accepted design recommendations:

1. Name the runtime feature `capture-stage-core` and the user-facing capability
   “Game Capture Pipeline”.
2. Keep it separate from `scenes-core` and keep Studio tooling out of its
   dependencies.
3. Make external real-time audiovisual recording the required practical V1
   backend; keep offline PNG-sequence rendering as an optional exact-sampling
   backend.
4. Keep the internal stage → shot → framing → profile → job → take model, but
   expose the simpler shot → target → take → export workflow.
5. Normalize shot time to simulation ticks. Real-time FPS affects only the
   external recorder; offline FPS chooses exact sample ticks.
6. Allow a declared compatible default framing plus explicit aspect/platform
   overrides; never crop or reframe implicitly.
7. Separate immutable master takes from immutable delivery artifacts.
8. Implement the template and a private second consumer before declaring the
   runtime module reusable.

Resolved V1 product choices:

1. Deterministic replay of arbitrary human input is deferred. V1 supports
   manual live play and exact-tick authored execution; semantic reproduction is
   claimed only when full traces compare.
2. Windows V1 requires `game`, `system`, named microphone, game-plus-microphone,
   system-plus-microphone, and explicit `none` audio policies. Availability is
   capability-gated per backend; no silent downgrade is allowed.
3. Studio ships these capture targets:

   | Target id | Master | FPS | Framing |
   | --- | --- | --- | --- |
   | `landscape-1080p60` | 1920x1080 | 60 | `landscape` |
   | `vertical-social-1080p60` | 1080x1920 | 60 | `portrait` |
   | `square-1080p60` | 1080x1080 | 60 | `square` |

   Preview is a quality variant of the selected target, not a fourth framing
   target.

4. `vertical-social-1080p60` is the single TikTok/Shorts/Reels media target and
   resolves the versioned, non-destructive `universal-social-v1` intersection.
   Platform overlays remain inspectable separately. Their official source,
   variant, hash, and review date are mandatory, and policy updates do not
   change shot choreography or mutate old takes.

## 19. Implementation entry and exit gates

Gates are incremental, not a cyclic prerequisite for starting the work:

1. recorder kernel entry: recording/capability/target/artifact schemas, typed
   errors, canonicalization, and state transitions are frozen;
2. real-time adapter entry: one combined exact-window/application-audio path
   passes isolation, ownership, sync, activity, performance, and lifecycle
   spikes;
3. simple live exit: explicit source/audio selection, interruption-safe stop,
   master promotion, and default MP4 delivery pass;
4. stage-core entry: descriptor, lifecycle, plan-upload/arm, capacities, and
   release guards are frozen;
5. authored recording entry: the game-side 60 Hz paced tick-plan protocol and
   start/preroll/tick-0/postroll handshake pass the controlled fixture;
6. queue/catalog entry: their separate versioned schemas, locking, journal, and
   recovery contracts are frozen.

The external real-time backend may be implemented without a Neotolis change.
The optional offline backend remains separately gated until:

1. the presented-frame engine contract is accepted and proven by tick-0 and
   multi-step tests;
2. exact render-target capture is available;
3. the old prototype's timebase application and captured-tick bug are fixed.
