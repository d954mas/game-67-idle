# T0257 Phase 3 — GO/NO-GO gate report (R1/R2/R3 on the real wings fixture)

Date: 2026-07-04. Executor: fast-worker (same session as phases 1-2). All work under
`C:\projects\video_gen_experiment\` (isolated, wholesale-deletable). Fixture: read-only copy of
the angel-wings sprite from canvas project `benchmark-fixture-c7f9dc`
(`files/ab26367bd634730f18d18a3c7634a1aea182ae24fff539957c740c5d3b5490a6.png`, the alpha-cut RGBA
element `el_f3832532`), composited onto flat green/magenta at 480x480 —
`video_gen_experiment/phase3/fixture/wings_green_480.png`. The source canvas project was never
mutated (read-only `show`, then a filesystem copy).

## Verdict table

| Experiment | Verdict | One-line reason |
|---|---|---|
| **R1 — style drift** | **PASS** (hardened-prompt variant strongly recommended as default) | Codex-judged identity preservation 3.3-3.5/5 on baseline conservative prompts, **4.17/5** on the prompt-hardened variant — nothing close to the phase-2 smoke test's "hallucinated creature" collapse. Silhouette/style stayed recognizable in all 4 runs. |
| **R2 — matting/glow** | **RESOLVED in T0261 → CorridorKey PASS, MatAnyone PARTIAL, key_matte FAIL-on-glow** | Install permission was granted; both neural tools were built and run on the real glow frames. **CorridorKey** is the only tool that both preserves the soft gold glow (fractional-alpha 11.6% vs key_matte 4.8%) and despills cleanly (warm gold edges, no green) — visually the best, PASS. **MatAnyone** gives the most temporally-stable, sparkle-tracking alpha (video-aware) but does **no** color-unmixing/despill → a visible green fringe on every soft edge and a harder-clipped glow → PARTIAL for the glow case. The in-repo **key_matte** baseline confirms the documented glow-clip (muddy dark fringe + stray specks) → FAIL for glow, still fine for opaque sprites. See the R2 detail section below for the metric table, timings, and side-by-side image paths. |
| **R3 — iteration latency** | **PASS** | 3 successive realistic edits (slower / bigger amplitude / steadier-no-drift) each **complied on the first attempt** (0 re-rolls), at 113.6-143.4s per iteration (avg ~126s / ~2.1 min). Small sample (n=1 per edit) — a real usage pattern would need more trials to pin down the true re-roll rate, but the first-look signal is good. |

## GO/NO-GO recommendation

**Conditional GO.** Build the Track B pipeline's `generate` → `frames` → `sheet` stages now — R1
and R3 both support it: WAN 2.2 I2V GGUF Q4 + Lightning preserves 2D game-art identity well enough
to be usable (especially with the prompt-hardening pattern below), and the edit-and-regenerate
loop is fast enough (~2 min/iteration) and reliable enough (no re-rolls needed in this sample) to
beat the "gacha lottery" framing the lead dislikes. **The `matte` stage's neural-unmixer is now
CLEARED (T0261):** CorridorKey was built and run on the real glow frames and its glow claim
verified — it is the recommended extractor for **glow / translucent identity-critical assets**
(the lead's original "matte doesn't work with glow" complaint), the one case `key_matte` provably
cannot handle. **Scope now:** keep the fast in-repo `key_matte` for **opaque / non-glow** sprites
(cheap, ~0.28s/frame, no license constraint); route **glow/translucent/soft-edge** assets through
**CorridorKey** (~2.5-4s/frame at 2048 on the 4080, clean gold glow + despill). **Two caveats to
carry into the integration:** (1) CorridorKey is licensed **CC-BY-NC-SA-4.0** — its README grants a
carve-out to *process images for commercial projects* but forbids repackaging/reselling the tool or
a paid inference API; acceptable for internal asset production, flag it for the lead's records.
(2) CorridorKey is a **per-frame** keyer (mild temporal flicker across a clip); MatAnyone is
temporally stabler but green-fringes glow — if a glow asset ever needs a flicker-free *video* matte,
the hybrid to prototype is MatAnyone-alpha + a despill/soft-edge pass, or light multi-frame alpha
smoothing on CorridorKey output.

---

## R1 — style drift (detail)

### Setup
Fixture on flat green, 480x480, 33 frames, WAN 2.2 I2V GGUF Q4_K_S + 4-step Lightning (same
config phase 2 validated). 4 runs:

| Run | Seed | Prompt | Wall clock |
|---|---|---|---|
| run1 | 101 | "the wings flap slowly and gently" | 215.4 s |
| run2 | 202 | "the wings flap slowly and gently" (seed variation) | 125.6 s |
| run3 | 303 | "gentle floating bob" | 167.0 s |
| run4 (hardened) | 101 | "2d game art, flat colors, hand-drawn illustration, no photorealism, the wings flap slowly and gently" / negative: "photorealistic, 3d render, photograph, realistic lighting, depth of field, blurry, glossy plastic, CGI, film grain" | 241.2 s |

All workflow JSONs: `video_gen_experiment/phase3/r1/workflow_*.json`. Outputs:
`ComfyUI_windows_portable/ComfyUI/output/t0257_r1_*.mp4`. Extracted frames (6 per run, spread
across the clip): `video_gen_experiment/phase3/r1/<run>/frame_*.png`.

### Judging method
Codex CLI vision (`codex exec -i <fixture> -i <6 frames> --output-last-message out.txt -`),
scoring identity preservation 1-5 per frame against the original fixture as ground truth (5 =
near-identical style/linework/silhouette, 3 = recognizable but noticeable drift, 1 = hallucinated a
different subject/style). Raw judge output: `video_gen_experiment/phase3/r1/judge_*.txt`.

| Run | Frame scores (0/6/13/19/26/32) | Average |
|---|---|---|
| run1 | 4,4,3,3,3,3 | 3.33 |
| run2 | 4,4,3,3,3,3 | 3.33 |
| run3 | 4,4,4,3,3,3 | 3.50 |
| **run4 (hardened)** | **5,4,4,4,4,4** | **4.17** |

### My own read (eyes-on, per instructions)
Confirms codex: all 4 runs kept the wings clearly recognizable as the same painterly white/gold
game-art sprite — silhouette, general feather-layer structure, and gold rim light all survived.
The degradation codex flagged is real but modest: softened/smoothed feather linework, some loss of
the small floating sparkle-particle detail, and mild silhouette "breathing" as the wings pose
changes. **Nothing like the phase-2 smoke test's outcome**, where an abstract flat-color shape got
aggressively reinterpreted into a hallucinated creature with fake text. The difference is almost
certainly that "wings/feathers" is a well-represented concept in WAN's training data with a strong
enough prior to *stay on-model*, whereas the smoke test's improvised abstract shape had no such
prior and got filled in with generic photoreal content instead.

Frame comparisons worth a direct look: `video_gen_experiment/phase3/r1/run1_seed101/frame_019.png`
vs `.../run4_hardened_seed101/frame_019.png` — the hardened run visibly holds crisper feather edges
and a cleaner background.

### Actionable takeaway
**Always use the prompt-hardening pattern** ("2d game art, flat colors, hand-drawn illustration, no
photorealism" in positive + "photorealistic, 3d render, photograph, ... CGI, film grain" in
negative) as the Track B default — it was a free +0.67 to +0.84 average-score improvement with no
other change (same seed as run1, directly comparable).

---

## R2 — matting / glow extraction (detail)

### T0261 RESULT — install permission granted, both tools built + run for real

**Fixture:** the 33 glow frames from the phase-3 wings-with-glow WAN run
(`video_gen_experiment/phase3/r2/frames/frame_000..032.png`; raw `#00FF00` green plates of the
angel wings with a soft gold glow halo + floating sparkle particles — the exact soft-alpha case).

**What was installed (isolated under `video_gen_experiment/tools/`, own venvs, nothing system-wide):**

| Tool | Repo / commit | Env | Checkpoint | Disk (venv) | VRAM (this run) |
|---|---|---|---|---|---|
| **CorridorKey** | `nikopueringer/CorridorKey` @ `97e55a4` | uv, py3.13, torch **2.8.0+cu128** | `CorridorKey_v1.0.safetensors` 398 MB (auto-DL from HF) | 7.6 GB | ran at native 2048 backbone, no OOM on 12 GB (README rates ~10 GB) |
| **MatAnyone** | `pq-yang/MatAnyone` @ `e5ddc53` | venv, py3.11, torch **2.12.1+cu126** | `matanyone.pth` 141 MB (auto-DL from GH release) + resnet18/50 | 4.4 GB | 480p, small (<4 GB), no OOM |

T0261 added **~13 GB** total (under the 15 GB budget). MatAnyone install note: its full deps pull
Windows-hostile C-extension packages (netifaces/pycocotools/cchardet — Visual-Studio build fail);
installed **core inference deps only + `pip install -e . --no-deps`**, which is sufficient (demo/
training deps unused). Run its venv python **directly** — `uv run` re-triggers a full-pyproject sync
and the netifaces build failure.

**Inputs each tool needs (both satisfied without the heavy optional hint-generators):**
- CorridorKey: raw RGB + a *coarse* alpha hint per frame (README: "does not need to be precise…
  rough chroma key"). Generated a green-dominance chroma hint, dilated+feathered, into
  `ClipsForInference/glow/AlphaHint/`. Ran `--srgb --despill 0 --no-despeckle --refiner 1.0 --comp
  --cpu-post --screen-color green --image-size 2048`.
- MatAnyone: a single first-frame binary mask (`phase3/r2/matanyone_firstframe_mask.png`, from the
  same chroma key). Propagates it through the clip via its memory network.

**Quantitative comparison** (`phase3/r2/compare/metrics.json`; 33 frames, composited over one shared
dark checker):

| Metric | key_matte (baseline) | **CorridorKey** | MatAnyone |
|---|---|---|---|
| Soft/fractional-alpha % (↑ = softer glow) | 4.75 | **11.61** | 7.50 |
| Edge greenness (−=despilled gold, +=green spill) | −24.2 | **−29.6 (clean gold)** | **+78.5 (green fringe)** |
| Temporal α std, all-px (↓ = stabler) | 13.20 | 10.03 | **9.39** |
| Bg flicker, mean abs-diff (↓ = stabler) | 1.016 | 0.897 | **0.665** |
| Runtime / frame | ~0.28 s (CPU) | ~2.5–4.1 s @2048 (GPU) | ~0.4 s (GPU) |

Runtime notes: key_matte 9.08 s / 33 fr (studio `.venv`, CPU). CorridorKey 135 s / 33 fr **gross**
including model load + a failed `torch.compile` (max-autotune hit an `OverflowError` from
triton-windows and **gracefully fell back to eager** — results valid, just uncompiled; steady-state
eager ≈2.5–3 s/frame). MatAnyone 36 s total incl 135 MB+resnet downloads + load → steady-state
≈0.4 s/frame at 480p.

**Verdicts for the Track B matte stage:**
- **CorridorKey — PASS.** Only tool that preserves the soft gold glow AND despills. Highest
  fractional-alpha (11.6%), clean warm-gold edges (no green), sparkles kept as soft dots. Weakness:
  per-frame (mild flicker, middle of the three), slow (~3 s/frame @2048), CC-BY-NC-SA-4.0 licence.
- **MatAnyone — PARTIAL.** Best temporal stability (video-aware memory net — lowest flicker, tracks
  sparkles smoothly) and fast, BUT does **no** color-unmixing → a bold **green fringe** on every
  soft edge (+78.5 greenness) and clips the soft glow harder. Great object matte, wrong tool for
  glow-color without an added despill. S-Lab Licence 1.0 (strictly non-commercial).
- **key_matte (baseline) — FAIL on glow** (confirms the lead's original complaint): clips glow
  (4.75% soft), muddy dark fringe + stray specks, least temporally stable. Keep it for **opaque**
  sprites (fast, in-repo, no licence issue).

**Image evidence (all under `video_gen_experiment/phase3/r2/`):**
- `compare/sidebyside_frame{000,008,016,024,032}.png` — 4-up [input | key_matte | CorridorKey |
  MatAnyone] over the shared dark checker.
- `compare/wingtip_zoom_frame{000,016}.png` — 3× wingtip zoom; the money shot: CorridorKey clean gold
  glow, MatAnyone a distinct green rim, key_matte muddy specks.
- `compare/{key_matte,corridorkey,matanyone}/frame_*.png` — per-tool checker composites (all 33).
- `corridorkey_rgba/` — CorridorKey straight-RGBA PNGs (from FG+Matte EXR).
- CorridorKey native EXR outputs: `tools/CorridorKey/ClipsForInference/glow/Output/{FG,Matte,Comp,Processed}/`.
- MatAnyone outputs: `matanyone_out/frames/{pha,fgr}/` + `matanyone_out/*.mp4`.
- Scripts: `setup_corridorkey_shot.py`, `ck_exr_to_rgba.py`, `compare_r2.py`, `run_key_matte_baseline.py`.

**One-line recommendation:** route glow/translucent identity-critical assets through **CorridorKey**
(the only tool that keeps soft glow + despills), keep `key_matte` for opaque sprites, and hold
MatAnyone in reserve for hard-edged subjects where temporal stability matters more than glow color.

**Cleanup:** both tool processes exited; `nvidia-smi` shows 0 MiB used (no orphans), ComfyUI port
8188 down.

---

### Historical (pre-T0261, now RESOLVED): what was originally blocked

### What was attempted vs. what ran

**Blocked:** `uv sync --extra cuda` inside the freshly-cloned `video_gen_experiment/tools/CorridorKey/`
was denied by the harness's auto-mode permission classifier: *"[Untrusted Code Integration] `uv
sync` builds and installs dependencies from CorridorKey, an external repo outside trusted
source-control orgs, arranging untrusted external code to execute; run outside auto mode for
review."* The classifier is explicit that a coordinator/teammate relay message does not establish
the human lead's consent to build and execute a new third-party repository — only the lead
directly, or an explicit permission-system grant, can authorize that. Per the tool's own guidance
("stop and explain... let the user decide"), this was **not** worked around (no retry with plain
`pip install` instead of `uv sync`, no attempt to hand-implement the model). MatAnyone
(`pip install git+https://github.com/pq-yang/MatAnyone`) was not even attempted, since it is the
identical class of action and would almost certainly hit the same gate.

**To unblock in a future session, the lead needs to do ONE of:**
1. Add a Bash permission rule allowing `uv sync` / `pip install` for these two specific external
   repos (see Claude Code settings — the denial message names this path), or
2. Run `Install_CorridorKey_Windows.bat` inside `video_gen_experiment/tools/CorridorKey/`
   personally (it is already cloned there, pinned at commit `97e55a453060745bead1befd293f6e523c4b845c`,
   2026-05-28), or install MatAnyone personally, then hand the session back, or
3. Give the instruction directly (not via a relayed coordinator message) in a fresh session.

**What CorridorKey actually needs, confirmed by reading its README/CLI (source-only reading is not
blocked, only build+execute is):** two inputs per frame — the raw green-screen RGB, and a *rough*
black/white alpha hint (README explicitly: "This does not need to be precise. It can be generated
by you with a rough chroma key..."). Non-interactive CLI:
`uv run python corridorkey_cli.py --device cuda run-inference --backend torch --srgb --despill 0
--despeckle --refiner 1.0 --comp --gpu-post --screen-color green`, reading from a
`ClipsForInference/<shot>/Input/` + `ClipsForInference/<shot>/AlphaHint/` folder pair (matching
filenames). Model checkpoint auto-downloads (~300MB) on first run — small, no disk-budget concern.
GVM/VideoMaMa (the two "automatic hint" backends) need 80GB VRAM and were never in scope; the plan
was always the cheap rough-chroma-hint path the README itself endorses as sufficient — this part of
the plan needed **no extra heavy model**, just execution permission.

### What WAS run and measured (no external-repo execution needed)

**Glow-prompted generation:** one run, hardened-prompt style, "the wings glow softly, pulsing
light," seed 101, 480x480/33 frames — succeeded in 143.2s. Output:
`ComfyUI_windows_portable/ComfyUI/output/t0257_r2_glow_seed101_00001_.mp4`. All 33 frames
extracted to `video_gen_experiment/phase3/r2/frames/`.

**Dirty-green tolerance (measured, not attempted-and-blocked):** sampled the 4 corners of every
frame (nominal pure background) and measured Euclidean RGB distance from `#00FF00`.

- Mean drift across the clip: **11.08** (range 6.96-14.27 per frame) out of a max possible ~441.
- Max drift per frame: **~305-331**, but this is a known confound — the source art has small
  floating sparkle-particle decorations near the wingtips that occasionally fall inside the
  8%-corner sampling box and spike the max; the *mean* (dominated by the actual flat-green area) is
  the meaningful number.
- **Conclusion: WAN's generated green stayed much cleaner than the "dirty AI green screen" risk the
  synthesis report worried about** — this specific I2V setup does not obviously need CorridorKey's
  screen-color auto-detection/despill robustness to survive; that's a genuinely reassuring,
  unexpected finding. Full per-frame data: `video_gen_experiment/phase3/r2/bg_drift_report.json`.

**Baseline extractor (our own repo's production `key_matte`, in-repo/trusted code — no permission
issue):** ran `ai_studio.assets.tools.image.alpha_matte.key_matte.key_matte_cutout` on all 33
frames via the studio `.venv`. RGBA cutouts: `video_gen_experiment/phase3/r2/key_matte_baseline/`.
Checker-composited previews: `video_gen_experiment/phase3/r2/checker_composites/`. A 3x zoomed crop
of a wingtip edge (the sharpest test of glow handling):
`video_gen_experiment/phase3/r2/wingtip_edge_zoom.png`.

**Result:** the checker composites look decent at a glance (silhouette holds, most feathers extract
cleanly), but the zoomed edge crop shows exactly the failure mode `key_matte`'s own docstring
predicts: *"Soft fractional alpha (soft shadow, glow, glass, smoke) is mathematically unrecoverable
from one background."* There is a visible dark/muddy greenish-brown fringe hugging the feather
edges where the true pixel should be a clean warm gold rim-light fading to transparent — the
single-plate keyer cannot separate "feather-color-mixed-with-green" from "true dim gold glow," so
it darkens the edge instead. Small stray colored specks also survive in the nominally-transparent
region (the sparkle particles, imperfectly keyed). This is the concrete, expected evidence for
*why* the lead's original "matte doesn't work with glow" complaint is real — but it does **not**
tell us whether CorridorKey actually fixes it, only that the predicted failure mode is present and
visible on this exact fixture. That comparison is the piece that's blocked.

### Disk usage
`video_gen_experiment/tools/` = 42MB (CorridorKey source clone only, no venv/weights since the
build never ran). `video_gen_experiment/phase3/` = 38MB (all R1/R2/R3 frames, videos, reports).
R2's tool disk budget (≤15GB) was **not consumed** — nowhere close, since the heavy step (venv +
torch + checkpoint) never executed.

---

## R3 — iteration latency (detail)

### Setup
Took run4 (the hardened-prompt R1 winner, seed 101) as the base and applied 3 successive
"realistic edit" prompts, regenerating from scratch each time (same seed, only the prompt text
changed — matches how a real "fix this" loop would work: pin identity via seed, vary the
instruction). Workflows: `video_gen_experiment/phase3/r3/workflow_edit*.json`.

| Edit | Instruction added to prompt | Wall clock | Attempts to comply |
|---|---|---|---|
| 1. "slower" | "...the wings flap very slowly, minimal gentle subtle movement, almost still" | **143.4 s** | **1** (frame 0 vs frame 32 nearly identical pose — visibly less motion than the baseline run4) |
| 2. "bigger amplitude" | "...the wings flap with a large wide sweeping motion, big amplitude, dramatic full flap" | **113.6 s** | **1** (clear large sweep from raised-spread to lowered-inward across the clip — visibly bigger range than baseline) |
| 3. "less camera drift / steady" | "...static camera, wings stay centered and the same size, no zoom, no camera movement" | **120.5 s** | **1** (frame 0 vs frame 32: same framing/size/position, no visible drift or zoom) |

**Total for 3 edits: 377.5 s (~6.3 min), average 125.8 s/edit (~2.1 min), 0 re-rolls needed across
all 3.** Frame evidence: `video_gen_experiment/phase3/r3/edit{1,2,3}_frames/frame_*.png`.

### Honesty caveat
This is n=1 per edit — a single lucky run of 3 prompt tweaks, not a statistically characterized
re-roll rate. The community report's own framing ("gacha," ~80% first-try success sentiment) is
consistent with what was observed here (3/3 first-try), but a real production rollout should expect
some fraction of edits to need a re-roll or two, especially for more ambitious/specific asks than
these three simple ones. Recommend treating "~2 min/iteration, usually 1 attempt, occasionally 2-3"
as the working mental model for the lead rather than a guaranteed number.

### What this number means for the lead
This is the number to compare against the procedural track's *instant* (sub-frame, no
regeneration) feedback. ~2 minutes per edit with a real GPU cost, vs. zero-latency parameter
tweaking — confirms the synthesis document's framing: generative WAN is viable for the "verify +
occasional big changes" use case, but is **not** a substitute for the procedural track's
"nudge, don't re-roll" everyday editing loop. The two tracks remain complementary, not competing,
exactly as the T0256 synthesis recommended.

---

## Cleanup / isolation confirmation
- ComfyUI server killed after all phase-3 generations; verified port 8188 no longer answers and no
  `python_embeded\main.py` process remains (only this session's own shell-wrapper processes
  matched the process-list grep, same benign false-positive pattern seen in phases 1-2).
- Canvas source project `benchmark-fixture-c7f9dc` was never written to — only read via `show
  --json` and a plain filesystem copy of one PNG.
- Repo `game-67-idle` untouched except this report and the gitignored
  `tmp/t0257_setup_log.md` update.
- `video_gen_experiment/` total disk after phase 3: **~28 GB** (phase 3 itself added ~80MB on top
  of phase 1+2's ~28GB — R2's tool budget essentially unused due to the permission block).
