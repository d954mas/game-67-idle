# video_gen_experiment (T0257 — local video-gen)

**Date:** 2026-07-03 (phase 1, base infra) / 2026-07-04 (phase 2, models + smoke generation; phase 3, GO/NO-GO gate experiments)
**Owner task:** T0257 (game-67-idle taskboard).
**Status:** Phase 1 (isolated ComfyUI base, GPU-verified) DONE. Phase 2 (WAN 2.2 I2V GGUF stack +
smoke generation) DONE. **Phase 3 (R1 style-drift / R2 matting / R3 iteration-latency gate
experiments on the real angel-wings fixture) DONE — verdict: R1 PASS, R2 RESOLVED in T0261
(CorridorKey PASS / MatAnyone PARTIAL / key_matte FAIL-on-glow — see the "T0261" section at the
bottom for the tools table), R3 PASS. Full report:
`C:\projects\game-67-idle\tmp\t0257_phase3_report.md`.** See "Phase 3" section below for a summary.
Phase 4+ (building the actual Track B pipeline stages) is a separate future packet, not started
here.

## What this is

An ISOLATED, wholesale-deletable ComfyUI install used to explore local video generation on the
lead's RTX 4080 Laptop GPU (12GB VRAM), at zero per-iteration cost. Everything lives under this
one folder, outside any git repo. Nothing was installed system-wide: no system Python touched, no
PATH edits, no Windows services registered. The only footprint outside this folder is a status log
at `C:\projects\game-67-idle\tmp\t0257_setup_log.md` (gitignored tmp, not part of the repo).

## Hardware / driver facts (verified via nvidia-smi + torch)

- GPU: NVIDIA GeForce RTX 4080 Laptop GPU, 12282 MiB VRAM (WDDM driver model)
- Driver: 576.88, reports max supported CUDA runtime **12.9**
- Chosen build: ComfyUI portable **nvidia_cu126** variant (NOT the default "nvidia" variant —
  see "Install shape" below for why)

## Install shape chosen, and why

ComfyUI v0.27.0 (repo: `Comfy-Org/ComfyUI`, formerly `comfyanonymous/ComfyUI`) ships 4 portable
Windows release assets: `amd`, `intel`, `nvidia` (default), `nvidia_cu126`.

1. First tried the **default nvidia** portable (ships torch 2.12.0+**cu130**). Extracted fine, but
   `torch.cuda.is_available()` returned `False` with
   `cudaGetDeviceCount() returned cudaErrorNotSupported, likely using older driver`.
   Root cause: the default build's CUDA 13.0 torch wheel needs a newer driver than 576.88 (which
   caps at CUDA 12.9). **Deleted this install** to reclaim disk.
2. Switched to the **nvidia_cu126** portable variant instead (ships torch 2.12.0+**cu126**, CUDA
   12.6 — safely under the driver's 12.9 ceiling). Verified working: `torch.cuda.is_available()` ->
   `True`, device `NVIDIA GeForce RTX 4080 Laptop GPU`, ~11.99 GB VRAM visible.

**Portable was preferred over a manual venv** because it ships an embedded Python + matching CUDA
torch build with zero manual wheel-matching once the right variant is picked — simplest robust
shape for this box, per T0257 phase-1 instructions. No fallback to git-clone+venv was needed.

## Versions installed

- ComfyUI: v0.27.0
- Embedded Python: 3.12.10
- torch: 2.12.0+cu126 (torchvision 0.27.0+cu126... torchaudio 2.11.0 build tag +cu126 — installed
  by the portable release itself, not hand-picked)
- ComfyUI-Manager: cloned from `https://github.com/Comfy-Org/ComfyUI-Manager.git` (main branch,
  cloned 2026-07-03) into `ComfyUI/custom_nodes/ComfyUI-Manager`; its `requirements.txt` deps
  (GitPython, PyGithub, matrix-nio, jsonschema, etc.) were pip-installed into the embedded Python
  — fully isolated inside this folder, nothing system-wide.

## How to start

```
cd C:\projects\video_gen_experiment\ComfyUI_windows_portable
python_embeded\python.exe -s ComfyUI\main.py --listen 127.0.0.1 --port 8188
```

(or double-click `run_nvidia_gpu.bat` in that folder for the same thing with default args). Then
open http://127.0.0.1:8188 in a browser. First boot will take a bit longer while ComfyUI-Manager
seeds its caches (node list, registry data, etc.) over the network.

## How to stop

Ctrl+C in the terminal running it, or kill the `python_embeded\python.exe` process. No background
services are registered — closing the process fully stops everything.

## How to DELETE this experiment wholesale

Delete the folder `C:\projects\video_gen_experiment\` (this whole directory). That is the entire
footprint — no registry keys, no PATH entries, no services, no global Python packages were
touched. The only file that lives outside this folder is the status log at
`C:\projects\game-67-idle\tmp\t0257_setup_log.md`, which is a plain gitignored markdown note and
can be deleted separately with no effect on this install.

## Disk usage

~5.7 GB total (well under the 12 GB base-install budget). Breakdown: ComfyUI portable + embedded
Python + CUDA torch ≈ 5.6 GB, ComfyUI-Manager clone + its deps ≈ 0.1 GB.

## Smoke test evidence (2026-07-03, cu126 build)

Booted headless (`--listen 127.0.0.1 --port 8188`), then:
- `GET /system_stats` returned `pytorch_version: "2.12.0+cu126"`, `embedded_python: true`, and a
  `devices` entry: `"name": "cuda:0 NVIDIA GeForce RTX 4080 Laptop GPU : cudaMallocAsync"`,
  `"type": "cuda"`, `"vram_total": 12878086144` (~12 GB) — confirms real CUDA mode, not CPU
  fallback.
- Boot log independently confirmed: `Total VRAM 12282 MB, total RAM 32387 MB`,
  `pytorch version: 2.12.0+cu126`, `Device: cuda:0 NVIDIA GeForce RTX 4080 Laptop GPU : cudaMallocAsync`.
- `GET /` returned HTTP 200.
- ComfyUI-Manager loaded cleanly during prestartup (1.8s), no errors.
- One benign warning: `WARNING: You need pytorch with cu130 or higher to use optimized CUDA
  operations.` — this just means the newest `comfy_kitchen` fused-kernel fast path is unavailable
  on cu126; ComfyUI falls back to its `eager` backend automatically. Not a blocker for phase 2.
- Server was shut down after the smoke test; no processes were left running.

## Known caveat carried into phase 2

The **default** portable variant (cu130) is what most current ComfyUI docs/tutorials assume; this
box needed the **cu126** variant specifically because of the driver ceiling (CUDA 12.9 max). If the
lead updates the NVIDIA driver to something supporting CUDA 13.0+ later, the default variant (or an
in-place torch upgrade) becomes viable again — not necessary for now.

## Phase 1: no models installed

Per phase-1 scope, no checkpoints/video models were downloaded in phase 1. Model selection came
from a separate community-research report (`tmp/research_T0256_community_2026-07-04.md` +
`tmp/research_T0256_SYNTHESIS.md` in the game-67-idle repo), landing in phase 2 below.

---

# Phase 2 — WAN 2.2 I2V GGUF stack + smoke generation (2026-07-04)

## What was installed (model-specific, on top of the phase-1 base)

Stack chosen per the T0256 community report's headline recommendation: **WAN 2.2 I2V 14B GGUF Q4
(two-expert high/low-noise split) + the 4-step Lightning/LightX2V distill LoRA**, run through
ComfyUI-GGUF for quantized-unet loading. This is the community-confirmed 12GB-VRAM-class recipe
(report cites ~4.6 min/clip on a 6GB RTX 3050 with the same recipe; "Q4 for 12GB" as the standard
quant guidance).

### Custom node

- **ComfyUI-GGUF** — `git clone https://github.com/city96/ComfyUI-GGUF.git` into
  `ComfyUI/custom_nodes/ComfyUI-GGUF`. Pinned commit: `6ea2651e7df66d7585f6ffee804b20e92fb38b8a`
  (2026-01-12). License: Apache-2.0. Its one runtime dependency (`gguf>=0.13.0`, resolved to
  `gguf==0.19.0`) was pip-installed into the embedded Python — isolated, nothing system-wide.
  Installed via git clone + pip rather than the Manager UI, since this is a headless box and the
  exact commit is easier to pin/log this way.

### Models table

| File | Placed at | Size | Source repo | License |
|---|---|---|---|---|
| `Wan2.2-I2V-A14B-HighNoise-Q4_K_S.gguf` | `ComfyUI/models/unet/` | 8.75 GB (8,748,151,296 B) | [QuantStack/Wan2.2-I2V-A14B-GGUF](https://huggingface.co/QuantStack/Wan2.2-I2V-A14B-GGUF) (GGUF conversion of `Wan-AI/Wan2.2-I2V-A14B`) | Apache-2.0 |
| `Wan2.2-I2V-A14B-LowNoise-Q4_K_S.gguf` | `ComfyUI/models/unet/` | 8.75 GB (8,748,151,296 B) | same as above | Apache-2.0 |
| `umt5-xxl-encoder-Q4_K_M.gguf` | `ComfyUI/models/clip/` | 3.66 GB (3,655,145,312 B) | [city96/umt5-xxl-encoder-gguf](https://huggingface.co/city96/umt5-xxl-encoder-gguf) | Apache-2.0 |
| `wan_2.1_vae.safetensors` | `ComfyUI/models/vae/` | 254 MB (253,815,318 B) | same QuantStack repo, `VAE/Wan2.1_VAE.safetensors` (WAN 2.2 A14B I2V uses the WAN 2.1 VAE, confirmed via official docs/workflow) | Apache-2.0 |
| `wan2.2_i2v_lightning_rank64_high_noise.safetensors` | `ComfyUI/models/loras/` | 1.23 GB (1,226,977,424 B) | [lightx2v/Wan2.2-Lightning](https://huggingface.co/lightx2v/Wan2.2-Lightning), folder `Wan2.2-I2V-A14B-4steps-lora-rank64-Seko-V1/high_noise_model.safetensors` | Apache-2.0 |
| `wan2.2_i2v_lightning_rank64_low_noise.safetensors` | `ComfyUI/models/loras/` | 1.23 GB (1,226,977,424 B) | same repo, `.../low_noise_model.safetensors` | Apache-2.0 |

**Total model weight: ~23.86 GB** (budget was ≤25 GB additional — fits with ~1.1 GB margin).

### Quant choice reasoning (deviations from the report, and why)

- **GGUF unet quant = Q4_K_S (not Q4_0/Q4_1/Q4_K_M).** The report names "Q4" generically as the
  12GB-confirmed quant family. Of the available Q4 variants, `Q4_K_S` is the smallest true-Q4
  K-quant available for BOTH high-noise and low-noise experts symmetrically (Q4_0 only exists for
  LowNoise, not HighNoise, in the QuantStack repo) — picked for consistency and to stay inside the
  25GB disk budget. Going lower (Q3/Q2) would save more disk but the report only confirms Q4 as
  quality-safe; not attempted.
- **Text encoder = GGUF Q4_K_M (3.66GB), not the fp8/fp16 safetensors** the official Lightning
  workflow markdown note suggests. The phase-2 instructions explicitly allow "quantized ok" for the
  text encoder, and Q4_K_M vs the fp8 scaled safetensors (6.74GB) saves ~3GB of the tight 25GB
  budget with no observed quality problem on this smoke run.
- **VAE and LoRAs** used exactly as the official Lightning `NativeComfy` workflow specifies (no
  quantization available/needed — they're already small).

## Smoke workflow (the phase gate)

Saved next to this README at `C:\projects\video_gen_experiment\smoke_workflow_api.json` — a
ComfyUI `/prompt` **API-format** payload (`{"prompt": {...}}`), ready to POST directly, for reuse.

Graph (17 nodes), reconstructed and cross-checked node-by-node against the officially published
`lightx2v/Wan2.2-Lightning` `...-NativeComfy.json` workflow (same wiring/defaults) AND against this
exact ComfyUI install's own node source (`comfy_extras/nodes_wan.py`, `nodes.py`,
`comfy_extras/nodes_model_advanced.py`, `comfy_extras/nodes_video.py`) to make sure input names
matched this specific v0.27.0 build:

`UnetLoaderGGUF`(high) → `LoraLoaderModelOnly`(Lightning high) → `ModelSamplingSD3`(shift=5.0) →
`KSamplerAdvanced`(steps 4, split step 0→2, add_noise=enable, return_with_leftover_noise=enable)
— in parallel with the same chain for the low-noise expert (split step 2→4, add_noise=disable) —
chained latent-to-latent. `CLIPLoaderGGUF`(type=`wan`) feeds two `CLIPTextEncode` (positive/negative,
cfg=1.0 so negative is mostly inert — matches the Lightning-distilled cfg=1 convention).
`WanImageToVideo` conditions on the `LoadImage` start frame + `VAELoader`. Output:
`VAEDecode` → `CreateVideo`(fps=16) → `SaveVideo`(mp4/h264).

**Smoke-test settings (deliberately tiny, per phase-2 instructions):**
- Input image: locally generated via the embedded Python one-liner (PIL) — a flat green
  (0,255,0) 512×512 background with a bright orange circle + a small dark "eye" dot for a visible
  motion/identity reference point. Saved to `ComfyUI/input/t0257_smoke_orb.png`.
- Prompt: `"the object sways gently"` (positive); short generic negative.
- Resolution: 480×480 (small, "480p-ish" per instructions, and a multiple of 16 as WAN requires).
- Length: 33 frames, batch_size 1.
- Sampler: Lightning's confirmed 4-step split (2 steps high-noise + 2 steps low-noise), euler/simple,
  cfg 1.0, shift 5.0 — exactly the report's "4-step Lightning" recipe, no deviation.

### A dependency bug found + fixed along the way

First submission attempt failed fast (33s) with `ImportError: Please make sure sentencepiece and
protobuf are installed` from `CLIPLoaderGGUF`'s tokenizer reconstruction — even though
`pip install -r ComfyUI-GGUF/requirements.txt` had reported both as "already satisfied." Root
cause: the server is launched with `python_embeded\python.exe -s ...` (`-s` = isolate from the
per-user site-packages, standard/recommended for a clean isolated install). `pip install` (run
*without* `-s`) had found `protobuf` already importable from the machine-wide per-user directory
`C:\Users\ROG\AppData\Roaming\Python\Python312\site-packages` and skipped installing a copy into
the embedded env's own `site-packages` — so it was invisible at runtime under `-s`. Fixed with
`python_embeded\python.exe -m pip install --force-reinstall --no-deps sentencepiece protobuf`,
which forces a real copy into the embedded interpreter's own `Lib\site-packages`, verified via
`python_embeded\python.exe -s -c "import sentencepiece; import google.protobuf"` before retrying.
This is a general trap for this isolated-embedded-Python setup: **any pip install that reports
"already satisfied" should be double-checked to confirm the satisfying copy is inside
`python_embeded\Lib\site-packages`, not a global per-user location**, since `-s` will only see the
former at runtime.

### Result: SUCCESS

Two full end-to-end runs, both `status: success`, no OOM, no CUDA errors:

| Run | Seed | Wall clock (ComfyUI-reported) | What was cold/warm |
|---|---|---|---|
| Cold | 42 | **217.94 s** (~3.63 min) | First run after server (re)start: text-encoder GGUF load + tokenizer rebuild, both GGUF unets read from disk, VAE load, then 2+2 sampling steps + decode. |
| Warm (same seed, fully cached) | 42 | 0.02 s | ComfyUI's node-level result cache short-circuited the identical prompt entirely — not a real timing signal, noted for completeness. |
| Warm (new seed 43, forces re-execution) | 43 | **103.49 s** (~1.72 min) | CLIP/VAE/loaders stayed resident from the prior run (nodes 1-12 cache-hit); only the two GGUF unets needed re-loading onto the GPU (fast — OS page cache warm, no cold disk read) plus a fresh 2+2 sampling pass. This is the realistic "iterate with a different seed/prompt on an already-warm server" number. |

Both land squarely inside the community report's ~2-4 min/clip Lightning-accelerated target (R3
in the T0256 synthesis).

**Output files:** `ComfyUI/output/t0257_smoke_00001_.mp4` (215,215 B, run 1/cold) and
`t0257_smoke_00002_.mp4` (220,634 B, run 2/warm-seed43). Both verified via PyAV: 33 frames, 16fps,
480×480, ~2.06s duration — matches the requested settings exactly.

**Motion check (visual, via extracted frames):** genuine continuous, progressive motion confirmed
frame-by-frame (not a single pop-then-freeze) — frame 0 is the exact static input, by frame ~8 the
subject is mid-transformation, by frame ~20 it has settled into a fully different composited
scene, and frames 20→26 still show small continued motion (limb/tendril repositioning, an
appendage extending). Frames saved for inspection at
`C:\projects\video_gen_experiment\smoke_frames\frame_*.png`.

**Notable and worth flagging to the lead (not a phase-2 blocker, but directly relevant to the
T0256 report's R1 "style-drift" risk):** the model did NOT lightly animate the flat green+orange
test shape — it aggressively reinterpreted/hallucinated it into an elaborate detailed
sunflower/creature-like composition with hallucinated pseudo-text, and turned the flat green
background into a blurred photoreal grass/foliage bokeh. This is a vivid, concrete illustration of
exactly the R1 risk the community report flagged (WAN 2.2 trained on photoreal video content can
"photoreal" a stylized/abstract source). It does NOT block this phase's gate (video landed, clear
motion, no OOM) — but it is strong first-hand evidence for the R1 experiment that phase 3 is
supposed to run properly on the lead's actual stylized sprite art, not a placeholder for it.

**VRAM evidence (no OOM across either run):** boot log shows `Total VRAM 12282 MB`. During
generation each GGUF unet reports `loaded completely; ~8879 MB usable, ~8475 MB loaded, full load:
True`. Post-generation `/system_stats` (models still resident) showed `vram_free: 4,038,390,586`
(~3.76 GB still free) — comfortable headroom, consistent with the report's "~6-8GB@480p" Q4
estimate. Never approached the 12GB ceiling.

## Start command (phase 2, same as phase 1)

```
cd C:\projects\video_gen_experiment\ComfyUI_windows_portable
python_embeded\python.exe -s ComfyUI\main.py --listen 127.0.0.1 --port 8188
```

Then, to fire the saved smoke workflow headlessly (no browser):
```
curl -s -X POST http://127.0.0.1:8188/prompt -H "Content-Type: application/json" --data-binary @C:\projects\video_gen_experiment\smoke_workflow_api.json
```
Output video lands in `ComfyUI/output/t0257_smoke_*.mp4`. A seed-43 variant
(`smoke_workflow_api_warm.json`, same folder) is also saved — useful to force a genuine
re-execution instead of a no-op cache hit when re-running the identical graph.

**IMPORTANT — embedded-Python dependency trap:** if you ever re-run `pip install` for a custom
node's requirements and it reports a package "already satisfied," verify the satisfying copy is
actually inside `ComfyUI_windows_portable\python_embeded\Lib\site-packages` (e.g.
`python_embeded\python.exe -s -c "import <pkg>"`, using `-s` to match how the server itself is
launched) — pip can silently be satisfied by a global per-user site-packages directory that `-s`
hides at runtime. See the phase-2 "dependency bug" note below for the concrete case this bit us.

## Phase 2 disk usage

Models: **~23.86 GB** (6 files, see table above — exact byte counts verified via `curl -I -L`
before download and re-verified byte-exact after). ComfyUI-GGUF node clone: negligible (<1MB, no
weights). **Experiment folder total after phase 2: ~28 GB** (`du -sh`), comfortably inside the
combined phase1+phase2 budgets (12GB + 25GB = 37GB ceiling). ~68 GB still free on C: afterward.

## Phase 2 scope boundary (historical note)
Phase 3 (R1 style-drift / R2 alpha-on-glow / R3 latency-ladder on the real wings fixture) was a
separate packet, run after the lead reviewed the smoke numbers above — see below.

---

# Phase 3 — R1/R2/R3 GO/NO-GO gate on the real angel-wings fixture (2026-07-04)

**Full report:** `C:\projects\game-67-idle\tmp\t0257_phase3_report.md` (verdict table, timings,
image references, GO/NO-GO recommendation). This section is a condensed pointer.

**Fixture:** a read-only copy of the angel-wings RGBA sprite from canvas project
`benchmark-fixture-c7f9dc` (element `el_f3832532`), composited onto flat green `#00FF00` at
480x480 — `phase3/fixture/wings_green_480.png`. The source canvas project was only read
(`cli.mjs show --json` + a filesystem copy), never mutated.

## Verdicts
- **R1 (style drift): PASS.** 4 runs (3 conservative + 1 prompt-hardened), codex-vision-judged
  identity preservation vs. the original sprite: baseline runs averaged 3.33-3.50/5, the
  **prompt-hardened run averaged 4.17/5**. Nowhere near the phase-2 smoke test's "hallucinated
  creature" collapse — wings/feathers evidently have strong enough priors in WAN's training data to
  stay on-model. **Actionable finding: always use the hardening pattern** ("2d game art, flat
  colors, hand-drawn illustration, no photorealism" positive + "photorealistic, 3d render,
  photograph, ... CGI, film grain" negative) as the Track B default.
- **R2 (matting/glow): RESOLVED in T0261 — CorridorKey PASS, MatAnyone PARTIAL, key_matte
  FAIL-on-glow.** Permission granted; both neural tools built and run on the real glow frames.
  **CorridorKey** is the only tool that preserves the soft gold glow (fractional-alpha 11.6% vs
  key_matte 4.8%) *and* despills cleanly (warm gold edges, no green) → PASS, the recommended
  extractor for glow/translucent assets. **MatAnyone** gives the most temporally-stable, video-aware
  alpha (tracks sparkles) but does no despill → a bold green fringe on soft edges + harder-clipped
  glow → PARTIAL. **key_matte** confirms the documented glow-clip (muddy fringe + stray specks) →
  FAIL for glow, still fine for opaque sprites. Metric table + timings + side-by-side images:
  phase-3 report R2 section and `phase3/r2/compare/`; tools table in the "T0261" section below.
- **R3 (iteration latency): PASS.** 3 successive edits (slower / bigger amplitude / steadier) each
  complied on the **first attempt** (0 re-rolls), ~126s/edit average (~2.1 min), 377.5s total for
  all 3. Small sample (n=1/edit) — treat as an encouraging first look, not a guaranteed re-roll
  rate.

## GO/NO-GO
**GO** for the Track B pipeline's `generate → frames → sheet` stages (R1+R3 support it). The
`matte` stage is now **cleared (T0261)**: use the fast in-repo `key_matte` for **opaque/non-glow**
sprites, and **CorridorKey** for **glow/translucent** identity-critical assets (the one case
`key_matte` provably fails). CorridorKey is CC-BY-NC-SA-4.0 (README carve-out allows commercial
asset processing; no reselling/paid-API) and per-frame (mild flicker). Full reasoning + tools table
in the phase-3 report and the "T0261" section below.

## Artifacts
- `phase3/r1/` — 4 workflow JSONs, output videos (in `ComfyUI/output/t0257_r1_*.mp4`), 6 extracted
  frames per run, codex judge transcripts (`judge_*.txt`), history JSONs with timings.
- `phase3/r2/` — glow-run frames (all 33), `run_key_matte_baseline.py`, background-drift report
  (`bg_drift_report.json`), key_matte RGBA cutouts, checker-composite previews, a 3x zoomed
  wingtip-edge crop showing the glow-clipping artifact.
- `phase3/r3/` — 3 edit workflow JSONs, extracted frames per edit, history JSONs with timings.
- `tools/CorridorKey/` — pinned commit `97e55a453060745bead1befd293f6e523c4b845c` (2026-05-28); in
  T0261 the venv + weights were built and the tool run (see "T0261" section below).
  **MOVED 2026-07-07 (T0335)** to its permanent home `C:\projects\ai_studio_tools\CorridorKey`
  (studio config `corridorKeyRoot`) — it is NOT part of the deletable experiment anymore.
- `tools/MatAnyone/` — added in T0261, pinned commit `e5ddc534c1fff9bb9e54cf476095d29071b7cb4f`.
- `phase3/r2/compare/` — T0261 side-by-side montages, wingtip zooms, per-tool checker composites,
  `metrics.json`.

## Phase 3 disk usage
`phase3/` = 38 MB, `tools/` = 42 MB (source only). Experiment folder total unchanged at ~28 GB —
phase 3 added negligible disk since the heavy R2 tool installs never executed.

## Cleanup
ComfyUI server killed after all phase-3 generations; verified port 8188 down and no
`python_embeded\main.py` process remains.

---

# T0261 — R2 unblocked: glow-matte tools built + benchmarked (2026-07-04)

Install permission granted by the lead. Built and ran two neural matting tools on the 33 phase-3
glow frames and benchmarked them against the in-repo `key_matte` baseline. Full comparison (metric
table, timings, image paths) lives in `tmp/t0257_phase3_report.md` R2 section. Verdicts: **CorridorKey
PASS** (glow), **MatAnyone PARTIAL** (no despill → green fringe on glow), **key_matte FAIL-on-glow**.

## Tools table (installed under `tools/`, isolated venvs, nothing system-wide)

| Tool | Repo @ commit | Env / torch | Checkpoint | venv disk | Licence | VRAM / runtime (this box) |
|---|---|---|---|---|---|---|
| **CorridorKey** | `nikopueringer/CorridorKey` @ `97e55a4` | uv, py3.13, **torch 2.8.0+cu128** | `CorridorKey_v1.0.safetensors` 398 MB (auto-DL, HF `nikopueringer/CorridorKey_v1.0`) | **7.6 GB** | **CC-BY-NC-SA-4.0** (README carve-out: may process images for commercial projects; NO reselling / paid-API) | native 2048 backbone, no OOM on 12 GB (README rates ~10 GB); **~2.5–4.1 s/frame** (GPU, eager — see note) |
| **MatAnyone** | `pq-yang/MatAnyone` @ `e5ddc53` | venv, py3.11, **torch 2.12.1+cu126** | `matanyone.pth` 141 MB (auto-DL, GH release v1.0.0) + resnet18/50 | **4.4 GB** | **S-Lab Licence 1.0** (strictly non-commercial) | 480p, <4 GB, **~0.4 s/frame** (GPU) |
| key_matte (baseline) | in-repo `ai_studio.assets.tools.image.alpha_matte.key_matte` | studio `.venv` | n/a | n/a | in-repo | CPU, **~0.28 s/frame** |

**T0261 disk added: ~13 GB** (both venvs + checkpoints), under the 15 GB budget. Experiment folder
total now **~41 GB**; ~51 GB free on C: afterward.

## Install/run gotchas (documented for reuse)
- **CorridorKey:** `uv sync --extra cuda` (torch cu128 index) then run
  `uv run python corridorkey_cli.py --device cuda run-inference --backend torch --srgb --despill 0
  --no-despeckle --refiner 1.0 --comp --cpu-post --screen-color green --image-size 2048`. Needs a
  coarse alpha hint per frame (rough chroma key is enough) in `ClipsForInference/<shot>/AlphaHint/`.
  `torch.compile` (max-autotune) **fails on Windows** with an `OverflowError` from triton-windows,
  but the engine **gracefully falls back to eager** — outputs are valid, just uncompiled (hence the
  ~3 s/frame rather than faster). Outputs land in `ClipsForInference/<shot>/Output/{FG,Matte,Comp,
  Processed}` (FG=straight sRGB EXR, Matte=linear-α EXR, Comp=checker PNG, Processed=premult RGBA EXR).
- **MatAnyone:** full deps drag Windows-hostile C-extension packages (netifaces / pycocotools /
  cchardet → "Unable to find a compatible Visual Studio installation"). Install **core inference deps
  only + `pip install -e . --no-deps`**; that is sufficient for `inference_matanyone.py`. Invoke the
  venv python **directly** — `uv run` re-syncs the full pyproject and re-triggers the netifaces build
  failure. Run: `python inference_matanyone.py -i <frames_folder> -m <first_frame_mask.png> -o <out>
  --save_image` (from the repo root, so `hugging_face/` imports resolve).

## Cleanup (T0261)
Both tool runs were one-shot processes that exited; `nvidia-smi` shows 0 MiB used (no orphans),
ComfyUI port 8188 still down. No servers left running.

---

# T0262 — Draft/Final speed profiles (Package #1, zero-install) (2026-07-04)

Two reusable `/prompt` API workflows next to the smoke workflows, implementing the "zero-install
draft profile + same-engine draft->final ladder" from the speedup research
(`tmp/research_T0262_speedup_2026-07-05.md`, Package #1). No installs, outputs only. Same WAN 2.2
A14B GGUF stack as phase 2/3; only resolution/frames change between draft and final. The phase-3
prompt-hardening prefix (`2d game art, flat colors, hand-drawn illustration, no photorealism` +
photoreal negatives) is baked into **both** files.

## The two profiles

| File | Res | Frames | Total steps | Split (high+low) | Warm exec (this box, steady-state) |
|---|---|---|---|---|---|
| `draft_workflow_api.json` | 384x384 | 25 | 4 | 1 high + 3 low | **~35.5 s** |
| `final_workflow_api.json` | 480x480 | 33 | 4 | 1 high + 3 low | **~54 s** |

Both are `euler`/`simple`, cfg 1.0, shift 5.0, Lightning rank64 LoRAs — identical to the proven
phase-2/3 recipe except res/frames/split. Input image: `wings_green_480.png` (the phase-3
wings-on-green fixture; `WanImageToVideo` auto-resizes the start image to the target width/height,
so the same 480 input feeds both).

## Measured numbers (2026-07-04, RTX 4080 Laptop 12GB, this session)

Timing = ComfyUI `execution_start`->`execution_success` delta from `/history` (matches phase-2
method). "Warm" = models resident from a prior run, fresh seed (forces real re-execution).

- **Baseline confirmed = 4 total steps** (2 high + 2 low), NOT 8. (`smoke_workflow_api.json`
  nodes 13/14: `steps:4`, split 0->2 / 2->4.) This is the load-bearing fact for the research's §5
  projection: only res+frames give draft speedup here, not steps.
- **Draft warm (384x384/25f, 1+3):** 35.5 / 36.2 / 35.4 s across seeds 3003 / 4004 / 6006. (The
  first warm-after-cold run was a 54.2 s transitional outlier while VRAM settled post-cold-load.)
  Cold (first run after server start) = 88.1 s.
- **Final warm (480x480/33f, 1+3):** 54.2 s (repeat) / 57.8 s (first, transitioning up from draft
  res). 2+2 comparison at the same size = 53.8 s.
- **Split is timing-neutral at 4 total steps** (measured): 384 -> 1+3 = 35.4-36.2 s vs 2+2 = 35.6 s;
  480 -> 1+3 = 54.2 s vs 2+2 = 53.8 s. Biasing high-noise down does NOT cut the expert swap or save
  time at 4 total steps — the high->low swap happens exactly once regardless of the split, and the
  forward-pass count is fixed at 4. The 1+3 split is a *quality* knob, not a *speed* knob. The draft
  speedup (35.5 vs 54 s at the same warm state) comes entirely from **384-res + 25-frames** (~1.55x).
- **Baseline reconciliation:** the phase-2 "103 s warm" was the 2nd-ever run (shallow warm — the
  mid-run expert swap still paid a partial cold disk-read for the 8.75 GB low-noise GGUF). In a real
  multi-run iteration session (server up, both experts fully held in the 32 GB OS page cache) the
  *same* 480x480/33f/4-step job settles to **~54 s**. So the honest live-session baseline is ~54 s,
  and the draft profile takes that to ~35 s.
- **VRAM:** idle 184 MiB used / 11.58 GB free. Draft resident: 8570 MiB used / ~2.7 GB free. Peak
  during the heavier 480x480/33f final: **10362 MiB / 12282 MiB -> ~1.9 GB headroom, no OOM.**

## Draft quality (sanity check)

3 spread frames (t=0/0.5/1.0) from a 384 draft (seed 2002): clean, readable flap motion
(wings spread -> contract/lift -> fan wide again), wings stay on-model (no photoreal collapse
thanks to the hardening prefix), feather/gold linework legible at 384, sparkle particles animate.
No mush/ghosting. **1 high-noise step was enough to set the motion** — the draft is easily good
enough to accept/reject a motion take before committing to the final.

## How to pass a seed

Set the SAME integer on `noise_seed` of BOTH KSamplerAdvanced nodes (`"13"` high and `"14"` low) —
they must match or the two experts denoise different noise. Quick inject-and-fire with the embedded
Python (no extra deps):

```
cd C:\projects\video_gen_experiment
SEED=2002
ComfyUI_windows_portable\python_embeded\python.exe -c "import json,urllib.request; wf=json.load(open('draft_workflow_api.json')); wf['prompt']['13']['inputs']['noise_seed']=$SEED; wf['prompt']['14']['inputs']['noise_seed']=$SEED; d=json.dumps({'prompt':wf['prompt']}).encode(); urllib.request.urlopen(urllib.request.Request('http://127.0.0.1:8188/prompt',d,{'Content-Type':'application/json'}))"
```

(Swap `draft_workflow_api.json` -> `final_workflow_api.json` for the final.) Output lands in
`ComfyUI/output/t0262_draft_*.mp4` / `t0262_final_*.mp4`.

## Ladder usage (the whole point)

1. Iterate on **drafts** (~35 s each): change prompt/seed, eyeball the motion. Explore seeds
   sequentially (models stay resident — ~35 s per seed; simplest, no OOM risk).
2. When a **seed's motion is accepted**, run `final_workflow_api.json` with that **same seed** ->
   ~54 s. Same engine + locked seed means the final reproduces the draft's composition and gross
   motion at higher fidelity. Verified: seed-2002 draft vs final scored **STRONG** correspondence
   (codex-vision + eyeball) — same spread/contract/spread flap, final visibly sharper feathers.
3. Projected **edit-iteration time for the lead:** ~35 s per draft motion-try (vs the old
   ~103-126 s at 480), then one ~54 s final for the ship-quality render. Explore ~4 seeds + ship one
   in ~3 min total.

## Knobs / alternatives (documented, not the default)

- **Higher-fidelity final:** bump both KSamplers to `steps:6` (e.g. split 1/6 -> `end_at_step:1`,
  `start_at_step:1`, or 2/6). Adds ~1 forward pass; note it changes the sigma schedule so
  draft->final correspondence loosens slightly. The delivered default keeps final at 4 steps to
  maximize ladder fidelity.
- **2+2 split** (phase-3-proven, `end_at_step:2` / `start_at_step:2`) is timing-identical; use it if
  you want more high-noise motion-setting. Kept the delivered files at 1+3 per the task and because
  the 384 draft verified good with 1 high step.
- **Batch seeds:** raising `WanImageToVideo` `batch_size` to N renders N seeds (seed, seed+1, ...) in
  one graph, but at ~1.9 GB headroom on this 12 GB box it will likely OOM at 480/33 (untested) —
  sequential seeds on the warm server are the safe path here.
- **Preview upscale:** run a draft output through a 2x-AnimeSharp/RealESRGAN-anime node if you want to
  eyeball 384 detail; upscale can't add motion the low-res gen never made — fine for drafts only.

## Cleanup (T0262)
ComfyUI server (PID from `run_nvidia_gpu`) killed after all T0262 generations; `nvidia-smi` back to
idle, port 8188 down, no `python_embeded` server process left. Only new footprint: the two workflow
JSONs above + `t0262_*` output mp4s in `ComfyUI/output/`. No models or packages installed.
