---
name: game-feature-iteration
description: "Use when implementing, changing, debugging, or validating a playable game feature: gameplay mechanics, controls, camera, UI flow, game state, progression, balance, engine integration, prototype or vertical slice, build/run/debug tasks, CMake presets, release, packaging, or CI."
---

# Game Feature Iteration

Use as a thin router for small, verified playable increments.

## Load Only What Applies

- `references/iteration-cycle-playbook.md`: iteration loop, task packet,
  evidence, review, state update, anti-patterns, and report format.
- `references/playable-feature-gates.md`: reference deconstruction,
  first-60-seconds, visual/product gates, build/release, slice hygiene, promise
  push, stale fail audits.

## Router Workflow

1. Read `AGENTS.md`, `node tools/game_context/iteration_context.mjs`, relevant
   docs, and nearby code.
2. Select one task scope and primary runtime harness.
3. For non-trivial work, load `iteration-cycle-playbook.md`; implement the
   smallest playable slice, validate primary target, capture evidence, commit.
4. For named references, clarity, visual/product feel, native desktop,
   build/release, or handoff, load `playable-feature-gates.md` before coding.

## Always-On Rules

- Treat the local primary runtime as the platform gate.
- Do not pivot playable work to web without explicit user request or approval.
- Make one coherent gameplay increment at a time and avoid unrelated refactors.
- Preserve engine/submodule/vendor boundaries unless explicitly requested.
- Product gate fail blocks feature/content expansion unless lead accepts debt.
- `lead-rejection` tasks need `node tools/ai.mjs close-slice --resolved-rejection
  "<exact rejected issue and proof>"` for strict closeout.
- Keep product, game-loop, art-source, and technical gates separate.
