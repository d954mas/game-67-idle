# Game Capture Pipeline Implementation Plan

Status: approved implementation plan; WP0 implementation exists but its gate
was reopened by the post-commit contract review. WP1 measurements, cleanup, and
the WP1b process-loopback routing/isolation spike are complete, but no combined
adapter passed the exit gate. WP2/WP3 backend implementation remains blocked.

Implementation progress (2026-07-26): WP0 contract/safe-area/media-fixture
foundation is implemented and tested, but post-commit review returned `NO-GO`
and reopened the freeze gate. Remaining blockers are recorded in
[`CAPTURE-PIPELINE-REVIEW.md`](CAPTURE-PIPELINE-REVIEW.md). The universal social
policy remains truthfully incomplete until eligible
standard-organic geometry exists for its eight required LTR/RTL variants. WP1
ran on the interactive desktop. FFmpeg proved healthy exact-HWND video with
lower measured GPU/headroom cost and graceful stop, but failed
forced-interruption recovery and exposed no application-audio capability. OBS
Game Capture failed, while OBS Window Capture later proved healthy video and
active real-game process audio. OBS 30.1.2 still failed the lifecycle and
performance gates: startup preroll existed, the native-game shutdown crashed in
its WASAPI path, and it consumed materially more GPU/frame-time headroom. No
backend is selected. A Studio-owned ApplicationLoopback helper subsequently
proved finite 48 kHz game-process audio, foreign-process isolation, and real
game activity. The combined spike then found and fixed a regression from the
benchmark's exact-HWND input to desktop-region capture and now passes
`720x1280`, `30/1`, duration, frame-count, and active-audio topology. The
current automation run still returns nearly black exact-HWND pixels for an
unverified reason, so the pixel gate rejects the source, and streaming sync is
not established. See
[`CAPTURE-PIPELINE-WP1-SPIKE-REPORT.md`](CAPTURE-PIPELINE-WP1-SPIKE-REPORT.md),
[`CAPTURE-PIPELINE-WP1B-PROCESS-LOOPBACK-REPORT.md`](CAPTURE-PIPELINE-WP1B-PROCESS-LOOPBACK-REPORT.md),
and
[`CAPTURE-PIPELINE-RECORDER-BENCHMARK.md`](CAPTURE-PIPELINE-RECORDER-BENCHMARK.md).

Normative contract:
[`CAPTURE-PIPELINE-SPEC.md`](CAPTURE-PIPELINE-SPEC.md).

The plan delivers one Windows-first Studio recorder used by the template and
every subsequent game. Manual gameplay and authored reels share the same
recording kernel and real-time backend adapters. The optional deterministic PNG
backend is a separately gated follow-up and cannot delay audiovisual V1.

## 1. Delivery strategy

Implementation proceeds in vertical increments:

1. prove external audiovisual capture and isolation before building the full
   workflow;
2. build the backend-neutral recording kernel and artifact state machines;
3. ship the smallest useful manual gameplay command;
4. add game-owned stages and authored shots without changing the recorder;
5. add target fan-out, immutable masters, and delivery exports;
6. prove reuse in the template and one private second consumer.

Every increment starts with failing tests or an executable spike acceptance
script, ends with a user-observable workflow, and leaves the previous increment
usable. No phase edits `external/neotolis-engine`.

## 2. Target module shape

Runtime Automation owns a Python package under:

```text
ai_studio/runtime_automation/capture/
  __init__.py
  cli.py
  doctor.py
  errors.py
  models.py
  canonical_json.py
  targets.py
  jobs.py
  recording.py
  artifacts.py
  export.py
  process.py
  hotkeys.py
  probes.py
  backends/
    base.py
    obs.py
    ffmpeg.py
  shots/
    schema.py
    compiler.py
    scheduler.py
    devapi_stage.py
```

Normative JSON Schemas live under:

```text
ai_studio/runtime_automation/schemas/capture/
```

The reusable runtime C module remains:

```text
features/capture-stage-core/
```

Template-owned examples and smoke fixtures live with the template rather than
inside Studio tooling:

```text
templates/template/capture/
templates/template/src/capture/capture_stages.*
```

Each game owns:

```text
<game>/capture/config.json
<game>/capture/shots/
```

The config declares only build/launch, exact client-size arguments,
process/window identity, and application-audio topology. The common CLI and
backends are never copied into a game.

Generated recordings, attempts, and probes are ignored and stay in a game-local
`tmp/capture/` root. OBS configuration stays in the separately locked
Studio-owned writable root selected by WP1. Secrets, device display names, and
absolute private paths are redacted from shareable manifests.

The current flat prototype files are evidence, not compatibility contracts.
WP0 immediately marks `capture_scenario.py` experimental/deprecated because its
paired multi-step capture path has a known wrong-tick defect. Useful generic
client changes are classified and preserved by focused tests; capture-specific
paths are migrated or removed only after they have an explicit successor.

## 3. Core interfaces to freeze first

### 3.1 Recorder backend

```text
probe() -> BackendCapabilities
resolve(VideoSource, AudioPolicy, Target) -> ResolvedInputs
start(ResolvedInputs, staging_path) -> RecordingSession
status(RecordingSession) -> RecordingStatus
stop(RecordingSession, reason) -> StoppedRecording
inspect(StoppedRecording) -> ValidatedMaster
```

`BackendCapabilities` is data, not adapter-specific branching in the CLI. It
states supported source kinds, audio policies, dimensions/FPS, codecs,
multi-track support, interruption behavior, and diagnostic quality.

### 3.2 Recording state machine

```text
created
  -> preflighted
  -> countdown
  -> recording
  -> stopping
  -> validating
  -> promoted

created|preflighted|countdown|recording|stopping|validating
  -> failed|abandoned
```

Only `validating -> promoted` creates a master take. Stop is idempotent.
Cancellation, Ctrl+C, hotkey, game exit, backend exit, timeout, and low disk
are typed terminal reasons.

### 3.3 Artifact state machines

```text
RecordingJob -> Attempt -> immutable RealtimeMasterTake
EncodeJob    -> Attempt -> immutable DeliveryArtifact
```

An encode failure never changes a master. A failed or abandoned staging
directory never appears in `take list` as a take.

### 3.4 Mutable durable state

Immutable take directories are the source of truth. The take catalog is a
rebuildable versioned index updated under a cross-process lock by atomic
replace. Queue definitions are immutable; each execution writes a separate
versioned append-only run journal with idempotent attempt ids and recovery
rules. An interrupted real-time job always creates a new attempt.

### 3.5 Stable error families

Freeze machine-readable families before CLI copy:

- `CAPABILITY_MISSING`;
- `SOURCE_NOT_FOUND`;
- `AUDIO_SOURCE_NOT_FOUND`;
- `AUDIO_TRACK_MISSING`;
- `AUDIO_ACTIVITY_MISSING`;
- `AUDIO_ROUTING_INVALID`;
- `TARGET_UNSUPPORTED`;
- `SOURCE_SIZE_MISMATCH`;
- `BACKEND_START_FAILED`;
- `BACKEND_EXITED`;
- `GAME_EXITED`;
- `STOP_TIMEOUT`;
- `AV_VALIDATION_FAILED`;
- `LOW_DISK`;
- `CONTRACT_MISMATCH`;
- `RELEASE_SURFACE_LEAK`.

Each error includes a safe diagnostic payload and one actionable remediation;
tests assert codes, not prose.

## 4. Work packages

### WP0 — Contract freeze and executable fixtures

Deliver:

- schemas for target, audio policy, recording job, attempt, real-time master
  take, delivery preset, encode job, and delivery artifact;
- safe-area source-mask and derived-policy schemas, including platform/surface,
  placement class, layout variant, caption/UI state, locale/direction, source
  provenance, origin/license/redistribution, normalization, SHA-256 inputs,
  full canonical source-record hashing, hash-only deduplication, total canonical
  sort, and immutable derived-policy identity;
- canonical JSON and hashing rules;
- capacities, path limits, duration/byte limits, and state-transition tables;
- a synthetic audiovisual fixture: changing frame counter, color bars, 48 kHz
  tone, start/end sync impulses, and known duration;
- structural and timestamp assertions from `ffprobe`, plus decoded
  flash/impulse content-sync detection for controlled fixtures;
- inventory of legacy template `--capture`/PPM source, CLI, symbols, assets, and
  Release leakage;
- deprecation banner and a wrong-tick regression for the draft paired
  capture/step path;
- TikTok, YouTube Shorts, Instagram Reels, and Facebook Reels standard-organic
  evidence acquired or measured, classified, source-dated, license-reviewed,
  hashed, normalized to 1080x1920, and combined into the
  `universal-social-v1` intersection; official paid-ad checkers remain inputs
  only to explicit `ad-*` policies;
- four pinned delivery constraint sets for the same vertical MP4, each with
  platform/surface, official source, review date, and machine-checkable
  container/stream/media assertions.

Tests:

- schema positive/negative fixtures;
- canonicalization and hash golden tests;
- every legal and illegal state transition;
- corrupted, missing-track, wrong-routing, activity-required silence,
  wrong-size, wrong-rate, truncated, timestamp-drift, and controlled
  content-desync fixtures;
- common-domain/polarity checks, conservative rasterization, mask
  union/intersection, pinned measurable worst-case caption/UI variants, separate
  LTR/RTL readiness rows, proven identical-direction mask folding, canonical
  source-record/order independence, hash-only deduplication, transform/geometry
  hash invalidation, half-open tick-region coverage, missing evidence, and known
  inside/outside critical-content fixtures;
- one MP4 passes all four delivery constraint sets; a platform-specific codec,
  color, audio, duration, or size violation fails the responsible set.

Exit:

- all WP0 recording-path data crossing a process or artifact boundary has a
  schema/version; later stage/shot/queue schemas freeze at their own entry gate;
- each deliberately broken fixture fails at the correct structural, timestamp,
  or content-sync layer;
- the legacy prototype cannot be mistaken for acceptance evidence;
- `vertical-social-1080p60` cannot be advertised until every required source
  mask for the complete standard variant matrix is eligible,
  provenance/license-complete, and reproducibly derives the published policy
  hash; ad-only or incomplete evidence can provide guides but cannot produce a
  universal-safe `pass`.

### WP1 — OBS and FFmpeg feasibility spikes

#### OBS spike

Prove with one pinned OBS build whose capability floor is at least 30.1:

- `GetVersion`, RPC, available-request, source-kind, and application-audio
  runtime discovery;
- Studio-owned isolated profile/collection or managed portable tree without
  modifying an unmanaged user scene collection;
- global cross-process lock, random IPv4 loopback port, authentication, and
  verification of PID/executable/config-root/ownership token;
- credentials absent from command line, manifests, and logs;
- exact-window Game Capture or Window Capture;
- application audio bound to the selected game;
- optional named microphone on a separate track plus compatibility mix;
- programmatic start, status, stop, output-path discovery, and dropped-frame
  diagnostics;
- recoverable MKV after normal stop and forced interruption;
- reconnect/stop behavior for WebSocket loss, owned-process cleanup through a
  Windows Job Object or equivalent, deterministic attempt output path, orphan
  staging classification, and optional validated MKV salvage.

Reject the spike if isolation requires mutation of a user's active OBS
configuration or attachment to an unmanaged OBS process. Record the exact
supported OBS/protocol range and treat unknown future majors as unsupported
until tested.

#### FFmpeg spike

Prove with the repository-detected FFmpeg build:

- `gdigrab` by HWND and by exact desktop region;
- Windows Graphics Capture-backed exact-HWND `gfxcapture` when the detected
  FFmpeg build exposes it, without treating experimental presence as support;
- DirectShow device enumeration with backend/host-scoped identity and
  re-resolution rules;
- one explicitly selected audio device with measured separate-input clock model,
  start offset, drift, gaps, minimize/occlusion behavior, and soak result;
- graceful stdin stop and forced-stop recovery behavior;
- MKV validation and MP4 remux.

The spike must demonstrate that FFmpeg refuses `audio=game` unless a concrete
capability supplies application/loopback audio. It must not infer this from the
presence of `dshow`.

#### Windows application-loopback helper spike

Prove a small Studio-owned helper based on the official Microsoft
`ActivateAudioInterfaceAsync` process-tree loopback contract:

- bind only to the owned game PID and its children;
- emit timestamped 48 kHz PCM and explicitly measure whether endpoint or
  application mute/volume scalars affect it;
- feed PCM and exact-HWND FFmpeg video into one staging master;
- validate controlled flash/impulse offset, drift, gaps, and declared audio
  activity;
- demonstrate graceful stop, forced interruption, restart, ownership, cleanup,
  and unsupported-OS refusal.

This is the primary next candidate because it preserves the lean measured
FFmpeg video path without adding the OBS compositor. OBS remains a separately
gated manual/compatibility candidate and may be re-spiked only on a pinned
current stable build.

Progress (2026-07-26): the finite-WAV helper, PID/creation-time identity,
held-process lifetime, atomic publication, 48 kHz stereo format, continuity
reporting, real-game activity, and controlled process isolation pass.
Timestamped streaming, mute/volume behavior, A/V offset/drift,
interruption/restart, and the combined semantic video gate remain open. The
exact-HWND adapter now uses canonical `0x` hexadecimal, rebases timestamps, and
normalizes CFR. Its latest combined file passed topology with 145 decoded frames
at `30/1` over 4.91 seconds and active audio, but was rejected because the
exact-HWND source contained nearly black pixels for a cause not established by
the current evidence. The detected
`gfxcapture` filter is also not selected: one context stalled after its first
D3D11 frame and another failed setup with HRESULT `0x80070424`.

Exit:

- one adapter proves game/application audio end to end;
- both adapters emit a normalized capability report;
- the selected adapter's isolation and process ownership contract is frozen;
- the approved backend matrix is updated from measured evidence.

### WP2 — Recording kernel, targets, and doctor

Deliver:

- backend registry and capability-based `auto` selection;
- target resolver with the approved landscape, universal vertical social, and
  square targets plus aspect-preserving preview quality;
- safe-area policy resolver and `target describe` output for the combined mask,
  individual platform masks, supported surface matrix, provenance, and review
  dates;
- explicit audio policy parser and backend/host-scoped device ids;
- process supervision, countdown, stop-reason arbitration, disk monitoring,
  and staging lifecycle;
- canonical `python -m ai_studio.runtime_automation.capture` entrypoint plus
  repo-local `capture` wrapper;
- `<game>/capture/config.json` schema, game-root discovery, base
  `recording-native` configure/build/launch wiring, exact client-size arguments,
  process/window identity, and application/audio topology;
- template seed plus idempotent existing-game migration/check;
- `capture doctor`, `capture target list`, and `capture target describe`;
- redacted diagnostic bundle.

Tests:

- fake backends cover selection and all state transitions;
- an adapter cannot be selected when one requested capability is absent;
- `audio=none` is the only success path with no requested audio track; a
  present but silent requested stream succeeds with a warning unless
  `audio_expectation:activity_required` is declared;
- backend/host mismatch or ambiguous device re-resolution refuses to retarget;
- launch passes/measures exact requested client size; attach refuses mismatch
  unless explicit `--resize` is used;
- doctor distinguishes backend absence from missing game wiring;
- low-space preflight and runtime threshold behavior;
- no child process survives teardown in fault-injection tests.

Exit:

- `capture doctor` explains exactly why each standard command can or cannot run;
- every new template game has base recording wiring without recorder code;
- no recording has started yet, but all resolution is inspectable as a dry run.

### WP3 — Smallest useful manual gameplay recording

Deliver:

```text
capture live
capture live --attach
capture live --target vertical-social-1080p60 --audio game
capture live --audio game+mic:<device-id>
```

Behavior:

- infer the current game or require `--game`;
- launch `recording-native` at the target client size or resolve one explicit
  attached process/window;
- show resolved settings and countdown;
- preserve all game input;
- stop by dedicated hotkey, Ctrl+C, or explicit timeout;
- own the complete Attempt → validated immutable RealtimeMasterTake transition,
  including hashes, provenance, and `handoff.json` written last;
- automatically create the default immutable MP4 DeliveryArtifact unless
  `--master-only` is supplied; export failure does not invalidate the master;
- print take id, absolute MKV master, and MP4 delivery paths;
- never require `capture-stage-core`.

Tests:

- CLI parser and dry-run snapshots;
- multiple candidate window refusal rather than guessing;
- launch exact-size and attach mismatch/explicit-resize behavior;
- normal stop, hotkey, Ctrl+C, game exit, backend exit, and hung stop;
- 30-second controlled template capture with declared activity and game audio;
- `game+mic` track-map validation;
- interrupted promotion never creates a ready take;
- artifact inventory, hashes, handoff ordering, master full-decode, and default
  MP4 full-decode assertions;
- attach and launch parity.

Exit:

- a developer can record ordinary gameplay through the common CLI from every
  consumer that passes its declared application-audio topology contract.

### WP4 — Take catalog and additional delivery exports

Deliver:

- rebuildable versioned take catalog with locking, atomic replace,
  aliases/tags/accepted marker, and rebuild/migration outside immutable masters;
- `capture take list|show|tag|accept`;
- `capture export <take> --preset <id>`;
- additional validated delivery presets and explicit remux-versus-encode
  selection;
- explicit retention report and prune command with dry-run default.

Tests:

- concurrent catalog update, crash recovery, rebuild, and migration;
- repeat exports do not launch the game or mutate the master;
- one failed export does not invalidate other exports;
- filename/path collision and concurrent-attempt tests;
- remux refusal when a transform is required;
- delivery full-decode plus track title/disposition/default assertions.

Exit:

- recoverable MKV is the source of truth and shareable MP4 is reproducibly
  derived from it.

### WP5a — Stage-core contract freeze

Deliver:

- normative descriptor/control/action/status schemas and canonical descriptor
  fingerprint rules;
- lifecycle and teardown transition table with typed error mapping;
- fixed capacities and overflow behavior for catalog, controls, actions, plan
  ticks, transactions, arguments, upload chunks, receipts, and diagnostics;
- plan begin/append/commit/arm/status/receipt request/response schemas;
- exact host pre-update hook and 60 Hz pacing call-order contract;
- capture-only build/release guard contract.

Tests/review:

- schema and transition positive/negative fixtures;
- capacity boundary table;
- executable host call-order fixture design;
- one independent architecture/time review before C implementation.

Exit:

- specification section 19 stage-core entry gate has a versioned evidence index.

### WP5b — `capture-stage-core` implementation

Deliver:

- fixed-capacity catalog and typed descriptor validation;
- caller-owned lifecycle state and generation;
- strict controls/actions and typed errors;
- bounded chunked plan upload/commit/arm, game-frame pre-update execution,
  receipt hash chain, pacing diagnostics, and typed cleanup;
- `game.capture.*` DevAPI adapter including plan operations;
- `capture stage list|describe` Studio CLI with live discovery/schema tests;
- `authored-capture` target extending `recording-native` with DevAPI,
  capture-stage-core, public `nt_app_set_step_dt(1/60)` wiring, and reported
  effective timebase;
- removal or capture-only guarding of legacy template `--capture`/PPM surface;
- one template presentation stage using production scene/render/UI systems.

Tests:

- focused C catalog, duplicate, capacity, bounds, lifecycle, teardown, and
  reentrancy tests, including failed teardown terminal cleanup;
- DevAPI unknown/missing/wrong-type/non-finite tests;
- partial/chunk mismatch, plan hash, capacity, generation, arm, exact receipt
  ordering, lateness, and no-reuse-after-failed-teardown tests;
- effective timebase mismatch refusal;
- disconnect and partial-prepare cleanup;
- capture-build endpoint presence;
- native and Web Release legacy/new source, CLI, endpoint, symbol, flag, and
  asset absence.

Exit:

- stages are reusable game content contracts; no FFmpeg, OBS, files, shot JSON,
  or Studio process logic enters the feature.

### WP6 — Authored shot recording for reels

Deliver:

- shot/framing schemas and live descriptor validation;
- integer-tick timeline compiler;
- stage prepare/activate/warmup/teardown driver;
- `realtime_paced` driver: upload full plan, start/ack backend, explicit preroll,
  arm with lead time, game-side 60 Hz monotonic pacing, tick-0 presentation,
  lateness/backpressure abort without catch-up, postroll, stop, and receipt
  collection;
- `capture shot new|validate|preview|record`;
- stage registration recipe/scaffold and descriptor-driven shot scaffold;
- contact sheet and event-boundary evidence derived from the recorded master;
- clean/combined/per-platform safe-area preview modes and a framing validator
  that checks declared critical-content rectangles against the resolved common
  intersection without baking guides into recorded media;
- complete region evidence and provenance: normalized half-open rectangles,
  every authored tick, stage-evidence hash, validator version, policy hash, and
  per-region/per-interval results; empty or incomplete evidence is
  `not_measured`/`guidance_only`.

The game-side executor proves exact applied ticks. Semantic reproduction remains
`not_measured` unless full traces compare; media is never claimed deterministic.

Tests:

- timeline ordering, curve, overlap, exact-time, and duration fixtures;
- prepare failure, readiness timeout, action failure, disconnect, and teardown;
- backend-start loss, tick-0/preroll/postroll, pacing jitter, backpressure,
  lateness abort, and recorder/game time-anchor tests;
- 30/60 FPS recordings match generation, compiled plan, and applied-tick
  receipt chain while only external media FPS changes;
- preview and record share shot/framing/compiled-plan hashes while preserving
  their explicit quality/range differences;
- template portrait and landscape framing from one shot;
- one vertical delivery is selected unchanged for TikTok, YouTube Shorts, and
  Instagram/Facebook Reels and passes all four delivery constraint sets; known
  critical rectangles pass the complete standard matrix and one violation per
  source mask fails with the responsible platform/surface named;

Exit:

- one authored shot records in landscape, universal vertical social, and square
  without duplicating choreography or recorder code;
- preview evidence shows both the conservative combined safe region and each
  input overlay, while the master and delivery remain guide-free.

### WP7 — Queue, multi-target workflow, and private consumer

Deliver:

- ordered data queues with run/status/cancel/retry/resume;
- immutable queue definitions plus versioned append-only run journal, locking,
  idempotent attempt ids, crash recovery, and new-attempt retry;
- `capture shot record --targets` fan-out to independent recording jobs;
- `capture take compare|reproduce`, with explicit failure when a referenced
  executable/source snapshot is unavailable;
- concurrency one by default;
- completed-job preservation when a later job fails;
- private second-consumer integration and private evidence.

Tests:

- queue restart and partial-failure fixtures;
- catalog/queue concurrent access and recovery fixtures;
- interrupted realtime resume starts a new attempt and never appends a master;
- target-specific framing mismatch;
- no accidental shared mutable OBS/backend state between jobs;
- template and private consumer contract suites;
- materially different stages and parameterized content/action in the private
  consumer.

Exit:

- the recorder is declared reusable only after both consumers pass the same
  public contract without forks in Studio code.

### WP8 — Hardening and release gate

Deliver:

- 10-minute soak report;
- 1080p60 performance baseline;
- structural/timestamp/content-sync, activity expectation, routing,
  dropped-frame, and low-disk probes;
- privacy/redaction audit;
- release-surface scan;
- user documentation for install, doctor, live recording, shot recording,
  exports, safe-area policy refresh/review, failures, and recovery.

Exit:

- every required item in specification section 16.1 has linked evidence;
- 30-second 1080p60 has zero recorder-reported drops; 10-minute soak has at most
  0.1% dropped/duplicated frames, at most 50 ms end drift, no timestamp gap over
  two frame durations, stop below 5 seconds, and zero leaked owned processes;
- no high-severity issue remains open;
- every original prototype behavior is migrated with tests or explicitly
  retired; the known wrong-tick path is absent from supported workflows.

### WP9 — Optional deterministic offline frames

Start only after V1 audiovisual recording is accepted and the separate engine
contracts are proven through issues/PRs.

Deliver:

- exact render-target capture;
- presented-frame serial and simulation-frame identity;
- fixed-step shot scheduler and PNG frame index;
- semantic/pixel comparison and `capture shot render`;
- offline audio remains `none` until a separately approved `audio-core`
  deterministic mix contract exists.

This package has its own acceptance section and release decision. Failure to
ship it does not weaken or block the real-time recorder.

## 5. Dependency order and safe parallelism

```text
WP0
 +--> WP1 OBS spike ----+
 +--> WP1 FFmpeg spike -+--> WP2 --> WP3 --> WP4
 +--> WP5a --> WP5b ------------------+
                                           |
                             WP3 + WP5b --> WP6
                              WP4 + WP6 --> WP7 --> WP8

accepted V1 + approved engine contracts --> WP9
```

The two backend spikes can run independently. WP5a can proceed in parallel
after WP0; WP5b starts only after its independent contract review. Integration,
artifact promotion, and shared CLI edits remain single-owner work to avoid
divergent state machines.

## 6. Verification matrix

| Requirement | Primary proof |
| --- | --- |
| One recorder for all games | template plus private consumer contract suites |
| Simple recording | `capture live` 30-second audiovisual functional test |
| Reels/scripted scenes | one shot, three framing targets, matching applied-tick receipt chains |
| One universal social file | same 1080x1920 delivery hash passes four pinned platform constraint sets |
| All supported safe zones | complete standard-organic variant matrix, eligible provenance-locked source masks, reproducible intersection, critical-content positive/negative/incomplete-evidence fixtures |
| Game sound | each consumer topology probe plus controlled activity/content-sync fixture |
| Microphone combinations | compatibility mix and isolated track-map assertions |
| No silent fallback | capability and missing-track negative tests; silence fails only when activity is declared |
| Interruption safety | Ctrl+C, hotkey, forced backend exit, recoverable MKV |
| Different sizes | exact 1920x1080, 1080x1920, 1080x1080 masters |
| Reusable exports | repeat MP4 export without game launch or master mutation |
| Release safety | native/Web release symbol, endpoint, asset, and flag scan |
| Provenance | normalized job, capability, environment, media probe, and hashes |

## 7. Implementation review gates

Independent review is required at these points:

1. after WP0: schemas, state machines, error taxonomy, privacy model;
2. after WP1: backend feasibility and audio truthfulness;
3. after WP3: manual recording failure behavior and process cleanup;
4. after WP6: stage lifecycle and timeline semantics;
5. after WP8: full acceptance evidence and release exclusion.

Normal logic receives one independent reviewer. Process concurrency, credential
handling, and release-surface changes receive two.

## 8. Non-goals that must not enter V1

- deterministic replay of arbitrary human input;
- editor/NLE features, titles, captions, or publishing;
- automatic crop/reframing between aspect ratios;
- platform-specific upload APIs;
- paid-ad anchors/cards/interactive overlays in the standard publishing safe
  policy; these use explicit stricter policy variants;
- direct edits to Neotolis;
- mandatory offline frames or deterministic PCM;
- a per-game recorder or OBS scene maintained by each game.

## 9. Definition of done

V1 is done when:

- `capture live` records controlled manual gameplay from the template with
  declared application audio and creates MKV plus default MP4;
- `capture shot record` records one authored shot in landscape, universal
  vertical social, and square through the same recording kernel;
- the vertical result is one 1080x1920 TikTok/Shorts/Reels delivery validated
  against the complete eligible `universal-social-v1` matrix and four pinned
  delivery constraint sets, not separate platform renders;
- OBS and FFmpeg capabilities are discovered honestly and no requested audio
  policy can degrade silently;
- interruption-safe MKV masters and validated MP4 deliveries are separate,
  immutable artifacts;
- the template and private second consumer pass the same public and
  application-audio topology suites;
- capture-only runtime surface is absent from release builds;
- specification section 16.1 has an evidence index and no unresolved blocker.
