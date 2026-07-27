# Agent-ready game capture

Status: normative V1 workflow.

At Studio commit `3cce0ba65`, the isolated OBS recorder is implemented and
validated. The agent workflow described here is the only supported product
surface being added on top of it. The older jobs, queues, backend-selection,
offline-render, and multi-target design is historical research, not an
implementation contract.

## User surface

There are exactly two routine commands:

```text
capture live
capture shot <id>
```

`capture live` records normal play after a countdown. `capture shot <id>`
launches one approved, game-owned deterministic scenario and records it.
Repository-local launch shims may be needed to put `capture` on a shell path;
they do not add commands or expose recorder internals.

There is no user-facing backend selector, recording job, queue, render command,
target fan-out, stage editor, or backend doctor. The existing
`record_game.py` implementation remains the single recording path:

- isolated portable OBS Window Capture (BitBlt);
- NVIDIA NVENC;
- Windows process-loopback audio for the selected game process;
- validated H.264/AAC media;
- no access to the user's normal OBS profiles, scenes, or audio devices.

## Ownership

Runtime Automation owns:

- the two-command orchestration;
- launch/attach, countdown, and OBS-recorder invocation;
- deterministic scenario playback over the live game DevAPI;
- media validation, safe-area evaluation, draft/master classification, and
  local E2E proof.

Each game owns:

- `capture/catalog.json`, the compact approved shot catalog;
- the referenced deterministic scenario documents;
- the game DevAPI scene commands used by those scenarios;
- semantic shot purpose, duration, framing, and critical-content regions.

The recorder continues to own OBS setup, window capture, process audio, media
encoding, pixel-health checks, and technical inspection. Games never own OBS
configuration or encoding code. No change is made under
`external/neotolis-engine`.

## Approved shot catalog

The catalog is intentionally small and reviewable. Every entry contains:

- stable `id`;
- human-readable `purpose`;
- exact `duration_seconds`;
- concise `angle`;
- one relative deterministic `scenario` path;
- one `preset` (`social`, `landscape`, or `square`);
- normalized critical-content rectangles when the social safe-area policy
  applies.

The scenario duration and output FPS must equal the catalog duration. Scenario
game id, scene id, seed, contract version, events, and ramps remain in the
game-owned scenario document. Unknown catalog fields, duplicate ids, paths
outside the game, mismatched duration, and missing scenarios are hard errors.

## Live workflow

`capture live`:

1. resolves the game's normal DevAPI capture executable;
2. launches a fresh game or attaches only when explicitly configured by the
   game shim;
3. starts the unchanged OBS recorder after its normal countdown;
4. records game-window pixels and game-process audio;
5. validates the decoded video, pixel health, audio stream, audio activity, and
   OBS diagnostics;
6. publishes a draft take.

Live play cannot provide complete critical-content geometry, so its universal
safe-area result is `guidance_only`. A live take is never automatically called
a master.

## Shot workflow

`capture shot <id>`:

1. resolves the id from the game catalog and validates its scenario;
2. launches the game with DevAPI enabled;
3. discovers endpoints and describes the unfamiliar game capture-scene
   commands;
4. loads the declared seeded scene, switches to manual time, and completes the
   declared warmup;
5. starts the unchanged OBS recorder;
6. while OBS and process audio are active, applies scenario events before their
   fixed tick, steps the declared tick count, and host-paces output frames;
7. validates the resulting audiovisual media;
8. evaluates declared critical-content rectangles against the current
   universal-social safe-area guide and policy readiness;
9. classifies and publishes the take.

The schedule is deterministic game choreography. OBS media is a real-time
observation and is not claimed pixel- or timestamp-deterministic.

## Universal safe areas

`vertical-social-1080p60` references `universal-social-v1`. Its required matrix
is TikTok feed, YouTube Shorts, Instagram Reels, and Facebook Reels in both LTR
and RTL layouts.

The checked-in matrix currently has no complete eligible standard-organic
source geometry. Its truthful readiness is therefore `incomplete`. The Studio
may show a conservative guide and may report whether declared regions fit that
guide, but it must not report `universal_safe=pass` or promote a social take to
master until:

- every required matrix row has eligible, reviewed source geometry;
- the derived policy status is `official` or `measured`;
- every declared critical region is measured for its complete interval;
- no region intersects the derived unsafe mask.

This gate applies to shot takes. Live takes always report `guidance_only`.
Guides are evidence overlays only and are never burned into recorded media.

## Draft and master

A successful OBS recording is a **draft take** first. Passing codec, duration,
pixel-health, and audio checks is necessary but not sufficient for master
status.

Draft artifacts live under:

```text
tmp/captures/<mode-or-shot>/<take-id>/draft/
  recording.mkv
  edit.mp4
  capture.json
  representative-frame.png
```

A **master take** is a separately published immutable directory:

```text
tmp/captures/<shot>/<take-id>/master/
  recording.mkv
  edit.mp4
  capture.json
  representative-frame.png
```

Master promotion requires all of:

- scripted shot mode;
- successful recorder/media validation;
- completed scenario playback with no DevAPI or pacing failure;
- applicable safe-area policy ready and passing;
- matching scenario/catalog identity and duration.

Promotion copies the validated draft into a new master directory and never
renames an unchecked draft to make it appear accepted. Failed or incomplete
safe-area evidence leaves the media in `draft/` with an explicit reason.

The recorder's internal `master.mkv` filename is an implementation detail inside
its temporary work directory. The agent workflow never exposes that file as a
product master before the promotion gate.

## Local E2E proof

The game provides one reproducible local E2E command that records an approved
short shot and asserts:

- the command exits successfully;
- a representative decoded PNG exists and passes pixel health;
- the MKV contains a decoded video stream at the requested dimensions/FPS;
- the MKV contains 48 kHz stereo AAC game-process audio;
- measured audio activity is present;
- the workflow manifest reports the truthful draft/master and safe-area state.

The E2E output stays ignored under the game `tmp/` tree. It is runtime evidence,
not a committed binary fixture.

## Current implementation status

| Capability | Status at `3cce0ba65` |
| --- | --- |
| Isolated OBS window recording | implemented |
| Game-process loopback audio | implemented |
| Video/audio/pixel validation | implemented |
| `capture live` orchestration | pending this workflow change |
| `capture shot <id>` orchestration | pending this workflow change |
| Game-owned approved catalog | pending this workflow change |
| Universal safe-area gate | requirements implemented; source matrix incomplete |
| Draft/master product separation | pending this workflow change |
| Reproducible local audiovisual E2E | pending this workflow change |

The implementation updates this table when those pending items land. Historical
spike reports remain evidence for why the fixed OBS path was selected; they do
not reopen backend selection.

## Non-goals

- recording backend comparison or selection;
- jobs, queues, retry journals, or backend state machines;
- offline frame rendering;
- arbitrary input replay;
- automatic editing, captions, publishing, or platform upload;
- automatic reframing between aspect ratios;
- user OBS profile or scene management;
- engine changes.
