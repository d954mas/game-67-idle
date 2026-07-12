# Windows native audio benchmark

Measured at: 2026-07-12T14:37:29.5966398Z
Scope: **Windows native plus paired Emscripten Release build**
Current source: working tree at 19b15dcc94a6b6e0d364383fc41b1bf67e93c2bb (58 changed/untracked paths)
Baseline: 0fb94303f
Compiler: clang version 19.1.7

## Results

| Metric | Baseline | Current | Delta |
| --- | ---: | ---: | ---: |
| clean native game build | 17503.329 ms | 19451.7419 ms | 1948.4129 ms (11.1317%) |
| game.exe | 1901568 B | 2347008 B | 445440 B (23.4249%) |
| game.ntpack | 1330756 B | 1368056 B | 37300 B (2.8029%) |

The packed WAV+MP3 sources total 36981 bytes. Pack delta includes pack framing/alignment, so it is measured independently rather than inferred from source sizes. Binary and pack deltas compare the complete dirty working tree with the baseline; they are not symbol-level attribution to audio alone.

## Clean Emscripten Release game build

Both sides used the same warmed checkout-local Emscripten cache, compiler,
engine checkout, generated asset header, and clean `game` target after
configure. Baseline is `0fb94303f`.

| Metric | Baseline | Current | Delta |
| --- | ---: | ---: | ---: |
| build wall time | 25815.2084 ms | 25196.1988 ms | -619.0096 ms (-2.3978%) |
| game.js | 106623 B | 121663 B | +15040 B (+14.1058%) |
| game.wasm | 1071462 B | 1081435 B | +9973 B (+0.9308%) |

## Clean native audio translation-unit compile

Each Release TU was compiled 3 times to a fresh object with the production warning policy and -Werror. These are process wall-clock measurements with a warm OS filesystem cache; the minimum is the requested low-noise figure and the median shows normal local variation.

| TU | Minimum ms | Median ms | Object BSS bytes |
| --- | ---: | ---: | ---: |
| audio | 76.8352 | 79.3136 | 2092 |
| audio_resource | 45.2269 | 46.9455 | 0 |
| audio_backend_miniaudio | 87.5574 | 89.0427 | 42841 |
| audio_miniaudio_impl | 3444.2011 | 3499.6047 | 52 |

Sum of per-TU minima: **3653.8206 ms**; sum of medians: **3714.9065 ms**. The pinned Miniaudio implementation TU contributes **94.26%** of the minimum sum.

Total fixed BSS across the four measured audio objects: **44985 bytes**. This is object static storage, not total runtime memory.

## Runtime latency

Measured on a real Windows device: init 134.637000 ms; WAV load/decode 0.086700 ms; unlock 0.321400 ms; first-play API submission 0.001900 ms.

first_play_submit_ms ends when the audio API accepts the voice. It is not a microphone-based audible-onset measurement.

## Fixed caps and memory boundary

- Public runtime: 64 clip slots and 32 simultaneous voice slots.
- Native backend: 64 clip slots and 32 voice slots.
- Web source constants: 64 clip slots and 32 voice slots; inspected only, not measured in a browser.
- Fixed native audio-object BSS measured from Release COFF objects: 44985 bytes.
- Decoded PCM is capped at 128 MiB per clip and 256 MiB aggregate on both native and web; Miniaudio device/backend heap outside decoded PCM remains dynamic.

## Not measured

Linux runtime performance, WebAudio latency, acoustic onset, and peak backend/device heap outside capped decoded PCM were not measured. No claim about them is made from this run.

Raw evidence: result.json. Reproduce with:

~~~powershell
powershell -ExecutionPolicy Bypass -File features/audio-core/benchmarks/run.ps1 -CompileRuns 3 -RuntimeDevice -WebEvidencePath features/audio-core/benchmarks/results/windows-2026-07-12/web-result.json -OutputDirectory features/audio-core/benchmarks/results/windows-2026-07-12
~~~
