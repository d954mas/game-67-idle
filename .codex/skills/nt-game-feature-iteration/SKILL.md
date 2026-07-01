---
name: nt-game-feature-iteration
description: "Use when implementing, changing, debugging, or validating a playable game feature: gameplay mechanics, controls, camera, UI flow, state, progression, balance, engine integration, prototype slice, build/run/debug tasks, release packaging, or CI for a current game."
---

# NT Game Feature Iteration

Use this skill as a thin router for small, verified playable increments.

## Start

1. Read the explicit `games/<game-id>/` folder for the current game.
2. Use `node ai_studio/taskboard/cli.mjs context --json` only when durable task
   state is needed.
3. Select one player-visible goal and one primary runtime/proof target.

## Routing

- For concept/GDD/source-of-truth changes, use `nt-primary-gdd`.
- For reference research or reusable design knowledge, use `nt-design-knowledge`.
- For quality checks, use `nt-quality-checks`.
- For runtime screenshots, DevAPI, native iteration helpers, or visual evidence,
  use `nt-runtime-automation`.

## Rules

- Make one coherent gameplay increment at a time.
- Prefer the active game's primary runtime. Do not change platform target unless
  the user asks or accepts the exception.
- Validate with evidence that matches the claim: test, run, screenshot,
  recording, log, or runtime report.
- Review player-facing changes as product quality, not only code correctness.
- Update durable game state only when it helps the next agent; keep scratch and
  failed experiments in `tmp/`.
