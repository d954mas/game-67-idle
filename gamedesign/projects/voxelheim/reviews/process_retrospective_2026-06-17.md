---
type: Process Retrospective
title: Voxelheim Development Process Retrospective 2026-06-17
description: Blunt review of the Voxelheim rescue development process, evidence quality, failures, and pipeline fixes.
tags: [voxelheim, process, retrospective, pipeline, ui, product-gate]
checked: 2026-06-17
---

# Voxelheim Development Process Retrospective

## Scope

Session scope: the Voxelheim rescue loop from "generic idle RPG" toward
**Frost Keep Rebuilder**, including competitor/product deconstruction, GDD and
economy rewrite, native runtime sync, UI rescue, reward feedback, and the later
live UI regression around CTA text and purple button edges.

Objective reviewed: make the prototype into a successful game by researching,
comparing, designing, building, improving, and reviewing with critics/players.

## Evidence Inspected

- Project rules: `AGENTS.md`, `AI_PIPELINE.md`, `tasks/README.md`.
- Taskboard: `node tools/taskboard/cli.mjs summary`.
- Profiler:
  - `node tools/ai.mjs status`
  - `node tools/ai.mjs status --verbose`
  - `node tools/ai.mjs status --require-current-scope-usable`
  - `node tools/ai.mjs import-codex-session`
- Current changed files: `git status --short`.
- Active task logs:
  - `tasks/active/T0001-first-native-playable-slice-for-voxelheim.md`
  - `tasks/active/T0005-design-the-real-voxelheim-game-refs-hook-core-lo.md`
  - `tasks/active/T0009-profiler-coverage-guard-exclude-idle-gaps-1h-aut.md`
- Product/design evidence:
  - `gamedesign/projects/voxelheim/reviews/prototype_deconstruction_2026-06-17.md`
  - `gamedesign/projects/voxelheim/references/competitor_deconstruction_2026-06-17.md`
  - `gamedesign/projects/voxelheim/reviews/live_ui_regression_2026-06-17.md`
  - `gamedesign/projects/voxelheim/visual/ui_ux_rescue_spec.md`

## Evidence Quality

Profiler evidence is **not trustworthy as a complete review source**.

Observed profiler status:

- profile exists and is valid JSONL;
- records after import: 210;
- current scope: `true/T0005`;
- work-item coverage: 2.4%;
- unresolved failures: 5;
- review confidence: broken;
- current-scope review confidence: broken;
- guard: `node tools/ai.mjs status --require-current-scope-usable` fails.

Therefore this retrospective uses the profiler only as a friction signal:
repeated commands, unresolved failed records, and coverage gaps. It does not use
the profiler to make strong time-spend claims.

## What Was Done

The work did move the prototype materially:

- The failed generic idle loop was deconstructed and redirected into **Frost
  Keep Rebuilder**.
- Current competitor/product references were gathered strongly enough to justify
  the design pivot, though not strongly enough for exact final art/economy.
- The GDD, concept, implementation plan, balance data, and rescue-loop data were
  rewritten around visible keep rebuilding.
- Native runtime gained Frost Blocks, Gate/Forge/Campfire repairs, rune-card
  choice, Keep Rank, Avalanche Reset, Frost Shards, Frost Blueprints, offline
  return, reward feedback, audio cues, and UI rescue passes.
- Multiple screenshots, readability montages, focused probes, and product gates
  were produced.
- After lead feedback, a live UI regression review identified the missed state:
  post-offline combat with Blueprints visible, Gate CTA affordable, floaters
  active, and HUD resources visible.

This is real progress, but it was not a clean success process.

## Where The Most Effort Went

Strong evidence:

- The profiler shows repeated build/check friction:
  - `cmake`: 28 runs.
  - `node ai.mjs`: 22 runs, 2 failed.
  - `py ui_readability.py`: 15 runs.
  - `py pixel_health.py`: 15 runs.
  - `py offline_return_probe.py`: 12 runs.
- Task logs show many small runtime/product-gate slices: repair chain,
  Avalanche Reset, Frost Blueprints, offline return, UI rescue, right rail,
  reward feedback, and live UI hotfix.

Weak evidence:

- Exact time allocation is unknown because profile confidence is broken.
- The largest profiler gaps are 5-8 minutes, but unresolved failure records mean
  the profile cannot support a complete time audit.

## Where The Agent Got Stuck Or Behaved Poorly

### 1. Passing the wrong state

Symptom:

The reward-feedback gate passed on an offline/reward screenshot, then the lead
showed a live screen where CTA text, floaters, right rail, HUD icons, and purple
button edges were still bad.

Cause:

The product gate was treated as if one screenshot covered the UI system. It only
covered the captured state. There was no required state matrix.

Faster path:

Before calling any UI slice pass, enumerate required player states:
fresh run, CTA affordable, modal choice, meta panel, offline popup, post-offline
live combat, and stress frames with floaters active. A pass can only cover the
states it captured.

### 2. Fixing symptoms without locking source assets

Symptom:

The purple edge returned on the green CTA button after it had supposedly been
fixed.

Cause:

The durable source PNG still had chroma/magenta contamination, and the packer
builds from `assets/raw/*.png`. The earlier check did not audit the source asset
and runtime crop together.

Faster path:

Any generated/chroma-key UI asset needs a source-to-runtime gate:
source PNG audit, pack rebuild, native screenshot crop, and edge pixel audit.

### 3. Overclaiming pass evidence

Symptom:

Several task logs record product gate PASS entries, while later user feedback
found the screen still ugly, unclear, or unreadable in another state.

Cause:

The gate result was logged without a clear statement of scope limits. "PASS
(desktop_reward_feedback)" sounded broader than "this one offline reward state
passed."

Faster path:

Every gate log line should include `covered states` and `not covered states`.
If not covered, it is debt, not implied pass.

### 4. Source-of-truth drift

Symptom:

`AGENTS.md` contains both new Frost Keep Rebuilder direction and old generic
Voxelheim/Game Seed current-surface statements in the same active sections.

Cause:

Rules were appended during rescue instead of replacing obsolete active truth.
This keeps old instructions alive and makes future agents vulnerable to picking
the wrong runtime or design source.

Faster path:

After a direction pivot, run a source-of-truth cleanup pass before the next
implementation cycle. Current docs should have one active concept, one active
runtime, one active gate.

### 5. Critic timing was late

Symptom:

Subagent/UI critic feedback was requested after the lead had already rejected
the screenshot, not before the pass was accepted.

Cause:

The main agent tried to self-review too much from local gates and screenshots.
The separate critic was not part of the acceptance loop.

Faster path:

For UI/product passes, run critic review before product-gate PASS when the
screen has any known density/readability risk.

### 6. The process created many gates but not enough gate composition

Symptom:

There are visual gates, readability gates, product gates, reference gates, and
profiling guards, but they did not prevent a narrow state pass from being
overgeneralized.

Cause:

Each gate answered its own small question. No orchestrator asked, "which states
does this proof cover, and which user complaints remain untested?"

Faster path:

Use a single slice closeout checklist:
required states, required player-read answers, required asset audits, known red
gates, and explicit uncovered debt.

## Tool And Automation Audit

What worked:

- Native PC harness and DevAPI probes made deterministic screenshots possible.
- `ui_readability.py --compare` caught regressions that full screenshots hid.
- Focused probes such as `ui_text_overlap_probe.py` were better than generic
  screenshots because they created the exact failed state.
- `audit_cta_purple.py` turned a subjective "purple edge" complaint into a
  source/runtime pixel check.

What failed:

- `node tools/ai.mjs status --require-current-scope-usable` currently fails
  during the retrospective, so profiler cannot support a strong time audit.
- Product gate lacks state coverage semantics.
- Readability metrics can warn on visually acceptable text or non-text regions;
  the zoom montage remains necessary.
- Temporary probes are useful but not yet promoted into a reusable acceptance
  matrix.

## Context Problems

- The agent loaded and modified a lot of project state across design, runtime,
  tasks, reviews, and tooling. The session was large enough that compaction risk
  became real.
- Some rules are duplicated across `AGENTS.md`, `AI_PIPELINE.md`, skills, and
  project docs. This makes it easy to obey a stale line.
- `tasks/STATUS.md` is still compact enough to use, but it now needs to reflect
  that the game iteration has stopped and process review tasks are active.

## Planning Problems

- The iteration became a chain of adjacent fixes: loop -> meta -> offline ->
  UI -> reward -> hotfix. Each step was reasonable, but the acceptance boundary
  shifted after every pass.
- The process did not force an explicit "state matrix" before broad UI claims.
- "Game done" was treated as moving through gates, not as a full coverage audit
  against lead complaints and all visible states.
- The lead's qualitative feedback should have triggered a stricter critic pass
  earlier.

## Product / Result Quality Problems

- The game is now much closer to a product than the failed prototype, but it is
  still not proven successful.
- Final room art remains placeholder-level.
- CTA readability still had an automated warning after the hotfix, even though
  the zoom was readable.
- The broader 5-minute fun/retention critic pass remains incomplete.
- The current UI evidence is state-specific, not a complete UI system proof.

## Improved Workflow For The Next Cycle

1. Start with one active source of truth: concept, runtime, gate, current task.
2. Before implementation, write a state acceptance matrix for the slice.
3. For UI/visual work, capture the current bad state first and name mismatches.
4. Make the smallest runtime change that closes one named mismatch.
5. Rebuild and capture the exact same state.
6. Run zoom/readability and source-to-runtime asset audits where applicable.
7. Run a critic before recording PASS for high-risk UI.
8. Product gate records covered states and uncovered debt.
9. Task log uses "PASS for state X", not generic "PASS".
10. Only after state matrix passes should feature/content expansion resume.

## Prompt / Rule Changes

Recommended process changes:

- Add a state-coverage requirement to product gates.
- Add a Voxelheim-specific live-state UI acceptance matrix.
- Clean `AGENTS.md` so obsolete active direction is removed, not merely
  contradicted later.
- Promote purple/chroma edge audits into reusable generated UI asset validation.
- Make profiler status explain current unresolved records and recovered status
  more directly, or let the retrospective mark known resolved failures without
  editing raw history.

## Top 10 Improvements

1. Implement T0013: reusable live-state UI acceptance matrix, with Voxelheim as
   the first fixture.
2. Implement T0012: product gate state-coverage tags.
3. Implement T0011: clean active source-of-truth drift.
4. Convert `tmp/ui_text_overlap_probe.py` into a named Voxelheim UI acceptance
   probe or matrix scenario.
5. Convert `tmp/audit_cta_purple.py` into a reusable generated UI edge audit.
6. Require critic review before UI PASS when the screenshot has dense HUD,
   modal, right rail, and transient effects together.
7. Change gate log phrasing to "PASS for covered states" and include explicit
   not-covered debt.
8. Make taskboard/status distinguish "game iteration stopped" from "game done".
9. Run the broader 5-minute fun/retention critic before any future success
   claim.
10. Keep profiler evidence partial unless `--require-current-scope-usable`
    passes or unresolved failed records are explicitly explained.
