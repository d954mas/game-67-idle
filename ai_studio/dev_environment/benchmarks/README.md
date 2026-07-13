# Toolchain Benchmarks

Run the measured local baseline without an enforced threshold:

```powershell
node ai_studio/dev_environment/toolchain_benchmark.mjs --game template --samples 3 --out ai_studio/dev_environment/benchmarks/template.windows.json
```

Each record captures repeated Python process startup, cold configure/build,
one-source warm rebuild, and no-op build samples with min/median/max summaries
plus toolchain and Git-state metadata. Results are machine-local evidence; add a gate
only after several stable baselines establish a defensible threshold.
Use `--workspace-note <text>` when preserved dirty work affects the measured tree.
