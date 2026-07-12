# audio-core benchmark

This benchmark keeps machine measurements separate from estimates. It compares
the current working tree with baseline commit `0fb94303f`, using clean Release
build directories and the same compiler for both sides.

Run from the repository root:

```powershell
powershell -ExecutionPolicy Bypass -File features/audio-core/benchmarks/run.ps1 `
  -CompileRuns 3 -RuntimeDevice `
  -WebEvidencePath features/audio-core/benchmarks/results/windows-2026-07-12/web-result.json `
  -OutputDirectory features/audio-core/benchmarks/results/windows-2026-07-12
```

`run.ps1` defaults to five compile samples. The canonical checked evidence uses
three and merges the checked paired-Web raw file. To regenerate that raw file,
first configure clean baseline/current Emscripten Release build directories
against the same engine checkout and `GAME_EMSCRIPTEN_CACHE_DIR`, generate the
baseline asset header with its native pack target, then run:

```powershell
powershell -ExecutionPolicy Bypass -File features/audio-core/benchmarks/run_web.ps1 `
  -BaselineBuildDirectory <baseline-web-build> `
  -CurrentBuildDirectory <current-web-build> `
  -OutputPath features/audio-core/benchmarks/results/windows-2026-07-12/web-result.json
```

The web runner warms the shared cache, cleans each build, times the complete
`game` target, and records JS/WASM bytes. The native runner validates the web
schema and baseline before merging it; rerunning the canonical command cannot
silently discard the Web section.

```powershell
node features/audio-core/benchmarks/verify_results.mjs
```

Add `-RuntimeDevice` to initialize the real native backend and submit one quiet
UI click (`gain=0.05`) to the default output device. Without that opt-in, device
latency is reported as unverified. If no real device is available, the harness
returns a successful probe with `device_available=false`; it never substitutes
Miniaudio's null backend and calls that a real-device result.

The runner records native measurements and deterministically validates/merges
the supplied paired Emscripten evidence against the same baseline; both sides
must use the same warmed `EM_CACHE`, compiler, engine checkout, and pack/codegen
preconditions so the web delta is comparable.

The benchmark records:

- clean Release compile time for each native audio translation unit under the
  production warning policy (`-Werror`), with configurable fresh object outputs
  (default five; the checked result uses three) and minimum/median values;
- clean native game build time and `game.exe` size against the pinned baseline;
- `game.ntpack` size against the same baseline;
- real-device init, WAV decode/load, unlock, and first-play submission latency
  when explicitly enabled;
- fixed clip/voice slot limits and static audio-object BSS. Decoded PCM and
  backend heap are called out separately; decoded PCM is capped at 128 MiB per
  clip and 256 MiB aggregate on both native and web.

Generated evidence goes under `benchmarks/results/`. The report is specific to
the recorded OS, compiler, CPU, checkout state, and command. The paired
Emscripten rows are Web build evidence, but the report is not Linux build
evidence or WebAudio runtime-performance evidence.
