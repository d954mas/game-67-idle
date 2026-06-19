---
name: game-feature-iteration
description: Use when implementing, changing, debugging, or validating a playable game feature in an existing game repository, or when discovering, adding, fixing, or running build, launch, debug, release, package, or CI tasks. Triggers include gameplay mechanics, controls, cameras, UI flows, game state, progression, balance-affecting code, engine integration, requests to make a small playable prototype or vertical slice, VS Code tasks, CMake presets, build scripts, launch configurations, release outputs, serving web builds, packaging, and explaining how to run or distribute the game. Works across engines by discovering local build/run/test conventions first.
---

# Game Feature Iteration

Use this skill as a thin router for small, verified playable increments. Keep
project-specific state in project docs and load detailed references only when
their trigger applies.

## Load Only What Applies

- `references/iteration-cycle-playbook.md`: non-trivial iteration loop, task
  packet, evidence, review, state update, and report format.
- `references/playable-feature-gates.md`: reference deconstruction, screen
  grammar, first-60-seconds, Definition of Ready, Reference Intake, Reference
  Digest, Source Ladder, Reference Evidence Board, Parallel reference work,
  visual/product gates, 5-line session contract, mismatch list, core-moment
  feel, Build, Launch, And Release Tasks, slice hygiene, 30 changed files,
  promise push, stale fail audits.

## Router Workflow

1. Read local source of truth first: `AGENTS.md`, compact context, active
   state/runbooks, relevant design docs, build presets, and nearby code. Prefer
   `node tools/ai.mjs context`; otherwise use
   `tools/game_context/iteration_context.mjs`.
2. Select one task scope and the primary runtime harness. For non-trivial
   playable, visual, pipeline, or tooling work, set passive profiling scope or
   state why profiling is unavailable.
3. If the task is non-trivial, load `iteration-cycle-playbook.md`; implement the
   smallest playable slice, validate the primary target first, capture evidence,
   update durable state only when useful, then commit intentional files.
4. If the work touches named references, first-player clarity, visual/product
   feel, build/release tasks, or prototype handoff, load
   `playable-feature-gates.md` before coding and follow its reference
   deconstruction, product gate, native desktop, `product_gate/review.mjs`,
   `node tools/ai.mjs gate`, `close-slice`, and slice hygiene rules.

## Discovery

Prefer local source-of-truth files over assumptions:

- Project rules: `AGENTS.md`
- Design docs: `gamedesign/`, `docs/design/`, `GDD.md`, or equivalent
- Build/run: `CMakePresets.json`, `.vscode/tasks.json`, package scripts,
  engine docs
- Existing examples: `examples/`, `samples/`, nearby features, engine submodules

If naming differs, infer equivalent directories from the repository.

## Always-On Rules

- Treat the local primary runtime as the platform gate; default to native desktop
  unless the project defines another target.
- Do not pivot playable work to web without explicit user request or approval.
- Make one coherent gameplay increment at a time and avoid unrelated refactors.
- Preserve engine/submodule/vendor boundaries unless explicitly requested.
- Product gate fail blocks feature/content expansion unless the lead explicitly
  accepts the debt.
- Keep product/readability, game-loop/fun, art-source/assets, and
  technical/build gates separate.
