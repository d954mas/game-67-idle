---
name: game-feature-iteration
description: Use when implementing, changing, debugging, or validating a playable game feature in an existing game repository, or when discovering, adding, fixing, or running build, launch, debug, release, package, or CI tasks. Triggers include gameplay mechanics, controls, cameras, UI flows, game state, progression, balance-affecting code, engine integration, requests to make a small playable prototype or vertical slice, VS Code tasks, CMake presets, build scripts, launch configurations, release outputs, serving web builds, packaging, and explaining how to run or distribute the game. Works across engines by discovering local build/run/test conventions first.
---

# Game Feature Iteration

Use this skill to make small, playable game changes without losing project
context.

## Load Only What Applies

- `references/iteration-cycle-playbook.md`: non-trivial iteration loop, task
  packet, evidence, review, state update, and report format.
- `references/playable-feature-gates.md`: reference deconstruction, screen
  grammar, first-60-seconds, Definition of Ready, Reference Intake, Reference
  Digest, Source Ladder, Reference Evidence Board, Parallel reference work,
  visual/product gates, 5-line session contract, mismatch list, core-moment
  feel, Build, Launch, And Release Tasks, slice hygiene, 30 changed files,
  promise push, stale fail audits.

## Minimal Workflow

1. Read local source of truth first: `AGENTS.md`, compact current context, active
   project state/runbooks, relevant design docs, build presets, and nearby code.
   Prefer `node tools/ai.mjs context`; if absent, use
   `tools/game_context/iteration_context.mjs`.
2. Select one task scope. For non-trivial playable, visual, pipeline, or tooling
   work, set passive profiling scope with
   `node tools/ai.mjs start <task-id> <iteration>` or state profiling is
   unavailable/off.
3. State the selected runtime harness before implementation and why local rules
   allow it.
4. If a named reference drives the work, load `playable-feature-gates.md` and
   verify the reference deconstruction is ready before coding.
5. Identify the smallest playable slice that satisfies the request.
6. For visual, FTUE, feel, audience, or casual-product work, load the product
   gate section and inspect/capture the first playable screen before broadening
   scope. Use `product_gate/review.mjs` or `node tools/ai.mjs gate`; use
   `close-slice` for handoff evidence when available.
7. For non-trivial implementation, use `iteration-cycle-playbook.md`.
8. Keep implementation close to existing engine/game patterns. Avoid broad
   refactors unless the feature cannot be implemented safely without them.
9. Validate the primary runtime target first; validate secondary targets only
   when relevant or requested. Default to native desktop when the project does
   not define another primary target.
10. Capture evidence and report what changed, how to run it, and what was
    verified.
11. Before committing or handing off a prototype slice, use the slice hygiene
    gate from `playable-feature-gates.md`.

## Discovery

Prefer local source-of-truth files over assumptions:

- Project rules: `AGENTS.md`
- Design docs: `gamedesign/`, `docs/design/`, `GDD.md`, or equivalent
- Build/run: `CMakePresets.json`, `.vscode/tasks.json`, package scripts,
  engine docs
- Existing examples: `examples/`, `samples/`, nearby features, engine submodules

If naming differs, infer equivalent directories from the repository.

## Always-On Rules

- Treat the local primary runtime as the platform gate; do not pivot to web for
  playable work without explicit user request or approval.
- Make one coherent gameplay increment at a time.
- Preserve engine/submodule/vendor boundaries unless explicitly requested.
- Keep reusable workflow in skills and project-specific facts/run commands in
  project docs.
- Product gate fail blocks feature/content expansion unless the lead explicitly
  accepts the debt.
- Keep product/readability, game-loop/fun, art-source/assets, and
  technical/build gates separate.
