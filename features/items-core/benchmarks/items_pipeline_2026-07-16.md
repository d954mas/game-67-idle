# Finished Items pipeline benchmark — 2026-07-16

T0380 measured the finished single-source path before changing it:

```text
Items Lua -> isolated evaluator -> Snapshot/validation -> compact blob/header
          -> C runtime bind
```

The production sample is `templates/template` (6 items, 1 materialized level,
616-byte blob). The semantic edit scenario uses a temporary copy of the
items-core CLI fixture so preview/apply/undo never touch repository authoring
sources. The JSON reports are the detailed evidence:

- `results/windows-pipeline-baseline-2026-07-16.json` — before the measured fix;
- `results/windows-pipeline-2026-07-16.json` — accepted result with hashes for
  the benchmark, CLI, evaluator, and C bind sources.

## Profile and current measured path

The initial Windows run put the cost in fresh evaluator startup, not Snapshot,
package encoding, or runtime bind. A CLI `validate` cProfile sample spent 0.328
of 0.392 seconds waiting for its sandbox subprocess. Profiling the isolated
worker directly attributed 0.080 of 0.119 seconds to imports, including 0.064
seconds in `importlib.metadata`; the actual `_evaluate` path was 0.017 seconds
for this production sample.

The only optimization replaces `importlib.metadata.version("lupa")` with the
already imported public `lupa.__version__`. It removes dependency-metadata/email/
zip imports from every fresh worker without adding a cache, daemon, alternate
evaluator, or weaker isolation boundary.

| Flow | Before (ms) | After (ms) | Change |
|---|---:|---:|---:|
| Cold validate | 380.741 | 302.825 | -20.5% |
| Warm validate median | 371.985 | 314.622 | -15.4% |
| Cold build | 387.518 | 318.186 | -17.9% |
| No-op build | 377.428 | 295.104 | -21.8% |
| One-edit apply | 672.043 | 550.161 | -18.1% |
| Eight-command agent scenario | 4024.548 | 3288.347 | -18.3% |

The accepted result was remeasured after T0438 added project-wide input
rechecks, so these advisory timings describe the current path rather than an
isolated attribution to the import optimization. Agent peak process-tree RSS
was 84,639,744 bytes versus the 93,257,728-byte baseline (-9.2%).
The final scenario performs source, preview, apply, build, affected validate,
focused inspect, stale-hash conflict, and returned-inverse undo: 8 tools, 79
logical project reads, 6,455 stdout bytes, 331 stderr bytes, and four passing
diagnostic-quality checks.

## Runtime and backend decision

The generated production blob binds through the shipping C implementation.
Nine in-process samples produced a 399 ns median; owned memory is exactly 616
bytes steady and 1,232 bytes while the input and owned copy coexist. The
external benchmark process wall time is reported separately and is not treated
as bind latency.

The result ratifies the existing choices:

- design backend: pinned `lupa@2.8`, `lupa.lua54`, Lua 5.4;
- runtime format: compact blob v2 plus generated ABI header;
- execution model: a fresh bounded worker for every evaluation.

The remaining design-time cost is process isolation/startup. A persistent
evaluator or speculative cache would add invalidation/state complexity and
weaken the current fresh-process safety property; this sample does not justify
that trade. Snapshot/package work and C bind are not bottlenecks.

The prior T0365 Linux result independently matches blob bytes, semantic
checksum, stable-header/no-relink behavior, and records 165 ns fixture bind.
The current branch continues to run the complete Studio verification matrix on
Ubuntu and Windows. T0380 deliberately does not add another stress-size or full
Linux timing matrix because neither could change the release choice established
by the production profile and existing cross-platform proof.

## Budgets

All timing and memory observations remain advisory machine samples. No numeric
budget or CI performance gate is accepted by this task; adopting one requires an
explicit product/release decision and representative hardware policy.
