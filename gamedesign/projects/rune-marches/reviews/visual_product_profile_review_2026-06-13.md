---
type: Review
title: Rune Marches Visual/Product Failure Review
description: Blunt review of why the current native slice fails the requested visual, playability, and AI workflow bar.
tags: [review, visual, ux, profiling, native, rescue]
timestamp: 2026-06-13T00:00:00Z
---

# Rune Marches Visual/Product Failure Review

## Scope

Objective under review: make an original casual Skyrim-like open-world fantasy
RPG for PC first, later web/mobile, with strong visual direction, gameplay,
balance, FTUE, and a path to Poki-audience testing.

Evidence inspected:

- Native screenshots:
  `tmp/rune_marches/native_branch_landmark_labeled.png`
- Native portrait screenshot:
  `tmp/rune_marches/native_branch_landmark_portrait.png`
- Art direction:
  `gamedesign/projects/rune-marches/art/art_direction.md`
- Runtime implementation:
  `src/main.c`, `src/game_state_actions.c`,
  `state/game_state.schema.json`
- Passive profiling summary:
  `tmp/session_profiles/session_profile_2026-06-14.summary.md`
- Task context:
  `tasks/STATUS.md`, `tasks/active/T0003-native-first-playable-rpg-slice.md`

## Verdict

The current native slice is not acceptable as a casual Skyrim-like prototype.
It is a functional automation demo with generated art fragments, not a playable
product-quality first screen. It should not be expanded with more content until
the visual hierarchy and first-session UX are rebuilt.

## Product Findings

### P0: The Screenshot Does Not Communicate The Game

Symptom: The screen has many labels, buttons, meters, and cropped icons, but no
clear fantasy action. A player sees a busy map and a button grid, not a
confident RPG loop.

Cause: Work optimized for state coverage and DevAPI milestones instead of a
first-screen player read. The implementation kept adding route beats and
telemetry while the visual surface stayed unresolved.

Required correction: Freeze content expansion. Rebuild the first screen around
one goal, one primary action, clear status, and one readable map route.

### P0: Visual Assets Are Cropped Fake-Shot Parts, Not Runtime Art

Symptom: Landmarks and UI elements feel pasted together. Icons repeat, labels
fight the background, and the map looks assembled rather than designed.

Cause: The accepted fake shot was treated as a crop source. The art direction
explicitly said fake-shot crops were only a first pass, but runtime work kept
building on them.

Required correction: Create a proper runtime UI/map asset job: clean landmark
icons, panel frames, button states, resource icons, and a simplified map
background. Do not bake labels into reusable art.

### P0: FTUE Is Overloaded

Symptom: The first-session path now contains too many beats: Wispfen, side
choice, tower, Reedmere, shrine, Greenfen, level-up, Ward II, route choice,
route combat, branch landmark. Automation can run it, but a casual player will
not understand the hierarchy.

Cause: Automation milestones became the product definition. Passing a scenario
was mistaken for making the game playable.

Required correction: Split runtime content from first-session presentation.
Keep deeper state available, but first audience test should expose fewer
choices and a shorter loop.

### P1: UI Looks Like Debug Controls

Symptom: Bottom controls are large slabs with text labels; disabled combat
buttons stay visible in exploration; journal uses dense text and tiny stats.

Cause: The runtime reused the same button grid for all states instead of
separate exploration/combat affordances.

Required correction: Exploration screen should show one primary action and
small secondary actions. Combat screen can show Strike/Spark/Guard/Retreat.
Use icons and state-specific layouts.

### P1: Mobile Portrait Is Technically Non-overlapping But Not Good

Symptom: Portrait screenshot avoids major text overlap after fixes, but it
still has cramped map labels, duplicated controls, and poor hierarchy.

Cause: Portrait was treated as a layout constraint, not as a separate first-use
experience.

Required correction: Design portrait composition first: top status, map, one
big action, bottom mode/context buttons. Reduce persistent labels.

## Profiling Findings

### P0: Passive Profile Was Enabled Too Late To Explain The Failure

Evidence: `session_profile_2026-06-14.summary.md` contains 4 records, 8.8s
profiled duration, all validation. It does not cover the long period where
poor product decisions accumulated.

Cause: Profiling was used around commands, not around product decision gates.

Required correction: Add explicit checkpoints before and after visual/product
decisions, for example:

```text
node tools/ai.mjs checkpoint "Visual gate before more content"
node tools/ai.mjs checkpoint "Lead screenshot review before next feature"
```

### P1: Validation Bias Hid Product Failure

Evidence: Native build, DevAPI smoke, scenario, and playtest proxy passed while
the user-visible result was rejected.

Cause: Tests proved state transitions and screenshot capture, not product
quality. There was no visual acceptance checklist that could fail the build.

Required correction: Add a visual review gate for T0006 before any new content:
desktop screenshot, portrait screenshot, one-paragraph player-read audit, and
explicit pass/fail on visual hierarchy.

### P1: Art Job Tool Wrote Outside The Project Folder Map

Evidence: `tools/assets/new_art_job.mjs` was invoked for
`rune-marches-ui-map-rescue-v2` and initially wrote under
`gamedesign/rune-marches/...` instead of
`gamedesign/projects/rune-marches/...`.

Cause: The helper is generic and does not fully enforce this repository's
current project wiki folder map.

Required correction: For this rescue pass, inspect generated art job paths
before using them. Later, either fix the helper or document its path behavior.

## Process Failures

- The agent kept extending a technical vertical slice after the visual bar had
  already failed.
- The task status said visual direction existed, but the runtime evidence did
  not match that quality.
- The project accumulated too many systems for a first audience test before
  proving the core loop feels good.
- “Generated/free assets allowed” was interpreted as permission to integrate
  rough crops, not as a requirement to produce a polished runtime asset set.

## Immediate Rescue Plan

1. Stop native content expansion.
2. Create or update a runtime art job for a clean UI/map kit.
3. Rebuild first screen around one player-readable loop:
   `Scout Road -> Fight -> Reward -> Upgrade/Next Place`.
4. Hide or de-emphasize deep route content from first-screen FTUE.
5. Make separate exploration and combat layouts.
6. Capture desktop and portrait screenshots.
7. Run a visual pass/fail review before running long gameplay scenarios.

Active art job:

- `gamedesign/projects/rune-marches/art_requests/rune-marches-ui-map-rescue-v2.json`
- `gamedesign/projects/rune-marches/data/rune-marches-ui-map-rescue-v2-crop_manifest.json`
- `gamedesign/projects/rune-marches/data/rune-marches-ui-map-rescue-v2-asset_manifest.json`

## Success Bar For The Next Pass

The screenshot must answer these questions without explanation:

- Where am I?
- What is my goal?
- What should I click first?
- What changed after the action?
- Why do I want to continue?

If the screenshot cannot answer those, scenario tests are not enough.
