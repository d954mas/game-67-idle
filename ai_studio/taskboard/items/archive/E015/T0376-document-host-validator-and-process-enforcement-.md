---
id: T0376
title: Document host validator and process enforcement boundaries
status: done
project: P001
epic: E015
priority: P1
tags: [agents, contracts]
created: 2026-07-10
updated: 2026-07-11
---

## What

Make agent/tool rules honest about where enforcement actually lives so logs do
not imply that a process convention was technically checked.

## Done when

- [x] Canonical agent/workflow contracts label each material rule as
      host-enforced, repository-validator-enforced, or process convention.
- [x] Each host/validator-enforced claim links to the actual configuration,
      command, or test that proves it.
- [x] Logs and quality reports distinguish observed enforcement from advisory
      instructions and do not claim a model/router mechanism the repo lacks.
- [x] After harness restart, a smoke procedure verifies requested role and
      actual model; generic fallback is reported as failure.
- [x] Wording stays short and routes to owning docs instead of duplicating all
      role/provider formats.

## Open questions

## Log

- 2026-07-10: The lead chose not to refactor different Codex/Claude role
  catalogs. This task documents/enforces the real boundary only.
- 2026-07-11: Checkpoint: canonical AGENTS/workflow/orchestration contracts currently state material rules without enforcement labels or evidence links, and no repository smoke procedure verifies requested agent_role against the actual selected model after restart. Existing profiling is advisory only. Starting a short routed enforcement contract plus proof/smoke tooling without refactoring Codex/Claude role catalogs or inventing a model router.
- 2026-07-11: Added one routed enforcement contract with explicit host, repository-validator, and process labels. Host proof_kind now distinguishes runtime-observed evidence from configuration-only responsibility; configuration-only claims state that runtime application is not repository-verified.
- 2026-07-11: Added enforcement and native Codex role/model validators with focused tests. The role smoke accepts only canonical nested spawn metadata from a rollout whose path, filename, and id match the native Codex sessions store; malformed, conflicting, generic, outside-store, role-mismatch, and model-mismatch evidence fails.
- 2026-07-11: Quality and profiling text/JSON reports now identify advisory task-log summaries and observed telemetry with advisory diagnosis as not enforcement. Root and owning docs route to the short contract and public checks without adding a router, scheduler, or provider catalog refactor.
- 2026-07-11: Verification: 24 focused role/enforcement/doc/quality tests and 27 profiling tests pass; enforcement validator, hooks sync, strict Architecture Map (348 mapped / 780 scanned, 0 issues), Taskboard validation, and cached diff check pass.
- 2026-07-11: Real post-restart smoke against native rollout 019f5001-1509-77f2-8624-c41e352d22e2 failed as required for the current generic fallback: agent_role was absent and actual gpt-5.6-sol mismatched expected fast-worker gpt-5.6-luna. This is host evidence, not a repository-validator pass claim.
- 2026-07-11: Independent review cycle 1 found 0 HIGH and 4 actionable findings; cycle 2 found 0 HIGH and 3 additional actionable findings across architecture, correctness, ownership, tests, process, performance, and context cost. All were fixed. Cycle 3 was clean from both independent reviewers: 0 HIGH, 0 actionable MEDIUM/LOW.
- 2026-07-11: Quality: QTECH_001=pass; evidence: truthful proof_kind semantics, trusted native-rollout smoke boundary, 51 focused tests, strict repository validation, expected real fallback rejection, and two clean independent final reviews.
- 2026-07-11: Closed after truthful enforcement classification, trusted native role/model smoke, explicit advisory report labels, green validation, and clean cycle-3 reviews.
- 2026-07-11: Wave 1 integration correction: native rollout identity now uses
  `session_meta.payload.id`, ignores unrelated parent session metadata, and no
  longer requires the non-canonical child `payload.session_id`. Missing/generic
  roles and model mismatches still fail; 23 focused validation tests pass and a
  real fallback rollout is rejected for both missing role and wrong model.
