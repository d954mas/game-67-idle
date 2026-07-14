---
id: T0427
title: Benchmark full verification domain concurrency
status: done
project: P001
epic: E001
priority: P1
tags: [verification, performance, concurrency, windows]
created: 2026-07-14
updated: 2026-07-14
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"2/3/4 each had three successful interleaved full Windows passes; fixed 3 median 23.219s vs 31.131s for 2; focused 27/27 and final full passed in 22.648s"}]}
---

## What

Measure whether full Studio verification is faster with 3 or 4 owner-domain
workers than the current fixed value of 2. Use the canonical native Windows
entry point, interleave trials to reduce warm-cache bias, and retain no runtime
configuration surface after the benchmark.

## Done when

- [x] Three valid full passes are recorded for each of 2, 3, and 4 workers.
- [x] Trial order is interleaved and wall time plus per-domain timing is captured.
- [x] The fixed worker count is selected from median time and dispersion, with no
      benchmark environment override left in production code.
- [x] The selected implementation passes focused Studio tests and one final full
      native Windows verification.
- [x] QTECH evidence states the performance claim and exact proof.

## Open questions

- Does additional domain parallelism reduce wall time, or only shift contention
  into Git, filesystem, Node, Python, and build subprocesses?

## Log

- 2026-07-14: Started native Windows benchmark. Planned interleaved order:
  `2,3,4 / 4,2,3 / 3,4,2`; temporary override will be removed after measurement.
- 2026-07-14: Three successful full passes per value: 2 workers
  `30.565/33.003/31.131 s` (median `31.131 s`, range `2.438 s`); 3 workers
  `24.127/22.883/23.219 s` (median `23.219 s`, range `1.244 s`); 4 workers
  `25.077/24.696/23.947 s` (median `24.696 s`, range `1.130 s`).
- 2026-07-14: Selected fixed concurrency 3. It improves the median by `7.912 s`
  (`25.4%`) versus 2; 4 is `1.477 s` slower than 3. Per-domain median seconds
  for 3 versus 4 were workspace `22.779/24.565`, assets `17.182/17.941`,
  work-management `6.209/7.694`, and template-release `10.116/11.579`.
- 2026-07-14: Two additional invalid passes failed in `template-release` and
  were excluded from timing medians. The 2-worker failure named
  `test_game_storage`; the earlier 4-worker collector did not retain its inner
  CTest name. The failures are not attributed to worker count. A native
  `test_game_storage` stress run then passed `100/100` in `7.36 s`; the original
  failure was not reproduced, so no root cause is claimed.
- 2026-07-14: Focused Studio tests passed `27/27` in `0.539 s`. Final fixed-3
  `node ai_studio/studio.mjs verify --full --json` passed all owner domains in
  `22.648 s` on native Windows.
- 2026-07-14: Independent review found no code/test defect. Its evidence P2s
  were resolved by recording per-domain medians, final gates, and the bounded
  flake investigation above.
- 2026-07-14: QTECH_001 pass. Claim: raising the fixed owner-domain worker pool
  from 2 to 3 shortens full native Windows verification without adding a config
  surface. Proof: three interleaved successful passes per value, the fixed-3
  concurrency unit test, `27/27` focused Studio tests, and one final full pass.
- 2026-07-14: Started interleaved native Windows benchmark for 2, 3, and 4 owner-domain workers.
- 2026-07-14: Selected fixed domain concurrency 3; temporary benchmark override removed.
- 2026-07-14: Quality: QTECH_001=pass; evidence: 2/3/4 each had three successful interleaved full Windows passes; fixed 3 median 23.219s vs 31.131s for 2; focused 27/27 and final full passed in 22.648s
