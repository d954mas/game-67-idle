---
id: T0431
title: Groom and archive stale Taskboard work
status: done
project: P001
epic: E001
priority: P1
tags: [taskboard, cleanup, archive, backlog]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=69 Taskboard tests, full Windows verify 10 domains, independent review"}]}
---

## What

Review every current Studio project, epic, and task. Close completed or
superseded work with explicit evidence, retain still-useful future work at the
right lifecycle state, and delete only records that are pure duplicate/noise
with no historical value.

## Done when

- [x] Every active task has a keep, refine, close, or delete judgment.
- [x] Every project and epic has a justified current status and live children.
- [x] Closed work is archived through the guarded Taskboard path.
- [x] Deleted records have no unique decision, evidence, or dependency value.
- [x] Taskboard validation, focused tests, independent review, and full Windows
      verification pass.

## Open questions

- Which `review` items are already implemented versus still awaiting product
  acceptance?
- Which old Canvas/template ideas remain useful enough to keep as `idea`?

## Log

- 2026-07-14: Started after T0430. Scope is current Studio Taskboard records;
  existing archive content is read only when needed to prove duplication or
  completion. No game-private stores are changed in this pass.
- 2026-07-14: Reviewed all 60 pre-existing active tasks plus this grooming task
  against current code, task logs, and git evidence. Active work fell from 60
  to 24 cards and stale `review` from 19 to 4.
- 2026-07-14: Archived 29 completed, superseded, rejected, or non-current tasks
  through guarded closeout. Deleted only empty/duplicate T0208, T0211, T0212,
  T0215, T0318, T0319, and T0320; useful T0208/T0215 scope was first merged
  into T0326/T0269. Independent review restored T0368/T0387/T0389 to the
  archive because they retain unique decisions or future contracts.
- 2026-07-14: Closed empty completed epics E002 and E014. Promoted the shipped
  Canvas epic E010 to active and filled its boundary. Kept E009 active for web
  parity/new-game Canvas, narrowed E016 to the Items authoring vertical, and
  extracted runtime/state work into E019.
- 2026-07-14: Four reviews remain intentionally: T0222/T0225/T0336 require
  explicit visual acceptance, while T0258 requires a matte/bleed strategy
  decision. T0259's completed relocation was closed and its real silent-wipe
  risk split into focused backlog T0432.
- 2026-07-14: Independent review also removed the last E019 runtime/state
  requirements from E016, refreshed changed-record dates, and corrected stale
  labels. Final Taskboard validation and diff checks pass, all 69 focused tests
  pass, and `studio verify --full` passes all 10 domains on Windows in 23s.
- 2026-07-14: Quality: QTECH_001=pass; evidence: QTECH_001=69 Taskboard tests, full Windows verify 10 domains, independent review
