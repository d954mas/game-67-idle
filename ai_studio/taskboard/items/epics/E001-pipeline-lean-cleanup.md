---
id: E001
title: Pipeline lean cleanup
status: done
project: P001
priority: P2
tags: []
created: 2026-06-19
updated: 2026-07-14
---

## Goal

Finish the lean cleanup of the reusable AI pipeline: subtract redundancy that
accreted iteratively, and close the two craft gaps the agentic-gamedev field
solved mechanically - self-attested verification and no structural style-lock -
with the lightest touch, without importing rosters, phases, or autonomous
content. Green checks should mean a real artifact exists.

## In scope

- Verification hardening: evidence-bound close-slice (T0001), opt-in adversarial
  verifier (T0002), parseable gate verdict + sharper repeated-failure guard (T0003).
- Skills honesty + routing: honest skills presence check (T0004), art-cluster
  disambiguation (T0005).
- Validator/doc hardening: non-.md reference checks (T0006), constitution dedup
  (T0007), config-protection hook (T0008).
- Structural: dormant asset/product-gate extraction gated on an active game (T0009).
- Deferred to post-prototype game: soft asset style-consistency (T0010).

## Out of scope

- Large agent rosters, spec/phase pipelines, document zoos.
- Prompt-to-game / neural runtimes, LoRA training, multi-engine export.
- Game content/runtime work (no active game concept).

## Log

- 2026-06-19: epic opened after deep pipeline review + agentic-gamedev competitor
  study. Profiler collapse + reset/gate-map dedup already landed (commit 97c0169).
- 2026-06-19: executed. T0001-T0006 + T0009 DONE and committed (close_slice
  arbiter, --verify, parseable verdict + total guard, honest skill presence check,
  art-cluster disambiguation, non-.md doc-ref checks, dormant product-gate gate).
  T0008 closed-with-reason (per-call node hook overhead vs the C fast-path). T0007 DEFERRED
  (lead's live AGENTS.md concept edits + native-PC invariant now concept-variable).
  T0010 deferred (post-prototype style-lock). Net: 7/9 done, 2 closed-with-reason.
- 2026-07-14: Closed after full AI Studio refactor follow-up, test optimization, architecture cleanup, and Taskboard grooming; no live child tasks remain.
