---
name: game-feature-iteration
description: Use when implementing, changing, debugging, or validating a playable game feature, gameplay mechanics, controls, camera, UI flow, game state, progression, balance, engine integration, small prototype or vertical slice, build/run/debug tasks, CMake presets, release outputs, packaging, or CI. Discover local build/run/test conventions first.
---

# Game Feature Iteration

Use this skill as a thin router for small, verified playable increments. Keep
project state in project docs; load references only when their trigger applies.

## Load Only What Applies

- `references/iteration-cycle-playbook.md`: non-trivial iteration loop, task
  packet, evidence, review, state update, report format.
- `references/playable-feature-gates.md`: reference deconstruction, screen
  grammar, first-60-seconds, visual/product gates, Build/Launch/Release Tasks,
  slice hygiene, 30 changed files, promise push, stale fail audits.

## Router Workflow

1. Read local source of truth: `AGENTS.md`, `node tools/ai.mjs context` or
   `tools/game_context/iteration_context.mjs`, relevant design/build files, and
   nearby code.
2. Select one task scope and primary runtime harness. Set passive profiling for
   non-trivial playable/visual/pipeline/tooling work or state why unavailable.
3. For non-trivial work, load `iteration-cycle-playbook.md`; implement the
   smallest playable slice, validate primary target first, capture evidence, and
   commit intentional files.
4. For named references, first-player clarity, visual/product feel,
   build/release tasks, or prototype handoff, load `playable-feature-gates.md`
   before coding and follow product gate, native desktop,
   `product_gate/review.mjs`, `node tools/ai.mjs gate`, `close-slice`, and
   slice hygiene rules.

## Discovery

Prefer local source-of-truth files:

- Project rules: `AGENTS.md`
- Design docs: `gamedesign/`, `docs/design/`, `GDD.md`, or equivalent
- Build/run: `CMakePresets.json`, `.vscode/tasks.json`, package scripts,
  engine docs
- Existing examples: `examples/`, `samples/`, nearby features, engine submodules

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
