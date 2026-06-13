---
id: T0101
title: Add one-command AI reflection prep
status: review
epic: ""
priority: P1
tags: [pipeline, profiling, reflection, automation, tools]
created: 2026-06-13
updated: 2026-06-13
---

## What

Add one command that prepares the current AI profile for reflection by running
only the stale/missing steps in the closeout -> baseline comparison ->
reflection packet -> reflection draft chain.

## Done when

- [x] `tools/ai_profile/prepare_reflection.mjs` refreshes stale/missing
      closeout bundles, baseline comparisons, reflection packets, and
      reflection drafts.
- [x] The command refuses to auto-capture baselines and stops on current-scope
      comparison regressions unless explicitly allowed.
- [x] The command writes optional final status JSON and prints the draft path
      when ready.
- [x] Regression tests cover fresh no-op, missing packet/draft generation, and
      regression refusal.
- [x] Profiling docs/reflection skill tell agents to use prepare command before
      manual handoff commands.

## Open questions

## Log

- 2026-06-13: Status now reports packet/draft freshness, but agents still have
  to manually run closeout, compare, packet, and draft commands in the right
  order. A one-command prep wrapper can reduce routine reflection handoff time
  while keeping baseline capture and regression judgment explicit.
- 2026-06-13: Implemented `prepare_reflection.mjs`, added regression coverage
  for no-op/missing-artifact/regression paths, documented it as the normal
  retrospective handoff entrypoint, and synced generated skills.
