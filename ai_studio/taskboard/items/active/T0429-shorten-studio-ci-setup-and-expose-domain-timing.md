---
id: T0429
title: Shorten Studio CI setup and expose domain timings
status: doing
project: P001
epic: E001
priority: P1
tags: [ci, performance, verification, windows, linux]
created: 2026-07-14
updated: 2026-07-14
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
- [ ] A pushed Windows/Ubuntu run is compared with the `5m41s/4m16s` baseline.
- [ ] Any further concurrency or compiler-cache change is made only from new timings.
- [ ] Independent review and QTECH evidence pass.

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
- 2026-07-14: Started from GitHub Actions run 29316401973 step timings.
