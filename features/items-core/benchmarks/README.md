# Items runtime package benchmark

This is the bounded T0365 comparison, not the later full Items pipeline
benchmark. Both candidates expose the same typed API and the runner refuses to
measure them unless their two-item/three-level catalogs are semantically equal.

Configure an optimized build, then run:

```powershell
cmake -S templates/template -B templates/template/build/items-benchmark-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang
node ai_studio/dev_environment/python_run.mjs features/items-core/benchmarks/benchmark_runtime_package.py --build-dir templates/template/build/items-benchmark-release --out features/items-core/benchmarks/results/windows-2026-07-15.json
node ai_studio/dev_environment/python_run.mjs features/items-core/benchmarks/benchmark_runtime_package_test.py
```

Recorded cross-platform results live in
`results/windows-2026-07-15.json` and `results/linux-2026-07-16.json`.

The runner records cold candidate compile/link, link-only and no-op build wall
time; cold/no-op/value-edit generation; raw bytes and a zlib compression proxy;
executable bytes and selected code/data section payloads; blob bind/copy bytes; and one million checked typed
level reads. Timings are advisory machine samples, while byte counts, changed
outputs, checksums, and relink decisions are deterministic proof.

On the recorded Windows run, tiny C arrays are faster and smaller in selected
code/data section payloads than the validated blob. Exact machine-dependent
samples live only in the dated JSON result so reruns cannot leave duplicated
prose stale. These section payloads are not process RSS or mapped/resident
memory; T0380 owns that profiling. The blob is much
smaller as authored data (568 raw / 267 zlib bytes versus 3,291 / 913) and a
value edit changes only the blob; the ABI header stays byte-identical and no C
relink is required. Its copy peak was 1,136 bytes for this fixture.

The Linux run reproduces the same semantic checksum, output sizes, stable-header
value edit, and no-relink decision. Therefore the provisional blob default
stands for scalable iteration and pack placement; C arrays remain the tiny
fallback/reference candidate. This does not claim that the blob wins
tiny-fixture access latency. T0380 owns representative full-pipeline sizes,
memory profiling, and budget ratification.

## Finished pipeline

T0380's production/agent-loop benchmark, profile, before/after evidence, and
backend decision are recorded in
[`items_pipeline_2026-07-16.md`](items_pipeline_2026-07-16.md). Reproduce the
current Windows result with:

```powershell
node ai_studio/dev_environment/python_run.mjs features/items-core/benchmarks/benchmark_items_pipeline.py --build-dir templates/template/build/items-benchmark-release --out features/items-core/benchmarks/results/windows-pipeline-2026-07-16.json
node ai_studio/dev_environment/python_run.mjs features/items-core/benchmarks/benchmark_items_pipeline_test.py
```

The runner uses a copied editable fixture and temporary build outputs. It does
not modify game or template authoring sources.

The finished pipeline profile is a Windows measurement, including native
process-tree RSS. Cross-platform CI verifies behavior and builds but is not
presented as a second full performance profile. The older Linux JSON above is
the separate T0365 package-format proof (semantic checksum, output sizes,
stable header/no-relink, and fixture bind); it is not a Linux rerun of the
finished Workbench/agent pipeline benchmark.
