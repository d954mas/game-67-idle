# Capture Stage Core

Status: approved specification; no reusable runtime implementation exists yet.

## Purpose

`capture-stage-core` is the planned dev-only L1 runtime module for controllable
game capture stages. It lets a game expose seeded, inspectable content to the
shared Studio recorder without putting game-specific characters, cameras,
scenes, or reel logic into reusable tooling.

The normative approved contract is
[`CAPTURE-PIPELINE-SPEC.md`](../../ai_studio/runtime_automation/CAPTURE-PIPELINE-SPEC.md).
The delivery sequence is
[`CAPTURE-PIPELINE-IMPLEMENTATION-PLAN.md`](../../ai_studio/runtime_automation/CAPTURE-PIPELINE-IMPLEMENTATION-PLAN.md).
Research and the
assessment of the current prototype are in
[`CAPTURE-PIPELINE-COMPARATIVE-ANALYSIS.md`](../../ai_studio/runtime_automation/CAPTURE-PIPELINE-COMPARATIVE-ANALYSIS.md).

## Public surface

No C or CLI surface is implemented yet. The proposed public surface is a
fixed-capacity stage registry with typed controls/actions and an optional strict
`game.capture.*` DevAPI adapter. Studio Runtime Automation consumes that
adapter; the runtime module never depends on Python, FFmpeg, or Studio tooling.

The exact proposed methods, data identities, ownership split, time model, and
acceptance requirements live in the Runtime Automation specification. The
independent review record is
[`CAPTURE-PIPELINE-REVIEW.md`](../../ai_studio/runtime_automation/CAPTURE-PIPELINE-REVIEW.md).

## Ownership

The planned split is:

- `features/capture-stage-core/`: byte-identical stage registry, validation,
  lifecycle state machine, bounded precompiled tick-plan executor, and optional
  DevAPI adapter;
- each game: stage catalog, content preparation, cameras, controls, actions,
  semantic state, shot timelines, and framing variants;
- `ai_studio/runtime_automation/`: process launch, manual-time orchestration,
  framebuffer capture, queues, immutable masters and exports, comparison, and
  encoding;
- Neotolis: public time, presentation, render-target, and readback primitives;
- `features/audio-core/`: any future deterministic offline mix source.

`capture-stage-core` is not a scene manager, replay system, renderer, camera system,
screen recorder, or video editor. A stage may use `scenes-core`, but the two
features remain independent.

## Validation

The current specification package is validated with:

```powershell
node features/validate_contracts.mjs
node --test features/validate_contracts.test.mjs
node ai_studio/core_harness/validation/doc_reference_check.mjs
```

Implementation validation will add focused registry/DevAPI tests, deterministic
timeline and capture-backend tests, exact-size template renders, an anonymized
second real-game consumer, and release-surface inspection as required by the
specification.

## Compatibility

Version `0.3.0` identifies the approved specification package, not an
implemented runtime API.

- PATCH clarifies the documents without changing a proposed observable
  contract.
- MINOR adds a backward-compatible specified or implemented capability.
- MAJOR changes stage identity, shot timing, runtime methods, artifact meaning,
  or another compatibility boundary after consumers exist.

Pre-implementation review may still replace the proposed API. Such a change
must update the specification and version together; no current game should
claim compatibility from the metadata alone.

## Extension points

Games extend the planned module through static stage descriptors, typed
callbacks, shot timelines, framing variants, and semantic diagnostics. Studio
tooling extends capture through versioned capture profiles, delivery presets, and
capture backends.

Game characters, domain actions, cameras, layout choices, and reel composition
remain outside the shared module. Suspected engine gaps require a reproduction
and root-cause classification before any Neotolis issue or PR.

## Current state

The existing recorder and private-game capture catalog are an exploratory
prototype. They are evidence for the contract, not the public surface of this
feature. WP0 is complete. The current WP1 attempt measured the available OBS
and FFmpeg paths, but neither passed the full video/audio/recovery exit gate, so
WP1 remains open and general backend work remains blocked. The failure is
recorded in the Runtime Automation WP1 report; it does not establish an engine
defect. The optional offline frame backend has additional engine
presentation/readback gates.
