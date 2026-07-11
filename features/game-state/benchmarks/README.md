# Game-state codegen benchmark

This local benchmark exercises the frozen four-fragment fixture in
`fixtures/multi_fragment.schema.json`. It performs 3 in-process warmups,
25 measured warm runs, and 3 cold Python-process runs. The report includes the
warm median, warm p90, and every cold sample.

Run it through the repository Python boundary:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/benchmarks/benchmark_codegen.py
```

Refresh `baseline.json` only after an intentional local measurement:

```powershell
node ai_studio/dev_environment/python_run.mjs features/game-state/benchmarks/benchmark_codegen.py --update-baseline
```

A warm-median increase above 15% prints `investigate locally`. It never fails
the command and is not a cross-machine CI performance gate.
