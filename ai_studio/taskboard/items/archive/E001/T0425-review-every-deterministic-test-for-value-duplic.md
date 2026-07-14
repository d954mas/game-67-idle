---
id: T0425
title: Review every deterministic test for value duplication and speed
status: done
project: P001
epic: E001
priority: P1
tags: [tests, audit, performance, simplification]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"185 starting test files and 1,969 concrete cases reviewed; focused owner suites passed; independent findings corrected; 10-domain Windows full passed in 30.6 s"}]}
---

## What

Review every tracked deterministic Node, Python, and native C test case. For
each case decide whether it proves a unique contract, duplicates another test,
asserts the intended result strongly enough, and can run with less process or
fixture overhead. Keep the audit diagnostic: do not create a maintained test
registry or new test taxonomy.

## Done when

- [x] Inventory every tracked deterministic test file and test case with no unassigned paths.
- [x] Classify every case as keep, merge/delete, strengthen, or speed up, with an owning contract.
- [x] Implement only reviewed changes that reduce duplication/overhead without weaker evidence.
- [x] Preserve real CLI, concurrency, transaction, privacy, release, and external-tool boundaries.
- [x] Pass affected owner suites and one final Windows full verification run.
- [x] Record QTECH_001 evidence, independent review, measurements, and atomic commits.

## Open questions

- None. A repeated assertion is retained only when it proves a distinct layer or failure boundary.

## Log

- 2026-07-14: Started after T0424. Scope is all 185 tracked deterministic test
  files across the 10 owner domains, including Node, Python, and native C.
  Audit output is temporary evidence, not a new hand-maintained map.
- 2026-07-14: Reviewed 1,969 concrete cases (1,968 ledger rows; one Canvas
  dynamic declaration expands to two cases). Every row records the asserted
  contract, duplication decision, evidence strength, speed opportunity, and
  boundary that must stay real. Classification: 1,690 keep, 82 merge/delete,
  33 strengthen, and 163 speed candidates. The four detailed local ledgers
  remain ignored diagnostic output under `tmp/`; no permanent registry or
  taxonomy was added.
- 2026-07-14: Deleted three low-value test files (nine structural/style/
  migration assertions): asset ownership proxy guards, Inspector domain
  regex/line-count guards, and a Canvas import-count budget. Existing owner
  suites retain manifest integrity, studio assignment, CLI behavior, and
  operation boundaries. Architecture tests now clean all 18 temporary
  fixtures they create.
- 2026-07-14: Kept real subprocesses where they prove transport or external
  behavior. Canvas CLI improved 7.81 s to 5.26 s; gallery promotion reduced
  18 process launches to 2 and focused time to about 0.40 s; progression
  generator improved 0.91 s to 0.25 s. Taskboard removed three more redundant
  CLI children, but no wall-time gain is claimed because the run stayed within
  noise. Items Viewer fake-Python optimization was reverted after review: its
  speedup weakened malformed-file and Node-to-Python evidence.
- 2026-07-14: Corrected two quality gaps discovered during the audit. Web
  build/runtime product failures now exit 1 instead of being reported as host
  skips, with configure, missing-artifact, state-set, and save regression
  coverage. `new_template --force` now has successful overwrite/catalog proof
  without destructive pre-delete semantics.
- 2026-07-14: Assets as a whole measured 14.18 s before and 14.35-15.04 s in
  later noisy runs, so no aggregate Assets speedup is claimed. Further
  `new_game` speed requires a bounded production transaction refactor; adding
  a generic test DI layer would make the Studio larger and was rejected.
- 2026-07-14: Independent review found the unsafe force delete, weakened Items
  Viewer boundary tests, shallow web regressions, and one stale owner-test row;
  all four findings were corrected. Focused suites passed, then the canonical
  native Windows full verification passed all applicable domains in 30.6 s;
  no WSL process was used.
- 2026-07-14: Quality: QTECH_001=pass; evidence: all 185 starting files and 1,969 concrete cases reviewed; focused owner suites passed; independent findings corrected; 10-domain Windows full passed in 30.6 s
- 2026-07-14: Quality: QTECH_001=pass; evidence: 185 starting test files and 1,969 concrete cases reviewed; focused owner suites passed; independent findings corrected; 10-domain Windows full passed in 30.6 s
