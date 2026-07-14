---
id: T0429
title: Shorten Studio CI setup and expose domain timings
status: done
project: P001
epic: E001
priority: P1
tags: [ci, performance, verification, windows, linux]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"focused 27/27 and native Windows full passed; GitHub run 29317633550 attempts 1 and 2 passed on Windows and Ubuntu; shallow checkout and junction reduced fixed setup without removing any quality domain"}]}
---

## What

Reduce the fixed setup cost of the Windows/Ubuntu Studio verification workflow
and expose the domain timings already measured by the harness. Keep one full
quality gate, the two-platform matrix, and all native, web, runtime, and
sanitizer proofs.

## Done when

- [x] Text-mode verification prints each owner-domain duration without another process.
- [x] Checkout does not fetch unused full history.
- [x] Windows exposes CMake through a fast space-free path without copying the toolchain.
- [x] Python package downloads are cached from the canonical requirements input.
- [x] Focused workflow/harness tests and native Windows full verification pass.
- [x] A pushed Windows/Ubuntu run is compared with the `5m41s/4m16s` baseline.
- [x] Any further concurrency or compiler-cache change is made only from new timings.
- [x] Independent review and QTECH evidence pass.

## Open questions

- Which owner domain is the clean-run critical path on each hosted runner?
- Does three-domain concurrency help or hurt the lower-core CI runner?

## Log

- 2026-07-14: Baseline GitHub run `29316401973`: Windows job `341s`, including
  checkout `34s`, CMake path copy `21s`, Python bootstrap `34s`, Emscripten
  `24s`, and full verification `214s`; Ubuntu job `256s`, including checkout
  `19s`, apt `17s`, Python bootstrap `21s`, Emscripten `24s`, and full
  verification `170s`.
- 2026-07-14: Text verification now prints each existing `durationMs` as seconds;
  no process, timing collector, output file, or second verification pass was added.
  Focused Studio/CI contracts passed `27/27`.
- 2026-07-14: Workflow now uses shallow recursive checkout, setup-python's pip
  download cache keyed by the canonical direct requirements file, and a Windows
  NTFS junction for the space-free CMake path instead of copying the toolchain.
  The junction command was exercised locally through a real CMake invocation.
- 2026-07-14: Native Windows full verification passed all applicable domains;
  its longest owner-domain duration was `20.143s`. The new output exposed local owner durations directly;
  no quality domain or command was removed.
- 2026-07-14: Independent review found no P1/P2 issues. It confirmed that the
  full gate needs only the current commit, the junction and pip-cache semantics
  are sound, and the two-platform matrix plus every quality domain remain intact.
- 2026-07-14: GitHub run `29317633550` passed twice on both platforms. Against
  baseline, checkout changed from Ubuntu `19s` to `5s/7s` and Windows `34s` to
  `17s/16s`; the Windows CMake path changed from `21s` to `9s/6s`. Warm Python
  bootstrap was `19s` on Ubuntu and `27s` on Windows versus `21s/34s` baseline.
- 2026-07-14: End-to-end remained runner-sensitive. Cold jobs were Ubuntu
  `4m59s` and Windows `6m25s`; warm jobs were Ubuntu `5m23s` and Windows `5m13s`
  versus the earlier `4m16s/5m41s`. Full-gate time, not setup, caused the spread:
  Ubuntu `228s/250s` and Windows `263s/226s` versus `170s/214s` baseline.
- 2026-07-14: New timings locate the hosted bottleneck. Warm Ubuntu owner times:
  assets `183s`, template-release `179s`, runtime `168s`, features `64s`; warm
  Windows: template-release `192s`, assets `131s`, workspace `55s`. The existing
  three-domain pool schedules these four heavy owners near its theoretical
  three-worker lower bound; the prior interleaved local benchmark also selected
  three over two or four. No concurrency change was justified.
- 2026-07-14: Tested Node 24 `--test-isolation=none` against all 87 asset Node
  files. It retained all tests but regressed the native lane from `13.778s` to
  `39.505s`, so it was rejected. Compiler/build caches were also deferred: they
  add invalidation and restore ceremony, while the two hosted attempts do not
  isolate a stable compile-only saving.
- 2026-07-14: QTECH_001 pass. The supported claim is narrower than a guaranteed
  wall-time reduction: fixed checkout/CMake/bootstrap overhead fell, both CI
  attempts and the native full gate passed, and no test, platform, sanitizer,
  runtime, native, or web proof was removed.
- 2026-07-14: Started from GitHub Actions run 29316401973 step timings.
