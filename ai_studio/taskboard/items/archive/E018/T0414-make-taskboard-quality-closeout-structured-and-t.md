---
id: T0414
title: Make Taskboard Quality structured and diagnostics lean
status: done
project: P001
epic: E018
priority: P0
tags: [taskboard, quality, evidence, profiling, context]
created: 2026-07-13
updated: 2026-07-13
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=Structured closeout and lean profiler: combined Taskboard/Quality 68/68; live profile is one compact record; Taskboard validation clean; independent review converged; Architecture Map owner coverage strict-clean."}]}
---

## What

Keep Quality mandatory while replacing free-form presence ceremony with a
structured closeout decision. Mechanical proof, quality judgment, and
Taskboard lifecycle remain separate responsibilities. In the same Taskboard
surface, retain only the useful privacy-safe context-size diagnostic.

## Done when

- [x] Applicable checks require `pass` plus structured evidence; not-applicable
      requires a non-empty reason.
- [x] The current presence/shape-only loophole is closed: a syntactically valid
      `block`, `review`, or `unverified` record cannot complete a task.
- [x] `block`, `review`, and `unverified` outcomes prevent a new transition to
      `done`; existing archived history remains grandfathered.
- [x] `skip` is not a silent third outcome; remove it or normalize legacy input
      to explicit not-applicable with a reason.
- [x] The CLI suggests relevant groups from ownership/change type but never
      silently waives checks or runs every Quality rule.
- [x] Routing stays coarse: code/runtime uses QTECH; player-facing UI adds
      QCLR; art uses QART and QASSET; GDD/design uses QGDD and QDES; asset
      pipeline work uses QASSET and QTECH, with only relevant groups required.
- [x] Technical proof cannot satisfy clarity, art, asset, GDD, or game-design
      judgment without matching evidence.
- [x] Store, CLI, API, parser, malformed-input, and close-transition tests prove
      the same contract without regex-dependent free-form state.
- [x] `taskboard profile --json` reports only per-store context bytes, returned
      and total counts, and truncation without task titles or bodies.
- [x] The profiler belongs to Taskboard rather than Core Harness, keeps its
      stable JSON surface, and has at most one direct contract plus one cheap
      CLI smoke in addition to existing body-free/limit-five product tests.
- [x] Profiler latency runs, duplicate summary/context/show records, path/query
      telemetry, one-off evidence, and elaborate benchmark fixtures are gone;
      limit-five/body-free product tests plus one small profiler contract remain.

## Open questions

- None blocking. The smallest compatible persisted Quality shape should be
  selected during implementation and documented once in the Taskboard contract.
  `ai_studio/quality/rules` and the Quality judgment/profile surface are not
  deletion targets without a separate evidence-backed decision.

## Log

- 2026-07-13: Lead confirmed Quality must remain a real acceptance gate because
  earlier game work was accepted without sufficient proof.
- 2026-07-13: Lead confirmed the context profiler is useful, but its benchmark
  and test ceremony should be reduced rather than preserved by default.
- 2026-07-13: TDD replaced log-presence gating with one structured frontmatter
  decision, per-check evidence, fail-closed catalog IDs, pass-only completion,
  explicit not-applicable reasons, fresh decisions after reopen, and coarse
  unioned CLI suggestions. Historical Quality log lines remain profiling-only.
- 2026-07-13: The Taskboard-owned context profiler is now one record per store
  with context bytes, returned/total counts, and truncation. Latency runs,
  summary/show duplication, path/query telemetry, and benchmark evidence were
  deleted. The implementation fell from 96 to 23 source lines; an implementation
  snapshot was 230 characters and the exact payload varies with task counts. Schema is
  `ai_studio.taskboard.context_profile.v1`.
- 2026-07-13: Verification: Taskboard 64/64, Quality 4/4, Taskboard validation
  clean, diff check clean. Independent review found five issues; all code/test
  findings were fixed and the focused recheck is clean. Architecture Map
  ownership is intentionally synchronized by T0413 before final closeout.
- 2026-07-13: Quality: QTECH_001=pass; evidence: structured state RED/GREEN,
  full Taskboard and Quality suites, live profiler output, validation, and
  independent review convergence.
- 2026-07-13: Final close after map sync: combined Taskboard and Quality suites 68/68, live privacy-safe profiler remains one compact record, Taskboard validation is clean, and the one-file Architecture Map owns Taskboard by subtree without profiler/test leaves.
- 2026-07-13: Quality: QTECH_001=pass; evidence: QTECH_001=Structured closeout and lean profiler: combined Taskboard/Quality 68/68; live profile is one compact record; Taskboard validation clean; independent review converged; Architecture Map owner coverage strict-clean.
