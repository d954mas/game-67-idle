# T0256 — Text-to-Animation, GENERATIVE angle (research)

Date: 2026-07-04. Author: deep-reasoner. Scope: generative routes for
"select image -> type animation -> preview -> fix problems" inside the canvas
tool (`ai_studio/assets/canvas/`), for 2D GAME art (sprites/icons/creatures,
256-1254px, already cut or on a flat key). Sibling agent covers procedural/rig.
Every access claim below is LIVE-probed on this box; external facts are sourced.

---

## 0. Live capability probe (this machine) — what is actually reachable TODAY

| Capability | Probe result | Verdict |
|---|---|---|
| GPU | `nvidia-smi`: RTX 4080 **Laptop**, **12282 MiB** (12 GB), driver 576.88, CUDA 12.9 | Capable but 12 GB caps video-gen res/length |
| torch | `torch 2.11.0+cpu`, `cuda.is_available()=False` | **CPU-only install; no local GPU inference set up** |
| ComfyUI | not found on disk (`/c/develop`, `~`, `/c/tools`) | **Not installed — local route is greenfield** |
| ffmpeg | `N-122685` (2026), with apng/webp/libvpx/nvenc/libplacebo | Present, fully capable for frames->APNG/webm/sheet |
| OpenAI (codex OAuth) | `.codex/skills/.../generate_image.py` lines 22, 121-151 | Reaches **`chatgpt.com/backend-api/codex/responses`**, `image_generation` tool ONLY. **No video path.** Transparent bg **rejected** on every model (line 124-126) |
| OpenAI Sora API | needs `sk-` key on `api.openai.com/v1/videos` | **NOT reachable** — he has OAuth JWT, not an `sk-` key |
| Gemini API key | `GEMINI_API_KEY` is set, but `GET /v1beta/models` -> **HTTP 400 `API key not valid`** | **Veo NOT reachable now** — key is stale/invalid; needs valid key + billing |
| agy / Antigravity | `agy models`: Gemini 3.5 Flash, 3.1 Pro, Claude, GPT-OSS only | LLM agents only, **no Veo/video model exposed** |

**Bottom line: there is NO turnkey text->video capability reachable today.**
The only reachable generative-media surface is `gpt-image` (single stills, on a
flat key colour). Every video route below requires either a new setup (local) or
a new/valid paid credential (cloud). This reframes the whole recommendation.

---

## 1. Cloud image+text -> video services (table; be brief)

All are image-to-video (I2V) with a reference/first frame -> preserves the
sprite's identity, which is mandatory for "выбрать картинку". None emit alpha.

| Service | I2V + first-frame | Price (approx) | Alpha? | Reachable now? | Notes for game sprites |
|---|---|---|---|---|---|
| **Veo 3.1 Fast** (Gemini API) | yes | **$0.15/s** (~$0.75/5s); Lite tier $0.03/s 720p no-audio | opaque RGB | **No** (key invalid; fixable) | Strong identity/prompt adherence; deprecated Veo3 shuts 2026-06-30, use 3.1 |
| **Sora 2 / 2-pro** (OpenAI Videos API) | yes (`input_reference`=first frame) | 1080p pro **$0.70/s** | opaque RGB | **No** (needs `sk-` key) | Videos API deprecates 2026-09-24 — moving target |
| **Kling** (API) | yes | prepaid packs from ~$9.80; ~$0.01-0.50/s class | opaque RGB | No (needs acct/key) | ToS: you keep IP, but grant Kuaishou broad sublicensable license to inputs/outputs, and "no training a competitor" — read before shipping client art |
| **Luma Ray 2** (API) | yes | ~$0.95/5s 1080p; Flash ~$0.60/720p | opaque RGB | No | Commercial rights require paid tier |
| **Runway Gen-3/4** | yes | subscription $12-76/mo | opaque RGB | No | Has built-in green-screen/matte tooling |
| **LTX (Lightricks) cloud** | yes | cheap | opaque RGB | No | Same model family you can run LOCALLY (see §3) |

Cloud pros: best first-shot quality, zero local setup. Cloud cons for HIS loop:
(1) not reachable today, (2) **every re-roll costs money — the "re-roll lottery"
he hates becomes a paid lottery**, (3) less seed/param control than local, (4)
ToS licensing questions on Kling/others for shippable assets.
Sources: ai.google.dev/gemini-api/docs/pricing; developers.openai.com/api/docs/guides/video-generation; kling.ai/docs/user-policy; eesel.ai/blog/luma-ai-pricing.

---

## 2. Local / open-source on his hardware (RTX 4080 Laptop, 12 GB)

Hardware is the right class; nothing is installed yet (torch is CPU-only, no
ComfyUI). One-time setup: CUDA torch + ComfyUI + model weights (a few hours).

| Engine | I2V (identity-preserving) | 12 GB feasibility | Speed on his card | Quality for sprites |
|---|---|---|---|---|
| **WAN 2.2 I2V 14B** | **yes** — first frame = his sprite | GGUF Q4 + T5 CPU-offload ~6-8 GB at 480p | **Slow: 10-20+ min/clip at 480p** on 12 GB (T5 offloaded); a 16 GB 4080 does ~6 min/81f. **4-step Lightning LoRA cuts this to ~2-4 min** — the key enabler | Best local identity + motion |
| **LTX-Video (0.9.x / LTX-2)** | yes | fits 12 GB (GGUF) | **Near real-time**: 5 s@768x512 in ~4 s on 4090; a few min on laptop 4080 | Fast DRAFTS; lower coherence/fidelity than WAN |
| **SVD / AnimateDiff** | SVD img2vid yes; AnimateDiff mostly T2V | fits | fast | Older; small/ambient motion only, weaker control |
| **Wan-Alpha** (RGBA-native, §4) | **NO — text-to-video only** (verified from paper) | WAN-class | 81f@480x832 in 128 s (4 steps) | Emits alpha+glow directly, but **cannot reproduce his exact sprite** (no image cond.) |
| **PixelLab API** | yes, first-frame preserved | cloud, $12-50/mo | fast | **Pixel-art ONLY, animation capped at 128x128** — too small for his 256-1254px general art; keep only for true pixel-art sprites |

Local pros: **free unlimited iteration, full seed/param control, private, no ToS
/asset-ownership issues** — exactly what "исправить проблемы, iterate" needs.
Cons: one-time setup; 12 GB caps res/length (use GGUF, 49-81 frames @ 480-720p,
then upscale); WAN quality-tier is slow unless the 4-step Lightning LoRA is used.
Sources: docs.comfy.org/tutorials/video/wan/wan2_2; nextdiffusion.ai (Wan2.2 GGUF low-VRAM); github.com/Lightricks/ComfyUI-LTXVideo; willitrunai.com/blog/wan-2-2-vram-requirements; pixellab.ai/docs/tools/animation.

---

## 3. The ALPHA / MATTING problem (rewritten per lead's steer)

Lead is right: per-frame chroma keying wastes the motion signal, and dual-plate
(white+black -> fractional alpha) does NOT extend to video (no consistent plate
pairs per frame). Modern **video matting** uses temporal info and emits soft
(fractional) alpha, on ANY background. Three tiers, best glow first:

**(A) RGBA-native generation — no matting needed (best glow, identity caveat).**
- **Wan-Alpha** (arXiv 2509.24979, Sep 2025): jointly generates RGB+alpha via a
  VAE that encodes alpha into RGB latent; paper explicitly claims *"semi-
  transparent objects, glowing effects, and fine-grained details such as hair
  strands."* Open, WAN-based, 4-step/128 s for 81f@480x832. **BUT verified
  text-to-video only — no image conditioning**, so it can't preserve his chosen
  sprite. Use for prompt-authored glowing FX layers, NOT identity-critical art.
- **TransPixeler/TransPixar** (CVPR 2025): RGBA video, but ~2x inference cost;
  Wan-Alpha supersedes it on efficiency/quality.
- Verdict: watch this space; not a v1 identity engine, but the *right long-term
  answer for glow*. A future Wan-Alpha-I2V or an alpha-LoRA on WAN 2.2 I2V would
  be the ideal — flag as R&D, not v1.

**(B) Temporal video matting on opaque output (the practical v1 extractor).**
| Model | Needs green/solid bg? | Temporal coherence | Soft/glow alpha | Local on 12 GB | License |
|---|---|---|---|---|---|
| **MatAnyone** (CVPR 2025) | **No** — any bg; needs 1st-frame mask (from SAM2) | **Flicker-free** memory propagation | **Yes — fractional**, preserves hair/transparency/motion-blur | Yes, ComfyUI nodes exist | research/open |
| **RVM** (Robust Video Matting) | No — trimap-free, any bg | Recurrent, good | Yes, fractional | Trivial (4K@76fps on GTX1080Ti) | open |
| **BEN2 / BiRefNet-v2** | No | BEN2 has video mode (reduced flicker); BiRefNet per-frame | Yes, matting variant emits soft alpha | Yes, ComfyUI-RMBG node | open |
| Runway green-removal / Unscreen | prefers solid | good | limited | cloud | paid |

- **MatAnyone is the recommended extractor**: any background, soft alpha,
  temporally stable, ComfyUI-native, takes a SAM2 first-frame mask (which the
  canvas can seed from the sprite's existing alpha). Best general-purpose choice
  for arbitrary creatures.
- **RVM caveat**: extremely fast and light, but trained on **humans/portraits**
  — may not generalize to non-humanoid game creatures. Good fast fallback; test
  per-subject.
- BEN2/BiRefNet: lighter per-frame option; more flicker risk than MatAnyone.

**(C) Chroma key (his existing `key_matte`) — FALLBACK only.** Generate on a
flat magenta/green key, key every frame. Works only for **hard-edged sprites**;
loses glow; risks per-frame key drift (the model repaints the bg slightly each
frame -> flicker). Keep as the cheap path for crisp cutout sprites, not glow.

**Glow honesty per route.** Matting emits *coverage* alpha, not additive light.
For a game this is usually fine: fractional alpha over the sprite reads as a
believable semi-transparent glow (premultiplied), and MatAnyone/Wan-Alpha both
capture soft falloff far better than chroma key. What is NOT perfectly
recoverable is true *additive bloom brighter than any backdrop* — for that,
Wan-Alpha (native alpha) or authoring the glow as a separate additive-blend
layer in-engine is the honest answer. Do not promise physically-additive glow
from a matte; promise "soft semi-transparent glow," which is achievable.

---

## 4. Iteration ergonomics ("fix problems", not first-shot) — scored /5

The lead's hard requirement. I2V + fixed seed is far more deterministic than
T2V because the **first frame pins identity to his exact sprite**; motion is
still stochastic, so "fix" = re-prompt + adjust motion/denoise strength + seed.

| Route | How you "fix problems" | Cost/re-roll | Determinism | Score |
|---|---|---|---|---|
| Local WAN 2.2 I2V + MatAnyone | seed lock, motion-strength/denoise, prompt edit, SAM2 mask retarget; region control via ControlNet = v2 | **free** | high (first-frame pinned + seed) | **4.5** |
| Local LTX (draft) | same, but faster loop, lower fidelity | free | med-high | 4 (great for drafts) |
| Cloud Veo/Kling | re-prompt + seed (less param control) | **paid per re-roll** | med | 2.5 |
| Chroma-key path | re-generate underlying clip | (route cost) | low (key flicker) | 2 |

Local wins decisively on iteration: zero marginal cost per re-roll and full
seed/param control. That is exactly the "исправить проблемы" loop.

---

## 5. RECOMMENDED v1 PIPELINE

**Primary: fully-local ComfyUI pipeline on the 4080.** Free unlimited iteration,
private, no ToS/ownership issues, real seed control — the best fit for his loop.

Stages (each an "animation card" step in the canvas):
1. **Source** — canvas layer/slice PNG (his cut sprite, on any bg) = I2V first
   frame. Its existing alpha seeds the SAM2 mask for step 3.
2. **Generate** — WAN 2.2 I2V 14B GGUF Q4 + **4-step Lightning LoRA**, 480-720p,
   49-81 frames, 16 fps, fixed seed. Prompt = his motion text ("крылья медленно
   машут, свечение пульсирует"). Fast-draft variant swaps in LTX-Video.
3. **Extract alpha** — **MatAnyone** (any-bg, soft alpha, flicker-free), mask
   seeded from the source alpha. Fallback: RVM (fast) or per-frame `key_matte`
   (hard-edged sprites only).
4. **Assemble** — ffmpeg (already installed): frames -> APNG + webm(VP9 alpha) +
   PNG sprite sheet + JSON frame metadata. Loop = ping-pong / best-cut-frame
   pass for seamlessness (WAN doesn't guarantee loops).
5. **Preview + iterate in canvas** — play the loop on an **"animation card"**
   (mirror the recipe card): inputs {source ref, motion prompt, engine, seed,
   frames, fps}; outputs {loop preview, sheet, per-frame RGBA}. **Cache by a
   `gen_hash` over (source-image-content, prompt, seed, params)** — reuse the
   exact sidecar-hash pattern already in `generate_image.py` (lines 195-210), so
   re-runs skip unchanged work and iteration is cheap.

Expected latency/iteration: draft (LTX) seconds-to-~2 min; quality (WAN+Lightning)
~2-4 min if Lightning holds, else 10-20 min (risk R3). Cost: $0.

**Fallback: cloud I2V + local matte.** When local quality/latency is
insufficient, or before local is set up: **Veo 3.1 Fast** (~$0.75/5 s, once a
valid Gemini key + billing exist) or Kling -> download opaque clip -> **MatAnyone
locally** -> same assemble/preview. Higher first-shot quality, no setup, but paid
per re-roll and ToS caveats. Keep the same "animation card" UI; only the generate
stage swaps engine.

**Parity note (Hard Invariant):** the generate/matte/assemble op must be ONE op
layer callable from both the agent CLI and the canvas page — same as existing
canvas ops. The card is just the site face of that op.

---

## 6. Three riskiest assumptions + cheap verification

- **R1 — Style drift (make-or-break).** I2V models are trained on photoreal
  video and may "photorealize" stylized game art or add unwanted motion. *Verify:*
  run 3-4 WAN 2.2 I2V clips on representative sprites (flat icon, painterly
  creature, pixel-ish), eyeball style fidelity vs source. ~1 evening once ComfyUI
  is up. If drift is bad, add a style/identity LoRA or fall back to cloud Veo
  (better style adherence).
- **R2 — Matte on non-human creatures + glow.** MatAnyone/RVM lean on human
  priors; glow is coverage-alpha not additive. *Verify:* matte one glowing test
  clip, composite over a checkerboard, inspect edge flicker + glow falloff. If
  weak, try BEN2, or author glow as a separate additive layer.
- **R3 — Iteration latency on the 12 GB LAPTOP 4080.** Whole plan assumes ~2-4
  min/iteration via the 4-step Lightning LoRA; if it's really 15-20 min, the
  "preview + fix" UX collapses. *Verify:* time one WAN 2.2 I2V GGUF Q4 + 4-step
  Lightning run at 480p/49 frames. If too slow, make LTX the default engine and
  WAN a "final render" button.

Secondary risks: seamless-loop quality (mitigate with ping-pong/boomerang in
ffmpeg); the invalid Gemini key blocks the cloud fallback until refreshed +
billing enabled; Wan-Alpha's lack of image conditioning means native-alpha glow
stays R&D, not v1.

---

## Sources
- Local probes: `nvidia-smi`, `agy models`, `curl` Gemini ListModels (HTTP 400),
  `.codex/skills/nt-asset-image-generation/scripts/generate_image.py`.
- ai.google.dev/gemini-api/docs/pricing; developers.openai.com/api/docs/guides/video-generation
- kling.ai/docs/user-policy; eesel.ai/blog/luma-ai-pricing; ulazai.com/ai-video-models-guide-2025
- docs.comfy.org/tutorials/video/wan/wan2_2; willitrunai.com/blog/wan-2-2-vram-requirements; nextdiffusion.ai Wan2.2 GGUF low-VRAM
- github.com/Lightricks/ComfyUI-LTXVideo; peterl1n.github.io/RobustVideoMatting
- github.com/pq-yang/MatAnyone (CVPR 2025); github.com/FuouM/ComfyUI-MatAnyone
- arxiv.org/html/2509.24979v1 (Wan-Alpha); wileewang.github.io/TransPixar (CVPR 2025)
- runware.ai/collections/best-background-removal (BEN2/BiRefNet); pixellab.ai/docs/tools/animation
