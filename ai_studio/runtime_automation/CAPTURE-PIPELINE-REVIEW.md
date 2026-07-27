# Historical Game Capture Pipeline Review and Critique

> **Superseded on 2026-07-27.** This review describes the foundation before
> the OBS recorder shipped in `3cce0ba65`; its `NO-GO`, "no backend selected",
> and WP gate language is historical. Current normative status is maintained
> only in [`CAPTURE-PIPELINE-SPEC.md`](CAPTURE-PIPELINE-SPEC.md). The measured
> spike findings remain useful evidence, but they do not reopen FFmpeg-versus-
> OBS selection or authorize jobs/queues/backend-selection work.

Date: 2026-07-26.

Verdict: **NO-GO after implementation review of commit `45cfbe504`**.
The earlier design convergence remains useful, but executable WP0 contracts
are not frozen and no production backend is selected.

## 0. Post-commit implementation review

Four independent read-only reviews of `45cfbe504` reopened the gate. Immediate
findings fixed after that commit are:

- completed-process cleanup no longer acts on a raw/recycled PID and the
  private spike no longer invokes `taskkill` through `PATH`;
- the spec-only `capture-stage-core` entry is absent from template runtime
  dependency seeds;
- screenshot subprocesses have a bounded deadline and normalized failures;
- the deprecated scenario runner emits `prototype-rejected` and
  `captureBeforeStep:false`, never a ready exact-tick handoff;
- public scenes-core evidence no longer names or depends on a private consumer.

The remaining blockers are deliberately not papered over:

1. delivery pass/fail contracts do not yet type every claimed codec, color,
   GOP/rate-control, SAR, and audio-layout requirement;
2. recording jobs do not freeze the full resolved transitive dependency graph;
3. measured safe-area provenance lacks enforced app/UI version, device class,
   and locale scope;
4. promoted-master contracts lack mandatory per-stream PTS/gap/drift,
   drop/duplicate, full-decode, and content-sync evidence;
5. critical-region proof is not yet bound to per-tick geometry plus stage
   evidence;
6. media fixture validation still needs strict non-overlapping PTS/cadence and
   measured marker timestamps;
7. equivalent unreduced rational FPS values can receive different identities;
8. executable identity and atomic Windows create-in-job are not proved for the
   private FFmpeg/process-loopback spike.

Until those contracts and tests land, WP0 is implemented but **gate-open**, and
WP2/WP3 must remain blocked.

Reviewed artifacts:

- [`CAPTURE-PIPELINE-SPEC.md`](CAPTURE-PIPELINE-SPEC.md);
- [`CAPTURE-PIPELINE-IMPLEMENTATION-PLAN.md`](CAPTURE-PIPELINE-IMPLEMENTATION-PLAN.md);
- [`CAPTURE-PIPELINE-COMPARATIVE-ANALYSIS.md`](CAPTURE-PIPELINE-COMPARATIVE-ANALYSIS.md);
- current Runtime Automation prototype, template build/Release surface,
  `features/capture-stage-core`, and relevant Neotolis public contracts.

## 1. Review process

The first round used five independent read-only passes:

1. reusable architecture, ownership, template distribution, and durable state;
2. Windows audiovisual capture, OBS/FFmpeg, audio routing, media validation, and
   recovery;
3. routine manual recording and authored-reel workflow;
4. real-time/offline clock, tick, preview, and determinism semantics;
5. local engine/build/prototype feasibility and Release leakage.

Because the first round changed high-risk clock, process ownership, and release
contracts, architecture, media, and time reviewers ran a second convergence
pass. The time reviewer returned GO. Media and architecture each returned one
or two precise conditional fixes; those fixes are integrated:

- silence without declared activity is a warning, not an unconditional failure;
- WP5a now owns the stage-core contract freeze before WP5b C implementation;
- `capture stage list|describe` is explicitly delivered and tested.

No unresolved P0/P1 specification contradiction remains.

## 2. Major criticisms and resolutions

### 2.1 “Universal game audio” was an overclaim

Criticism:

OBS application audio is real, but not every process topology is isolatable.
Some games use helper processes, multiple instances, or incompatible audio
paths. FFmpeg `gdigrab` plus DirectShow also does not automatically mean
per-application or system-loopback audio.

Resolution:

- universality now means one Studio CLI and one truthful backend contract;
- each game declares window/process/application topology;
- every consumer passes an audio capability probe;
- helper/multi-process audio requires an explicit `application_set`;
- missing capability fails before countdown; no `game → system → none`
  downgrade exists.

### 2.2 Authored real-time shots had no executable clock contract

Criticism:

Sequential per-tick DevAPI calls cannot reliably place controls/actions on exact
60 Hz game ticks while OBS/FFmpeg records wall-clock media. Unpaced manual time
runs too quickly; normal RUN time and network jitter move events.

Resolution:

- Studio compiles and uploads a bounded per-tick transaction plan before start;
- capture-stage-core validates, commits, and arms it;
- the game host executes it on the frame thread with a 60 Hz monotonic paced
  manual clock using the existing public Neotolis APIs;
- no per-tick network round trip, skip, merge, or catch-up is allowed;
- receipts bind plan hash, generation, logical tick, engine frame, callbacks,
  and lateness;
- backend acknowledgement, preroll, tick 0, media time origin, postroll, and
  stop are ordered explicitly;
- target 30/60 FPS changes external sampling only, not plan ticks.

This keeps real-time V1 independent of a Neotolis change.

### 2.3 Exact execution was confused with semantic/media determinism

Criticism:

Applying the same event on the same tick does not prove equal game state, and
OBS/FFmpeg output is never byte/pixel deterministic.

Resolution:

```text
timeline_execution: exact_tick | failed | not_applicable
semantic_trace: measured | not_measured
media_reproducibility: none
```

Semantic comparison is `match | mismatch | not_measured` only after comparing
complete traces. The optional offline lane owns exact sampled frames and scoped
pixel comparison.

### 2.4 Simple recording depended on a later stage feature

Criticism:

The first plan launched a “capture build” that simultaneously required DevAPI
and capture-stage-core, while claiming `capture live` did not need them.

Resolution:

- `recording-native` is a production-like base target with exact client-size
  launch and stable window/process identity; it has no stage or DevAPI;
- `authored-capture` extends it later with capture-stage-core, DevAPI, and the
  60 Hz host executor;
- template capture config and migration land before `capture live`;
- attach never silently resizes an existing game; mismatch provides an explicit
  relaunch/target/`--resize` remediation.

### 2.5 OBS automation was under-specified and potentially invasive

Criticism:

“Dedicated OBS configuration” did not prove that Studio would avoid controlling
or mutating a user's active OBS process. Version checks alone also do not prove
source/audio capability.

Resolution:

- WP1 must select and prove an isolated Studio profile/collection or managed
  portable tree;
- Studio controls only a verified owned PID/executable/config root/token;
- a global lock, random IPv4 loopback port, authentication, secret redaction,
  and owned-process cleanup are mandatory;
- eligibility uses a pinned tested version range plus RPC/request/source and
  runtime audio probes;
- unknown future majors are unsupported until tested;
- unmanaged OBS attachment is forbidden.

### 2.6 Media validation promised more than `ffprobe` can prove

Criticism:

Stream metadata cannot prove perceptual synchronization, the correct sound, or
flash/impulse alignment.

Resolution:

Validation has three independent results:

1. structural streams/codecs/rates/track routing;
2. timestamp PTS/gaps/start offset/end drift;
3. decoded content sync for controlled flash/impulse fixtures, otherwise
   `not_measured`.

Silence fails only when the job declares a controlled activity expectation.
Combination audio policies require compatibility mix plus isolated primary and
microphone tracks; a backend without multi-track support is ineligible.

### 2.7 Master, delivery, catalog, and queue state were mixed

Criticism:

An immutable take could not also receive later exports. Mutable catalog and
resumable queue state lacked locking/recovery rules.

Resolution:

- Attempt → immutable MKV master take;
- Encode Attempt → separate immutable delivery artifact;
- default `capture live` creates both MKV and default MP4, but MP4 failure never
  invalidates the master;
- take directories are source of truth;
- catalog is a rebuildable versioned atomic index under a cross-process lock;
- queue definitions are immutable and each execution has a separate append-only
  versioned run journal;
- interrupted real-time resume creates a new attempt and never appends a master.

### 2.8 Routine UX still exposed internal complexity

Criticism:

The original draft centered jobs, offline rendering, and evidence rather than
the frequent task of recording a clip.

Resolution:

```text
capture live
capture shot preview <shot> --target vertical-social-1080p60
capture shot record <shot> --targets landscape-1080p60,vertical-social-1080p60,square-1080p60
```

Jobs and queues remain internal/generated. Preview preserves selected aspect and
shares shot/framing/plan identity with real-time record while using explicit
lower quality/range inputs. The default live command returns take id, MKV master,
and MP4 delivery.

### 2.9 Release and prototype risks were deferred too late

Criticism:

The template already contains legacy `--capture`/PPM surface in normal sources,
and the prototype paired capture/step path has a known wrong-tick defect.

Resolution:

- WP0 inventories legacy CLI/source/symbol/asset leakage;
- the prototype is explicitly deprecated and cannot serve as V1 evidence;
- useful generic DevAPI client changes get focused successor tests;
- legacy capture is removed or capture-only guarded before Release acceptance;
- native/Web Release scans cover old and new capture surfaces.

### 2.10 Optional offline rendering was blocking the practical product

Criticism:

Exact render-target capture, presented-frame identity, and deterministic PCM
were conflated with ordinary video recording.

Resolution:

- real-time audiovisual V1 requires no engine video or PCM stream;
- deterministic PCM is not V1;
- offline PNG frames remain WP9 with separate engine issue/PR and acceptance
  gates;
- failure to ship WP9 does not weaken the universal real-time recorder.

### 2.11 Platform-specific reels would duplicate the same media

Criticism:

Treating TikTok, YouTube Shorts, and Reels as separate capture targets would
duplicate recording and encoding while still failing to express the real
constraint: each platform overlays different UI on the same 9:16 frame.
Conversely, one guessed inset would become stale and TikTok explicitly documents
that safe zones vary with dimensions, caption length, add-ons, and layout.

Resolution:

- one `vertical-social-1080p60` master/delivery serves all three platforms;
- `universal-social-v1` includes only eligible standard-organic evidence for the
  complete worst-case caption/UI and LTR/RTL matrix; paid-ad masks live in
  separate `ad-*` policies and measured organic evidence is labeled measured;
- critical content is validated against the common intersection while
  background may use the full frame;
- preview can show the combined mask or each platform mask, but media is clean;
- ad-only anchors/cards/add-ons use explicit stricter variants and do not
  silently shrink the standard publishing target;
- full canonical source-record hashes, hash-only deduplication, and a total sort
  make policy identity independent of enumeration order;
- all four surfaces require separate LTR/RTL readiness rows with pinned,
  measurable caption/UI bounds; direction rows fold only after identical mask
  hashes are proven;
- empty or incomplete per-tick critical-region evidence cannot pass;
- source assets carry origin/license/redistribution status and non-redistributable
  originals never enter git;
- the same MP4 must pass separate pinned TikTok, Shorts, Instagram Reels, and
  Facebook Reels delivery constraint sets.

## 3. Competitive critique

The approved design is stronger than OBS or Xbox Game Bar in authored semantic
control, target/framing reuse, provenance, and immutable artifacts. It is
stronger than Unity Recorder, Unreal MRQ, or Godot Movie Maker for the specific
need to record authentic manual gameplay through the same product used for
scripted reels.

It is also more complex than every individual competitor because it combines
their strongest lanes. That complexity is acceptable only if the work remains
vertical:

1. first prove controlled game audio and process isolation;
2. then ship `capture live`;
3. only then add stage/tick choreography and batching;
4. leave exact offline frames last.

The design is currently behind all competitors in one decisive dimension: it
does not exist yet. The WP1 proof is therefore a product gate, not a research
formality.

## 4. Residual measured risks

These are not specification blockers; they are mandatory spike evidence:

1. OBS is not currently installed in the inspected environment. Isolation,
   application audio, track mapping, output discovery, and recovery must be
   proven on the pinned build.
2. The local FFmpeg build exposes `gdigrab` and DirectShow, but no universal
   application/system audio path was proven. FFmpeg claims remain narrow.
3. Neotolis computes frame `dt` before the game update callback. The template
   call-order fixture must prove that queued manual steps and tick transactions
   align exactly at the next positive-dt update.
4. Backend “recording active” acknowledgement may precede stable first packets;
   preroll and decoded flash/impulse detection must establish usable media zero.
5. Dense curve plans and long shots need a measured capacity/overhead limit.
6. The 1080p60/10-minute thresholds must pass on the designated reference
   machine before that profile is advertised as supported.
7. Optional offline exact capture still needs focused engine contracts for
   presented-frame identity and render-target readback.
8. Safe-zone assets are UI policy, not eternal geometry. WP0 must classify,
   license-check, pin, hash, normalize, and date complete standard-organic
   evidence for TikTok, YouTube, Instagram, and Facebook before the universal
   social target is advertised; future UI changes create a new policy version.

## 5. Recommendation

Start only:

- WP0 contract/fixture/prototype/release inventory;
- WP1 OBS and FFmpeg spikes;
- WP5a stage-core contract freeze may run in parallel after WP0.

Do not generalize the recorder, add game-specific recorder branches, edit the
engine submodule, or begin WP9 until the corresponding entry gates pass.

No remaining product question needs the lead for the standard publishing
surface. Backend compatibility ranges, performance tolerances, and safe-mask
geometry are measured engineering decisions governed by the approved acceptance
contract. Paid-ad overlay variants remain opt-in scope.
