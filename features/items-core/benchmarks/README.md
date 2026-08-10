# Items pipeline benchmark

The authoring loop — evaluate, validate, build, one-edit apply, rebuild — is
what this measures. Reproduce the current Windows result with:

```powershell
cmake -S templates/template -B templates/template/build/items-benchmark-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang
node ai_studio/dev_environment/python_run.mjs features/items-core/benchmarks/benchmark_items_pipeline.py --build-dir templates/template/build/items-benchmark-release --out features/items-core/benchmarks/results/windows-pipeline-2026-08-10.json
node ai_studio/dev_environment/python_run.mjs features/items-core/benchmarks/benchmark_items_pipeline_test.py
```

The runner uses a copied editable fixture and temporary build outputs. It does
not modify game or template authoring sources. The profile is a Windows
measurement, including native process-tree RSS; cross-platform CI verifies
behavior and builds but is not presented as a second performance profile.

The production/agent-loop analysis, before/after evidence, and backend decision
are recorded in [`items_pipeline_2026-07-16.md`](items_pipeline_2026-07-16.md).

## Retired: the runtime-format comparison

`results/windows-2026-07-15.json` and `results/linux-2026-07-16.json` are the
recorded runs of a comparison between two runtime formats — a validated binary
blob bound at startup, and compiled C arrays. Both exposed the same typed API
over semantically equal fixtures.

The blob was the provisional default there because it was smaller as authored
data and a value edit relinked nothing. That default is gone: the C catalog is
the only runtime path, so the second candidate, its runner, and its fixture no
longer exist. The results stay as the record of why the choice was open, not as
something reproducible.
