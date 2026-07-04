# T0257 Phase 1 — Local video-gen BASE setup log

Executor: fast-worker. Target: C:\projects\video_gen_experiment\ (outside repo).

## GPU/driver check (2026-07-03)
- `nvidia-smi`: Driver Version 576.88, CUDA Version 12.9 (driver-reported max, not installed toolkit)
- GPU: NVIDIA GeForce RTX 4080 Laptop, 12282 MiB VRAM total, WDDM driver model
- Free disk on C: at start: ~91.5 GB (98,227,822,592 bytes free)

## Install shape decision
- Latest ComfyUI release: v0.27.0 (repo moved from comfyanonymous/ComfyUI to Comfy-Org/ComfyUI).
- 4 portable asset variants offered: amd, intel, nvidia (default), nvidia_cu126.
- FIRST ATTEMPT: downloaded+extracted `ComfyUI_windows_portable_nvidia.7z` (default nvidia build,
  2,086,299,430 bytes compressed -> 4,234,600,261 bytes extracted, ~3.94 GiB). Ships torch 2.12.0+cu130.
  Tested via embedded python: `torch.cuda.is_available()` -> **False**, with
  `cudaGetDeviceCount() returned cudaErrorNotSupported, likely using older driver`.
  Root cause: driver 576.88 reports max supported CUDA runtime 12.9 (per nvidia-smi), but default
  portable bundles a CUDA 13.0 torch build which needs a newer driver. MISMATCH — abandoning this variant.
- FALLBACK: switching to `ComfyUI_windows_portable_nvidia_cu126.7z` (the variant that ships an older,
  driver-compatible CUDA 12.6 torch build) as the task anticipated ("IF its torch matches the driver's CUDA").
  Deleting the cu130 install + archive to reclaim disk before re-downloading.

## cu126 build verified working
- Downloaded+extracted `ComfyUI_windows_portable_nvidia_cu126.7z`
  (2,025,281,292 bytes compressed -> 5,570,197,005 bytes extracted, ~5.19 GiB).
- Deleted the mismatched cu130 install + both archives after each was consumed (disk hygiene).
- Verified via embedded python:
  torch version: 2.12.0+cu126
  cuda available: True
  device name: NVIDIA GeForce RTX 4080 Laptop GPU
  vram total (GB): 11.99365234375
  torch cuda version: 12.6
- This confirms the cu126 build is the correct choice for driver 576.88 (CUDA 12.9 ceiling).

## ComfyUI-Manager installed
- `git clone https://github.com/Comfy-Org/ComfyUI-Manager.git` into `ComfyUI/custom_nodes/ComfyUI-Manager`
  (repo also moved orgs: ltdrdata/ComfyUI-Manager -> Comfy-Org/ComfyUI-Manager). Clean clone, no TLS issues.
- `python_embeded\python.exe -m pip install -r ComfyUI-Manager/requirements.txt` -> "Successfully installed"
  (GitPython, PyGithub, matrix-nio, jsonschema, etc.), all isolated inside the embedded python's
  site-packages. No system pip touched.

## Smoke test (headless boot, 2026-07-03 21:22)
- Launched: `python_embeded\python.exe -s ComfyUI/main.py --listen 127.0.0.1 --port 8188`
- Boot log confirmed: `Total VRAM 12282 MB, total RAM 32387 MB`, `pytorch version: 2.12.0+cu126`,
  `Device: cuda:0 NVIDIA GeForce RTX 4080 Laptop GPU : cudaMallocAsync`, `Set vram state to: NORMAL_VRAM`.
  ComfyUI-Manager prestartup loaded cleanly in 1.8s, no errors. One benign warning: "You need pytorch
  with cu130 or higher to use optimized CUDA operations" (comfy_kitchen fused-kernel fast path disabled,
  falls back to eager backend automatically — not a blocker).
- Server printed "Starting server" / "To see the GUI go to: http://127.0.0.1:8188".
- `curl http://127.0.0.1:8188/system_stats` -> `pytorch_version: "2.12.0+cu126"`, `embedded_python: true`,
  devices: `{"name": "cuda:0 NVIDIA GeForce RTX 4080 Laptop GPU : cudaMallocAsync", "type": "cuda",
  "vram_total": 12878086144}` (~12 GB). Confirms real CUDA mode, not CPU fallback.
- `curl http://127.0.0.1:8188/` -> HTTP 200.
- Killed the server process afterward. Verified: port 8188 no longer answers, no
  `python_embeded\python.exe` process remains running. Clean shutdown, nothing left running.

## TLS / workarounds needed
- None. `curl -L --ssl-no-revoke` worked for both GitHub release downloads; `git clone` over https
  worked without `--ssl-no-revoke`/`--trusted-host` workarounds; pip install worked without issue.
  (Avast MITM did not surface a problem this session — noting in case it does on a future run.)

## Disk usage (final)
- `C:\projects\video_gen_experiment\` total: **~5.7 GB** (budget was <=12 GB — well under).
- No archives left on disk (both .7z files deleted after their extraction was verified working).

## Deliverables
- `C:\projects\video_gen_experiment\README.md` written: what this is, start/stop commands, wholesale
  delete instructions, versions, disk usage, smoke-test evidence, known cu126-vs-cu130 caveat.

## Plan — ALL DONE
1. [x] GPU check passed
2. [x] Download+extract default nvidia portable -> torch/CUDA mismatch found, reverted
3. [x] Download+extract nvidia_cu126 portable -> CUDA confirmed working, 4080 detected
4. [x] Install ComfyUI-Manager into custom_nodes
5. [x] Smoke test full server boot, verified CUDA + VRAM via /system_stats, shut down cleanly
6. [x] Write README.md
7. [x] Report disk usage (~5.7 GB)

## Start command for phase 2
```
cd C:\projects\video_gen_experiment\ComfyUI_windows_portable
python_embeded\python.exe -s ComfyUI\main.py --listen 127.0.0.1 --port 8188
```
(or `run_nvidia_gpu.bat` in that folder). Phase 2 (model selection + install) is a separate task —
this phase intentionally installed NO video models.

---

# PHASE 2 — models + smoke generation (2026-07-04)

Coordinator accepted phase 1 and green-lit phase 2: pull the WAN 2.2 I2V GGUF stack named by the
T0256 community report and run one end-to-end I2V smoke generation via the `/prompt` HTTP API
(no browser). Same isolation laws as phase 1.

## Report read
- `C:\projects\game-67-idle\tmp\research_T0256_community_2026-07-04.md` (community-practice angle)
- `C:\projects\game-67-idle\tmp\research_T0256_SYNTHESIS.md` (Russian synthesis, lead-facing)
- Headline pick confirmed: **WAN 2.2 I2V 14B GGUF Q4 (two-expert high/low-noise split) + 4-step
  Lightning/LightX2V LoRA**, native ComfyUI workflow. Community numbers cited: ~4.6 min/clip on a
  6GB RTX 3050 with this exact recipe; "Q4 for 12GB" as standard quant guidance; without Lightning,
  10-20+ min/clip, with it ~2-4 min.

## Exact artifact resolution (via WebSearch + WebFetch + curl HEAD, 2026-07-04)
Community report named the recipe qualitatively (WAN 2.2 I2V GGUF Q4 + Lightning) but not exact
HF repo/file paths — resolved those from scratch:
- Unet GGUF: `QuantStack/Wan2.2-I2V-A14B-GGUF` (HighNoise/ and LowNoise/ subfolders, one file per
  expert per quant level). Confirmed via `curl -I -L` HEAD on the resolve URL (HF redirects through
  a signed CDN URL — `-L` required) — exact `content-length` logged in the models table below.
- ComfyUI-GGUF node: `city96/ComfyUI-GGUF` (README + source both confirm: place `.gguf` unet files
  in `ComfyUI/models/unet/`, GGUF clip/text-encoder files in `ComfyUI/models/clip/` — verified by
  reading the actual `nodes.py` source, since `UnetLoaderGGUF.load_unet()` calls
  `folder_paths.get_full_path("unet", ...)` literally (not `"unet_gguf"`/`"diffusion_models"`) even
  though the *listing* helper aliases through `unet_gguf`->`diffusion_models`. Same pattern for
  `CLIPLoaderGGUF.load_clip()` -> hardcoded `"clip"` key. Placing files in the new
  `diffusion_models`/`text_encoders` folders instead would have looked correct in the node's file
  dropdown but silently 404'd at load time — avoided by tracing the actual source instead of
  trusting docs alone.
- Text encoder: `city96/umt5-xxl-encoder-gguf`, `umt5-xxl-encoder-Q4_K_M.gguf` (quantized ok per
  packet instructions; picked over Q8_0 to save ~2.4GB of the tight 25GB budget).
- VAE: `wan_2.1_vae.safetensors` — WAN 2.2 A14B I2V uses the WAN 2.1 VAE (not the 5B TI2V model's
  `wan2.2_vae.safetensors`), confirmed via official ComfyUI docs + the Lightning workflow's own
  markdown note. Pulled from the same QuantStack repo's `VAE/` folder (same source, avoids an extra
  repo).
- Lightning LoRA: `lightx2v/Wan2.2-Lightning`, folder
  `Wan2.2-I2V-A14B-4steps-lora-rank64-Seko-V1/{high_noise_model.safetensors,low_noise_model.safetensors}`
  — this folder conveniently also ships the **official `-NativeComfy.json` workflow**, which was
  downloaded and used as the ground-truth graph (node types, wiring, default widget values) for the
  smoke workflow, rather than hand-guessing the WAN2.2 I2V graph shape.
- All licenses confirmed via HF model-card fetch: **Apache-2.0** across the board (WAN 2.2 base +
  QuantStack GGUF conversion, city96 umt5-xxl GGUF, lightx2v Lightning LoRA, city96/ComfyUI-GGUF
  node itself per its file header).

## Quant/size decisions (deviation from report, with reasoning)
- Report says "Q4" generically; picked **Q4_K_S** specifically for both experts (8.75GB each) —
  smallest true-Q4 K-quant available symmetrically for both HighNoise and LowNoise in the
  QuantStack repo (Q4_0 only existed for LowNoise, not HighNoise, in that repo's file list; using
  mismatched quant families for the two experts felt like an unnecessary risk vs. picking the
  smallest symmetric Q4 K-quant).
- Text encoder Q4_K_M (3.66GB) instead of the fp8/fp16 safetensors the official workflow doc
  suggests — "quantized ok" is explicit in the phase-2 packet, and disk budget was tight (see math
  below).
- Total model weight: 8,748,151,296 × 2 (unets) + 3,655,145,312 (text enc) + 253,815,318 (vae) +
  1,226,977,424 × 2 (loras) = **23,859,218,070 bytes (~23.86 GB)**, vs a 25GB additional-budget
  ceiling — fits with ~1.14GB margin. Verified every file's `content-length` via `curl -I -L` BEFORE
  downloading, specifically to confirm this math held before committing to the download.

## Downloads (2026-07-04)
- ComfyUI-GGUF cloned clean (pinned commit `6ea2651e7df66d7585f6ffee804b20e92fb38b8a`, 2026-01-12),
  its one dependency `gguf` resolved to 0.19.0, installed into the embedded Python, no issues.
- All 6 model files downloaded in **parallel** background `curl -L --ssl-no-revoke` processes
  (one per file) straight into their correct final `ComfyUI/models/<subfolder>/` locations with
  final filenames (no separate archive-then-move step needed, since these are single-file
  downloads, not archives).
- TLS: no workarounds needed beyond the already-standard `--ssl-no-revoke` — no Avast interference
  observed this run (same as phase 1).

All 6 downloads completed successfully in parallel; every file verified byte-exact against the
`content-length` recorded before download (no corruption/truncation). Aggregate download of
23,859,218,070 bytes (~23.86GB) across 6 parallel connections took roughly 25-30 minutes wall
clock (polled via a blocking bash loop checking combined byte totals every ~20-25s). Individual
completion order: VAE first (smallest, ~254MB), then both Lightning LoRAs (~1.23GB each), then the
text encoder (~3.66GB), then both GGUF unets last (~8.75GB each, largest files).

## Smoke workflow construction
Rather than hand-assemble the ComfyUI `/prompt` API JSON blind, fetched the official
`Wan2.2-I2V-A14B-4steps-lora-rank64-Seko-V1-NativeComfy.json` (UI-graph format) from the
lightx2v/Wan2.2-Lightning repo and parsed its `nodes`/`links` arrays directly (python) to extract
the exact 17-node graph, wiring (link source/target slots), and default widget values (steps split
2+2, shift 5.0, cfg 1.0, sampler euler/simple, fps 16, resolution 1280x720/length 81 defaults).
Cross-checked every node's expected input names against THIS install's actual node source
(`comfy_extras/nodes_wan.py::WanImageToVideo`, `nodes.py::KSamplerAdvanced/LoadImage/VAEDecode/
VAELoader/CLIPTextEncode/LoraLoaderModelOnly`, `comfy_extras/nodes_model_advanced.py::
ModelSamplingSD3`, `comfy_extras/nodes_video.py::CreateVideo/SaveVideo`) since this build uses a
newer `io.ComfyNode`/`define_schema` pattern for some nodes (video ones) vs. the legacy
`INPUT_TYPES` dict pattern for others — confirmed both styles map to the same input names used in
the official workflow, no surprises.
Swapped `UNETLoader`->`UnetLoaderGGUF` (x2) and `CLIPLoader`->`CLIPLoaderGGUF`, pointed at the GGUF
filenames above, reduced resolution to 480x480/length 33 (smoke-test tiny settings per packet
instructions, vs the workflow's 1280x720/81-frame defaults), and set the positive prompt to
"the object sways gently". Saved as `C:\projects\video_gen_experiment\smoke_workflow_api.json`.
Input image: generated locally with the embedded Python (PIL one-liner) — flat green (0,255,0)
512x512 background, bright orange circle + dark eye-dot for a visible motion reference — saved to
`ComfyUI/input/t0257_smoke_orb.png`.

## Smoke run attempt 1 — FAILED, real infra bug found (2026-07-04)
Booted server (`-s ComfyUI/main.py --listen 127.0.0.1 --port 8188`), confirmed responsive
(HTTP 200), POSTed `smoke_workflow_api.json` to `/prompt` -> accepted (`node_errors: {}`, prompt_id
`5ff72ef2-...`). Failed at execution time (33.35s) with:
```
File ".../ComfyUI-GGUF/loader.py", line 343, in gguf_tokenizer_loader
    raise ImportError("Please make sure sentencepiece and protobuf are installed.\npip install sentencepiece protobuf")
```
Root cause traced: server launched with `-s` (isolate user site-packages, the standard/recommended
flag). Earlier `pip install -r ComfyUI-GGUF/requirements.txt` reported `protobuf` "already
satisfied" — but that satisfying copy was in the **global per-user** directory
`C:\Users\ROG\AppData\Roaming\Python\Python312\site-packages`, NOT inside
`python_embeded\Lib\site-packages`. Since `pip install` (no `-s`) can see the user-site dir but the
actual server run (`-s`) cannot, pip silently skipped installing a real copy into the isolated
embedded env, leaving it invisible at runtime. Confirmed via
`python_embeded\python.exe -s -c "import google.protobuf"` -> `ModuleNotFoundError: No module
named 'google'` (reproduced the bug directly, isolating it from the ComfyUI stack itself).
**Fix:** `python_embeded\python.exe -m pip install --force-reinstall --no-deps sentencepiece
protobuf` (forces pip to actually write into the embedded env's own site-packages regardless of
what's importable elsewhere). Re-verified both imports succeed under `-s` after the fix. Documented
this as a general trap for the isolated-embedded-python setup in the README's start section, since
it will bite again on any future custom node whose deps happen to already exist in a global
per-user Python 3.12 install on this box.
Killed the broken server, relaunched clean (`comfyui_phase2_smoke_run2.log`).

## Smoke run — COLD (run 1, seed 42, 2026-07-04)
POSTed `smoke_workflow_api.json` again after the fix -> accepted, prompt_id `3b8a1f4c-...`.
Followed the boot/load chain live via log tail: VAE load -> GGUF text-encoder load (`gguf qtypes:
Q4_K (144), F32 (73), Q6_K (25)`) -> tokenizer reconstruction from GGUF metadata (`Created
tokenizer with vocab size of 256384`, after the dependency fix this now succeeds) ->
`Dequantizing token_embd.weight to prevent runtime OOM` (expected/benign GGUF-CLIP warning) -> CLIP
loaded to GPU (4661.20 MB) -> high-noise GGUF unet loaded (`gguf qtypes: F16 (694), Q4_K (356),
Q5_K (44), F32 (1)`, `loaded completely; 8885.73 MB usable, 8475.46 MB loaded, full load: True`) ->
2-step high-noise sampling (~21s) -> low-noise GGUF unet loaded (same size) -> 2-step low-noise
sampling (~21s) -> WanVAE decode -> **`Prompt executed in 217.94 seconds`** (~3.63 min). History
API confirmed `status_str: success`, `completed: True`. Output landed:
`ComfyUI/output/t0257_smoke_00001_.mp4`, 215,215 bytes. No CUDA errors, no OOM anywhere in the log.

## Smoke run — WARM #1 (identical prompt, seed 42) — cache no-op, not a real signal
Re-POSTed the exact same JSON. ComfyUI's node-level result cache short-circuited ALL 17 nodes
(`execution_cached: [1..17]`) since nothing in the graph changed and the seed was fixed ->
"Prompt executed in 0.02 seconds", no new output file. Correctly identified this as NOT a
meaningful warm-generation timing (it's a no-op cache hit, not a re-run) and did a second, proper
warm test instead (below).

## Smoke run — WARM #2 (seed 43, forces real re-execution, 2026-07-04)
Wrote `smoke_workflow_api_warm.json` = same graph with `noise_seed` changed 42->43 on both
KSamplerAdvanced nodes (nodes 13/14), everything else identical. POSTed it -> accepted, prompt_id
`a48ab6d3-...`. History showed nodes 1-12 (loaders, CLIP encode, VAE, WanImageToVideo) served from
cache (inputs unchanged) while 13-17 genuinely re-executed. Log showed both GGUF unets reloading
onto the GPU (`Requested to load WAN21` twice) but much faster than the cold run (OS page cache
warm from the first read, no CLIP/tokenizer/VAE reconstruction needed) -> 2-step high-noise
sampling (~24s) -> 2-step low-noise sampling (~20s) -> **`Prompt executed in 103.49 seconds`**
(~1.72 min, less than half the cold run). `status_str: success`. Output landed:
`ComfyUI/output/t0257_smoke_00002_.mp4`, 220,634 bytes.

## Output verification (via PyAV, embedded python — `av` package already present, no install needed)
Both output MP4s: 33 frames, 16fps, 480x480, ~2.06s duration — matches requested smoke settings
exactly. Extracted frames 0/2/5/8/12/16/20/26/32 from run 1 to `smoke_frames/frame_*.png` and
visually inspected them: frame 0 = the exact static input (flat green + orange circle + eye dot);
by frame ~8 the subject is visibly mid-transformation; by frame ~20 it has settled into a fully
different, elaborate composited scene (sunflower/creature-like form with hallucinated pseudo-text,
photoreal-ish blurred grass/foliage background replacing the flat green); frames 20->26 still show
continued small motion (limb/tendril repositioning, an appendage extending). **Confirms genuine
continuous per-frame motion, not a single pop-then-freeze** — satisfies the phase-2 gate's "visible
motion" requirement unambiguously.
Worth flagging to the lead (not a phase-2 blocker, but directly relevant to T0256's R1
"style-drift" risk): the model didn't lightly animate the flat test shape, it aggressively
reinterpreted/hallucinated it into a photoreal-ish elaborate scene. This is concrete first-hand
evidence for why phase 3's R1 experiment needs to run on the lead's ACTUAL stylized sprite art
(wings fixture etc.), not a placeholder shape — a flat abstract test image is apparently enough to
trigger heavy reinterpretation.

## VRAM evidence (no OOM either run)
Boot log: `Total VRAM 12282 MB`. Each GGUF unet during load: `loaded completely; ~8879 MB usable,
~8475 MB loaded, full load: True`. Post-generation `/system_stats` (models still resident,
mid-session, not freshly booted): `vram_total: 12,878,086,144`, `vram_free: 4,038,390,586` (~3.76GB
still free) — comfortable headroom, consistent with the T0256 report's "~6-8GB@480p" Q4 VRAM
estimate. Never approached the 12GB ceiling on either run.

## Shutdown + final verification (2026-07-04)
Killed the server process after both runs. Verified: port 8188 no longer answers (`HTTP 000`), no
`python_embeded\python.exe` process matching `main.py` remains running (only my own shell wrapper
processes matched the grep, as in phase 1 — same false-positive pattern, confirmed harmless).

## Phase 2 disk usage (final)
- 6 model files: byte-exact verified against `content-length`, totaling **23,859,218,070 bytes
  (~23.86 GB)** — budget was <=25GB additional, fits with ~1.14GB margin.
- `C:\projects\video_gen_experiment\` total after phase 2: **~28 GB** (`du -sh`), combined
  phase1+phase2 budget ceiling was 12+25=37GB — comfortably under.
- ~68 GB still free on C: afterward (started phase 2 with ~68-92GB free depending on measurement
  point; consistently well clear of running out).

## Phase 2 deliverables
- `C:\projects\video_gen_experiment\README.md` — updated with full phase-2 section: models table
  (file/size/source/license), quant-choice reasoning, smoke workflow description, dependency-bug
  writeup, timing table (cold/warm), VRAM evidence, disk usage, start commands.
- `C:\projects\video_gen_experiment\smoke_workflow_api.json` — reusable `/prompt` API payload
  (seed 42).
- `C:\projects\video_gen_experiment\smoke_workflow_api_warm.json` — same graph, seed 43, for
  forcing genuine re-execution instead of a cache no-op.
- `C:\projects\video_gen_experiment\smoke_frames\frame_*.png` — extracted evidence frames showing
  motion progression.
- This log, fully chronological, no placeholders remaining.

## Phase 2 — ALL DONE.

---

# PHASE 3 — R1/R2/R3 GO/NO-GO gate on the real angel-wings fixture (2026-07-04)

Coordinator accepted phase 2 (the hallucinated-creature smoke finding was flagged as exactly why
phase 3 exists) and green-lit phase 3: run the three gate experiments on the real fixture, produce
a GO/NO-GO verdict. Full report written to
`C:\projects\game-67-idle\tmp\t0257_phase3_report.md`; this is the chronological log.

## Fixture acquisition
- `node ai_studio/assets/canvas/cli.mjs show benchmark-fixture-c7f9dc --json` (run from repo root,
  read-only) -> located element `el_f3832532`: the alpha-cut RGBA angel-wings sprite,
  `files/ab26367bd634730f18d18a3c7634a1aea182ae24fff539957c740c5d3b5490a6.png` (1254x1254, real
  alpha variance 0-255, mean 104 — routed via key_matte->dual_plate per its metadata).
- Copied (never mutated the source project) to
  `video_gen_experiment/phase3/fixture/angel_wings_rgba.png`, resized to 480x480, composited onto
  flat green `#00FF00` and flat magenta `#FF00FF` backgrounds via embedded-python PIL ->
  `wings_green_480.png` / `wings_magenta_480.png`. Copied `wings_green_480.png` +
  `wings_magenta_480.png` into `ComfyUI/input/` for use as I2V start frames.

## R1 — style drift (4 runs)
- Built 4 workflow JSON variants (python script deep-copying `smoke_workflow_api.json`, swapping
  image/prompt/seed): run1 (seed 101, "the wings flap slowly and gently"), run2 (seed 202, same
  prompt), run3 (seed 303, "gentle floating bob"), run4-hardened (seed 101, positive prefixed with
  "2d game art, flat colors, hand-drawn illustration, no photorealism," negative =
  "photorealistic, 3d render, photograph, realistic lighting, depth of field, blurry, glossy
  plastic, CGI, film grain").
- Booted server, queued all 4 via `/prompt` back-to-back (ComfyUI's own queue serialized GPU work),
  monitored via a persistent bash-loop Monitor polling `/history/<id>` per prompt_id.
- All 4 succeeded: run1 215.4s, run2 125.6s, run3 167.0s, run4(hardened) 241.2s.
- Extracted 6 spread frames per run (indices 0,6,13,19,26,32 of 33) via PyAV.
- **Own-eyes read (before codex):** all 4 runs showed STRONG identity preservation vs the phase-2
  smoke test — clean flat green background held, wings stayed recognizable as the same painterly
  white/gold game-art sprite in every run, no hallucinated creature/text. Genuine continuous motion
  (subtle wing-angle flap) visible across frames.
- **Codex vision judging:** `codex exec -i <fixture> -i <6 frames> --output-last-message out.txt -`
  per run, scoring 1-5 identity preservation per frame with reasons. Results: run1 avg 3.33, run2
  avg 3.33, run3 avg 3.50, **run4(hardened) avg 4.17** — the hardening variant clearly won, +0.67 to
  +0.84 over the same-seed baseline (run1, same seed 101, only prompt differs -> clean A/B).
- **R1 VERDICT: PASS**, prompt-hardening pattern recommended as Track B default.

## R2 — matting/glow: BLOCKED on CorridorKey install, pivoted to available evidence
- Researched CorridorKey (github.com/nikopueringer/CorridorKey) and MatAnyone
  (github.com/pq-yang/MatAnyone) via WebFetch: CorridorKey = standalone uv-managed python app (NOT
  a ComfyUI node), ~300MB checkpoint auto-download, needs raw RGB + a *rough* alpha hint (README
  explicitly sanctions a rough chroma-key hint, no need for the heavy GVM/VideoMaMa 80GB-VRAM
  hint generators). MatAnyone = pip-installable, needs a first-frame segmentation mask.
- `git clone https://github.com/nikopueringer/CorridorKey.git` into
  `video_gen_experiment/tools/CorridorKey/` succeeded (source-only, pinned commit
  `97e55a453060745bead1befd293f6e523c4b845c`, 2026-05-28). Read its README + `corridorkey_cli.py` +
  `clip_manager.py` source directly to confirm the exact non-interactive CLI shape:
  `uv run python corridorkey_cli.py --device cuda run-inference --backend torch --srgb --despill 0
  --despeckle --refiner 1.0 --comp --gpu-post --screen-color green`, folder convention
  `ClipsForInference/<shot>/{Input,AlphaHint}/frame_XXXX.png` (confirmed via the repo's own test
  suite).
- **`uv sync --extra cuda` (the actual build+install step) was BLOCKED by the Claude Code auto-mode
  permission classifier**: *"[Untrusted Code Integration] `uv sync` builds and installs
  dependencies from CorridorKey, an external repo outside trusted source-control orgs, arranging
  untrusted external code to execute; run outside auto mode for review."* Per the denial's own
  guidance ("stop and explain... let the user decide," "should not attempt to work around this
  denial") — did NOT retry with `pip install` as a substitute, did NOT attempt MatAnyone (identical
  class of action, would hit the same gate), did NOT hand-reimplement the model. This is a policy
  decision only the lead can override (grant a Bash permission rule, or run the installer
  personally: `Install_CorridorKey_Windows.bat` is sitting there ready to double-click).
- **Pivoted to what WAS available without new external-repo execution:**
  1. Generated one glow-prompted run (hardened style, "the wings glow softly, pulsing light," seed
     101) -> succeeded 143.2s. Extracted all 33 frames.
  2. Wrote `phase3/r2/run_key_matte_baseline.py`: measures background color drift from `#00FF00`
     per frame (4-corner sampling) AND runs our own repo's production
     `ai_studio.assets.tools.image.alpha_matte.key_matte.key_matte_cutout` (in-repo/trusted code,
     no permission issue) on every frame, composited over a checker.
  3. Ran it via the studio `.venv`. Result: **mean bg drift 11.08** (range 6.96-14.27) out of ~441
     max possible -> WAN's green stayed much cleaner than the "dirty AI green screen" risk flagged
     in the synthesis doc; the per-frame *max* (~305-331) is a sampling-box artifact from the
     source art's own floating sparkle-particle decorations, not video-compression dirt.
  4. Checker composites looked decent at a glance; a 3x-zoomed crop of a wingtip edge
     (`wingtip_edge_zoom.png`) revealed the real story: a visible dark/muddy fringe at the
     feather-glow boundary — exactly the failure `key_matte`'s own docstring predicts ("soft
     fractional alpha... is mathematically unrecoverable from one background"). Concrete,
     photographed evidence for the lead's original glow complaint, but NOT a CorridorKey
     comparison — that piece stays unverified pending permission.
- **R2 VERDICT: PARTIAL/BLOCKED** (infra/permission gate, not a WAN-quality failure). Disk budget
  (<=15GB) essentially untouched (`tools/`=42MB source-only, no venv/weights ever built).

## R3 — iteration latency (3 edits on the R1 winner)
- Base: run4 (hardened prompt, seed 101, R1's highest scorer). 3 successive edits, same seed, only
  prompt text changed (matches a real "fix this" loop: identity pinned via seed, vary instruction):
  1. "slower" -> "...the wings flap very slowly, minimal gentle subtle movement, almost still" ->
     **143.4s**, frame0 vs frame32 nearly identical pose -> **compliant on attempt 1**.
  2. "bigger amplitude" -> "...large wide sweeping motion, big amplitude, dramatic full flap" ->
     **113.6s**, clear large sweep raised-spread -> lowered-inward across the clip, visibly bigger
     range than baseline -> **compliant on attempt 1**.
  3. "less camera drift/steady" -> "...static camera, wings stay centered and the same size, no
     zoom, no camera movement" -> **120.5s**, frame0 vs frame32 same framing/size/position ->
     **compliant on attempt 1**.
- **Total: 377.5s (~6.3 min) for 3 edits, avg 125.8s/edit (~2.1 min), 0 re-rolls across all 3.**
  Judged by direct visual inspection (frame0 vs frame32 comparison per edit), not codex this time
  (time-boxed; visual compliance was unambiguous in all 3 cases).
- Caveat logged: n=1 per edit, not a statistically robust re-roll-rate estimate — reported as an
  encouraging first look, not a guarantee, in the final report.
- **R3 VERDICT: PASS.**

## Shutdown + final verification
- Killed the ComfyUI server after all phase-3 generations (same PowerShell Stop-Process pattern as
  phases 1-2). Verified port 8188 down (`HTTP 000`) and no `python_embeded\main.py` process
  remains — only this session's own shell-wrapper processes matched the process-list grep (same
  benign false-positive pattern as phases 1-2, confirmed harmless).
- Canvas project `benchmark-fixture-c7f9dc` confirmed never mutated (read-only `show` + filesystem
  copy only).

## Phase 3 disk usage (final)
`video_gen_experiment/phase3/` = 38 MB. `video_gen_experiment/tools/` = 42 MB (CorridorKey source
clone only). Experiment folder total: **~28 GB** (unchanged from phase 2 — phase 3 added
negligible disk since R2's heavy tool installs never executed due to the permission block).

## Phase 3 deliverables
- `C:\projects\game-67-idle\tmp\t0257_phase3_report.md` — full verdict table, R1/R2/R3 detail
  sections, GO/NO-GO recommendation with PARTIAL scope-out, image/artifact path references.
- `C:\projects\video_gen_experiment\README.md` — updated with condensed Phase 3 section pointing
  to the full report.
- This log, fully chronological.
- All raw artifacts under `video_gen_experiment/phase3/{r1,r2,r3,fixture}/` and
  `video_gen_experiment/tools/CorridorKey/` (source only).

## Phase 3 — ALL DONE. GO/NO-GO delivered: conditional GO for generate/frames/sheet stages now,
HOLD on the CorridorKey matte-stage integration until R2 is actually unblocked and run.

---

# T0261 — R2 unblocked: build + benchmark glow-matte tools (2026-07-04)

Lead granted install permission ("разрешаю собрать и проверить"). Deep-reasoner executor. Goal:
finish R2 — can we extract clean RGBA incl. GLOW from the generated video? Build CorridorKey +
MatAnyone, run both on the 33 phase-3 glow frames, compare vs the in-repo `key_matte` baseline.

## Recon
- Confirmed: uv 0.9.5 on PATH, git, disk 64 GB free (budget ≤15 GB add'l for T0261). No conda
  (rules out MatAnyone's documented conda path — used a uv/venv instead).
- CorridorKey (pre-cloned @ `97e55a4`): uv project, py3.13, torch 2.8.0+cu128, CC-BY-NC-SA-4.0.
  Read `README.md`/`corridorkey_cli.py`/`clip_manager.py`: needs raw RGB + a *coarse* alpha hint
  per frame (rough chroma key sanctioned); `--device` is a top-level option (before the subcommand);
  non-interactive needs `--srgb --despill --despeckle --refiner` all set; outputs land in
  `ClipsForInference/<shot>/Output/{FG,Matte,Comp,Processed}` (FG straight sRGB EXR, Matte linear-α
  EXR, Comp checker PNG). Native 2048 backbone → ~10 GB VRAM regardless of input size (fits 12 GB).
- Glow frames already existed at `phase3/r2/frames/` (33). Confirmed frame 0 = wings on green with
  soft gold glow + floating sparkles = the exact soft-alpha case.

## CorridorKey install + run
- `uv sync --extra cuda` → OK (exit 0), torch 2.8.0+cu128, CUDA True, 4080 detected, **.venv 7.6 GB**.
- Staged shot: `setup_corridorkey_shot.py` (studio venv) copied 33 frames → `ClipsForInference/glow/
  Input/` and generated a green-dominance chroma hint (dilate+feather) → `AlphaHint/`.
- Ran `corridorkey_cli.py --device cuda run-inference --backend torch --srgb --despill 0
  --no-despeckle --refiner 1.0 --comp --cpu-post --screen-color green --image-size 2048`.
  Checkpoint auto-DL (398 MB safetensors). `torch.compile` max-autotune **failed** (OverflowError,
  triton-windows) but **fell back to eager** — all 33 frames written, "Clip glow Complete." Gross
  135 s / 33 fr = ~4.1 s/frame (incl load + failed-compile; steady eager ≈2.5–3 s/frame). Output
  480×480 (resized back from 2048). Benign Windows log noise: rich UnicodeEncodeError on '→' (cp1252)
  and a logging-handler emit error — neither affects results.
- `ck_exr_to_rgba.py` (in CK venv, OPENCV_IO_ENABLE_OPENEXR=1) → straight RGBA PNGs from FG+Matte.

## MatAnyone install + run
- Cloned `pq-yang/MatAnyone` @ `e5ddc53`. Deps include Windows-hostile C-extension pkgs
  (netifaces/pycocotools/cchardet → "Unable to find a compatible Visual Studio installation").
  Installed **core inference deps only** (torch 2.12.1+cu126, torchvision, opencv, hydra-core,
  omegaconf, einops, imageio, tqdm, huggingface_hub, safetensors, easing_functions) **+
  `pip install -e . --no-deps`**. **.venv 4.4 GB.** Trap found: `uv run` re-syncs the full pyproject
  → re-hits the netifaces build failure; must call the venv python **directly**.
- First-frame binary mask from the chroma key → `matanyone_firstframe_mask.png`. Ran
  `inference_matanyone.py -i phase3/r2/frames -m <mask> -o matanyone_out --save_image` from repo root.
  Auto-DL matanyone.pth (141 MB) + resnet18/50. **36 s total** (incl downloads+load); steady
  ~0.4 s/frame at 480p. Output: `matanyone_out/frames/{pha,fgr}/` (33 each) + `*_pha.mp4`/`*_fgr.mp4`.
  Observed: pha = crisp/stable alpha tracking the sparkles; fgr = **no despill** (green background
  fully retained), so soft edges keep green spill.

## Comparison + verdicts
- `run_key_matte_baseline.py` re-run for timing: 9.08 s / 33 = **275 ms/frame** (CPU).
- `compare_r2.py` (studio venv): composited all three tools' straight RGBA over one shared dark
  checker → `phase3/r2/compare/{key_matte,corridorkey,matanyone}/`; built 4-up side-by-sides
  (frames 0/8/16/24/32), 3× wingtip zooms (0,16), and `metrics.json`.
- Metrics: soft-α% key_matte 4.75 / **CK 11.61** / MA 7.50; edge-greenness key_matte −24.2 /
  **CK −29.6 (clean gold)** / **MA +78.5 (green fringe)**; temporal-α-std key_matte 13.2 / CK 10.0 /
  **MA 9.39 (stablest, video-aware)**; bg-flicker key_matte 1.016 / CK 0.897 / **MA 0.665**.
- Visual (side-by-side + wingtip zoom): CK = clean soft gold glow + despilled edges + clean bg;
  MA = crisp stable matte but bold GREEN RIM on every feather edge; key_matte = muddy dark/colored
  specks + clipped glow. Consistent across frames 0 and 16.
- **Verdicts: CorridorKey PASS, MatAnyone PARTIAL (no despill on glow), key_matte FAIL-on-glow.**

## Disk / cleanup
- T0261 added ~13 GB (CK 7.6 + MA 4.4 + checkpoints 0.54). Experiment folder ~41 GB; ~51 GB free.
- Both tool runs were one-shot processes that exited. `nvidia-smi` = 0 MiB used (no orphans), no
  python/comfy processes, ComfyUI port 8188 down. Repo untouched except this log + the phase-3
  report (both gitignored tmp).

## T0261 — ALL DONE. Recommendation: route glow/translucent identity-critical assets through
CorridorKey (only tool that keeps soft glow + despills); keep key_matte for opaque sprites; hold
MatAnyone in reserve for hard-edged subjects needing max temporal stability (needs an added despill
for green-screen glow).

---

# T0262 — Draft/Final speed profiles applied (Package #1, zero-install) (2026-07-04)

Applied the ranked speedup research (`tmp/research_T0262_speedup_2026-07-05.md`) Package #1 on the
isolated experiment. No installs, outputs only, repo untouched except this gitignored log. GPU free,
sole owner of the folder. Same WAN 2.2 A14B GGUF stack — only res/frames change between profiles.

## What was built
- `draft_workflow_api.json` — 384x384, 25 frames, 4 total steps, split 1 high + 3 low, seed on
  nodes 13/14, hardened prompt baked in, prefix `t0262_draft`.
- `final_workflow_api.json` — 480x480, 33 frames, 4 total steps, split 1 high + 3 low, seed passed
  in, same hardened prompt, prefix `t0262_final`.
- Both reuse the phase-3 `wings_green_480.png` input and the phase-3 hardening prefix/negatives.

## Baseline step count (the fact the research needed)
- **Current run = 4 TOTAL steps (2 high + 2 low), not 8.** `smoke_workflow_api.json` nodes 13/14:
  `steps:4`, split 0->2 / 2->4. => per research §5, at 4 steps the draft speedup comes only from
  res+frames, and the "bias high-noise down to cut the swap" lever has no headroom (see below).

## Measured (ComfyUI execution_start->success delta, warm = models resident + fresh seed)
- Draft warm 384/25 (1+3): 35.5 / 36.2 / 35.4 s (seeds 3003/4004/6006). First warm-after-cold =
  54.2 s transitional. Cold = 88.1 s.
- Final warm 480/33 (1+3): 54.2 s (repeat), 57.8 s (first, ramping from draft res).
- Split timing-neutrality PROVEN: 384 1+3=35.4-36.2 vs 2+2=35.6; 480 1+3=54.2 vs 2+2=53.8. At 4
  total steps the high/low split does NOT change forward-pass count (fixed 4) or swap count (exactly
  1). "Bias high-noise down to cut the expert swap" = no speed effect here; it's a quality knob only.
- Baseline reconciliation: phase-2 "103 s warm" was the 2nd-ever run (partial cold disk-read on the
  mid-run 8.75 GB low-noise expert swap). Deep-warm steady state for the SAME 480/33/4-step job =
  ~54 s (both experts held in the 32 GB OS page cache). Draft profile -> ~35 s.
- VRAM: idle 184 MiB used / 11.58 GB free; draft resident 8570 MiB; peak during 480/33 final
  10362/12282 MiB -> ~1.9 GB headroom, no OOM.

## Draft quality + ladder
- Draft quality (3 frames, seed 2002, 384): clean readable flap (spread->contract->fan), on-model
  wings (no photoreal collapse), legible feather/gold linework, animating sparkles, no mush/ghost.
  1 high-noise step sufficient. Verdict: good enough to accept/reject motion.
- Ladder (seed 2002 draft -> seed 2002 final): codex-vision + eyeball = **STRONG** correspondence.
  Same gross flap + composition; final visibly higher fidelity (sharper feathers, cleaner edges).
  Minor: draft t=1.0 has slightly heavier particles / more outward stretch.
- Projected lead edit-iteration: ~35 s per draft motion-try (vs old ~103-126 s at 480), then one
  ~54 s final. Explore ~4 seeds + ship one in ~3 min.

## Cleanup (T0262)
ComfyUI server killed; nvidia-smi idle, port 8188 down, no server process. Only new footprint: 2
workflow JSONs + t0262_* mp4s in ComfyUI/output/. No models/packages installed. Disk delta negligible.
