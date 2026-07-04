# T0262 — Speeding up local video generation (WAN 2.2 I2V on RTX 4080 Laptop 12GB)

Research-only. Nothing installed, experiment folder untouched. All numbers below are
community results on comparable consumer hardware (12–24 GB Ada/Ampere), 2025–2026.

---

## 1. Baseline recap (measured on this box)

- **Stack:** ComfyUI portable + WAN 2.2 I2V **A14B GGUF Q4_K_S** (high+low expert pair) +
  umt5-xxl Q4 + **Lightning rank64 LoRAs (4–8 step distill)**.
- **Job:** 33 frames @ 480×480, 16 fps.
- **Time:** ~218 s cold / ~103 s warm; practical edit iteration ~126 s.
- **Goal:** draft iteration meaningfully **under a minute**, knowing the quality cost of each lever.

**Where the warm ~103 s goes (estimated split, for reasoning about which lever hits what):**
- ~65–75 s sampling — two experts × 4–8 steps of the A14B transformer.
- ~15–20 s VAE decode of 33 frames.
- ~10–15 s fixed overhead — text encode (cached if prompt unchanged) + **the high→low expert
  model swap** (both A14B experts do NOT fit together in 12 GB, so one is offloaded/reloaded
  mid-run — this is real warm-iteration cost, not just cold).

**Load-bearing consequence:** the two big time sinks are *sampling* (steps × frames × pixels,
attention quadratic in tokens) and *VAE + frames*. The levers that hit those directly (resolution,
frame count, steps) beat the fashionable "accelerator" nodes here — see §4.

**FIRST THING TO CONFIRM with the worker:** is the current run **4 total steps or 8**
(2+2 vs 4+4)? The baseline says "4–8". This single fact changes remaining headroom a lot
(see projections in §5).

---

## 2. Ranked lever table

Gain = realistic multiplier **at this 480×480 / few-step / GGUF operating point** (not the
headline number from many-step photoreal benchmarks). Quality risk is specifically for
**flat 2D game art (linework + flat colors)**.

| # | Lever | Realistic gain here | Quality risk (flat 2D) | Setup / fragility (Win portable) | Composes with |
|---|-------|--------------------|------------------------|----------------------------------|---------------|
| 1 | **Draft resolution 480→384 (+anime upscale back)** | **~1.4×** | Low–med: linework thins/aliases at 384, but 2x-AnimeSharp / RealESRGAN-anime **restore linework cleanly** (built for exactly this). Upscale cannot restore motion the low-res gen never made — fine for drafts. | **Zero install** (change latent size). Upscale node ~1–2 s/frame. Reversible per-run. | everything |
| 2 | **Fewer frames (33→25) for drafts** | **~1.3×** | Low: shorter clip, same motion quality. | **Zero install.** Reversible. | everything |
| 3 | **Step / expert rebalance (8→4 total; bias high-noise down)** | **~1.3–1.6×** (only if currently 8) | Med: 4-step is the validated floor; motion simplifies, fine detail softens — holds up better on flat art than on photoreal. Fewer high-noise steps also **cuts expert swaps on 12 GB.** | **Zero install** (Lightning 4-step lora / step sliders). | everything |
| 4 | **SageAttention 2.2** | **~1.15–1.4×** (attention is a smaller share at 480² than at 720p) | **Very low** — INT8/FP8 attention; SpargeAttention variant reportedly *improved* image quality. Neutral on linework. | **Moderate–fragile:** Triton-windows + matching woct0rdho wheel (torch/cu126/py). Can change torch ver & break custom nodes → **do on a COPIED portable.** One-time. NOTE: breaks WAN **5B** (noise) — irrelevant, we're on 14B. | everything (incl. GGUF+Lightning; proven at 4-step) |
| 5 | **Batch N seeds in one graph** | frees ~load cost per explored seed | None | Zero install. | everything |
| 6 | **FLF (first-last-frame) loop trick** | fewer frames per seamless loop | None–low (loop-only) | Workflow rewire; low. | everything |
| 7 | **Gen at 8 fps + GMFSS 2× interpolate → 16 fps** | up to ~1.8× on the gen | **Med–high:** interpolating big motion smears linework; **GMFSS Fortuna** (anime-tuned) beats RIFE on flat art but still risky on fast motion. | Custom node install; moderate. | res/frames/steps |
| 8 | **torch.compile (WanVideo)** | ~1.1–1.5× *when it works* | Low (numeric only) | **Fragile on GGUF/portable:** PyTorch 2.8 gave 8–9 min first-run recompiles; some GGUF Q4 reports show **no runtime gain, only VRAM drop**. Cache only survives if **shapes stay fixed** — changing res/frames per edit re-triggers. | Sage (common pair) but stacks fragility |
| 9 | **`--fast` / fp16 accumulation** | ~1.0–1.05× here | None | Zero install (flag). | — |
| 10 | **TeaCache / MagCache** | **~1.0–1.15× here (NOT the 2× headline)** | **Med–high in few-step:** the step-similarity assumption breaks at 4–8 steps → ghosting/motion-stutter, up to ~15% semantic drop; very visible on flat linework. | Node install, needs threshold tuning. | technically yes, **not worth it while distilled** |
| 11 | **WAN 5B ti2v as draft engine** | fast w/ Lightning, but | **High mismatch:** 5B is weak on complex/multi-object motion; its motion won't predict the 14B → poor draft→final fidelity. SageAttention breaks 5B. | Model swap; med. | — |
| 12 | **LTX-2 distilled as draft engine** | genuinely fast (8-step CFG-1; ~2 min/5.4 s@480p on 16 GB; Speed LoRAs 5–10×) | Handles flat/cel/anime **if** given a strongly stylized first frame — but **different family → different motion/aesthetic than WAN**, so a weak "draft-of-the-final." | Separate model+nodes; med. | standalone only |

### Why the "accelerator" nodes rank low here (the key finding)
TeaCache's famous 2× (and the 3.09× Voltage-Park stack) come from **40-step** runs — TeaCache
alone was 2.05× of that because there's a lot of redundant steps to skip. **At 4–8 distilled
steps there is almost nothing to cache**, and few-step caching is documented to break the
adjacent-step similarity assumption (motion stutter, ~15% semantic loss). So on this *already
distilled* stack, **caching is low gain + real quality risk** — deprioritize. **SageAttention is
different**: it speeds the attention op itself regardless of step count, so it *does* compose with
Lightning (proven 1.6× on a 3090 at 4-step, 1280×704; less at 480² where attention is a smaller
fraction).

---

## 3. Draft→final ladder proposal (recommended)

**Keep the SAME engine (WAN A14B) for both draft and final** — only lower resolution/frames/steps
for the draft. Same engine + **locked seed** means the draft faithfully predicts the final's
composition and motion. That predictive fidelity is the entire point of fast iteration and is
exactly what a cross-engine ladder (LTX-2 / 5B draft → 14B final) throws away.

- **DRAFT profile:** 384×384, 25 frames, 4 total steps, fixed seed, (+SageAttention in Pkg #2).
  Iterate prompt/seed here. Optional 2x-AnimeSharp preview upscale.
- **FINAL profile:** re-run the *accepted seed* at 480×480 (or 768 via upscale), 33 frames,
  4–6 steps. One render, ~100–140 s warm.

LTX-2 distilled is worth keeping in your back pocket **only** as an "instant rough-motion sketch"
when WAN-fidelity isn't required — not as the drafts for WAN finals.

---

## 4. Recommended packages

### Package #1 — Zero-install draft profile (apply today)
No installs, fully reversible via a draft/final toggle, no fragility.
- Resolution **384×384** for drafts (+2x-AnimeSharp when you want to eyeball detail).
- Frames **33→25**.
- Steps at **4 total** (2 high + 2 low) if currently 8; bias high-noise steps down for
  same-first-frame edits (don't fully drop high-noise — it sets motion/layout).
- **Batch 2–4 seeds** per graph for exploration.

**Projected warm draft iteration:**
- If currently 8 steps → **~35–45 s** (res+frames+steps compound on the sampling/VAE share).
- If already 4 steps → **~50–57 s** (res+frames only). Still under a minute, but tighter.

### Package #2 — Add one install (if #1 not fast/clean enough)
- **SageAttention 2.2** on a **copied** portable (Triton-windows + woct0rdho wheel matched to
  torch/cu126/python). One-time, ~1.15–1.4× on top of #1, negligible quality cost.
- Consider **torch.compile only if you freeze draft shapes** (res+frames fixed) so its cache
  survives across edits; skip it if you vary res/frames per iteration.

**Projected warm draft iteration:** **~30–40 s** (from 8-step baseline) / **~42–50 s**
(from 4-step baseline).

**Deliberately excluded:** TeaCache/MagCache (low gain + quality risk while distilled),
`--fast` (no effect on GGUF), 5B/LTX draft engines (fidelity mismatch to the WAN final).

---

## 5. Bottom line

The under-a-minute target is reachable **without installing anything** — the biggest, safest wins
are the boring ones (resolution + frames + steps), because they hit the two real time sinks
directly, are zero-install, and are reversible per-run. SageAttention is the one worthwhile install
(do it on a copy). The trendy caching accelerators are the wrong tool for an *already-distilled*
4–8-step pipeline. Keep one engine (WAN A14B) across a locked-seed draft→final ladder so the fast
draft actually predicts the shipped frame.

Confirm current step count first — it decides whether #1 alone clears a minute or whether you need #2.

---

### Sources
- SageAttention/SpargeAttention on WAN 2.2 (RTX 3090, 4-step Lightning, 1.6×/2×): https://www.digitalcreativeai.net/en/post/how-speed-up-wan2-2-comfyui-sageattention-spargeattention
- SageAttention2++ (3.9× over FlashAttention): https://arxiv.org/html/2505.21136v1 ; repo https://github.com/thu-ml/sageattention (Windows 37% vs FlashAttn: issue #150)
- Per-step optimization breakdown / TeaCache 2.05× at 40 steps: https://www.voltagepark.com/blog/accelerating-wan2-2-from-4-67s-to-1-5s-per-denoising-step-through-targeted-optimizations
- TeaCache node + thresholds: https://github.com/welltop-cn/ComfyUI-TeaCache ; MagCache (Wan2.2 1.5–2×): https://github.com/Zehong-Ma/ComfyUI-MagCache
- Few-step caching degradation (~15% semantic drop): https://arxiv.org/pdf/2508.08978 ; https://arxiv.org/pdf/2606.13496
- torch.compile on Windows / WAN GGUF status (no runtime gain on Q4, VRAM drop; PyTorch 2.8 recompile pain): https://github.com/Comfy-Org/ComfyUI/discussions/5236 ; https://github.com/kijai/ComfyUI-KJNodes/issues/364 ; Triton-windows https://github.com/triton-lang/triton-windows
- fp16 accumulation / `--fast` (fp16 only, not fp8/GGUF): https://github.com/Comfy-Org/ComfyUI/pull/6453
- Lightning 4+4 vs steps quality & high/low balance: https://huggingface.co/Kijai/WanVideo_comfy/discussions/59 ; https://civitai.com/models/1822764 ; lightx2v skepticism https://huggingface.co/lightx2v/Wan2.2-Lightning/discussions/23
- WAN 2.2 MoE high/low expert design & swap on 12 GB: https://huggingface.co/Wan-AI/Wan2.2-I2V-A14B ; https://docs.comfy.org/tutorials/video/wan/wan2_2 ; https://wan27.org/blog/wan-2-2-vram-guide
- Resolution/draft ladder (480 draft → 720 final), 5B weak on complex motion: https://www.spheron.network/blog/deploy-wan-2-1-ai-video-generation-gpu-setup/ ; https://wan27.org/blog/wan-2-2-model-files-explained
- WAN 5B ti2v speed/VRAM: https://willitrunai.com/video-models/wan-video-2-2-ti2v-5b ; https://www.nextdiffusion.ai/tutorials/fast-image-to-video-comfyui-wan2-2-lightx2v-lora
- LTX-Video 0.9.x / LTX-2 distilled speed & 2D/anime support: https://github.com/Lightricks/ComfyUI-LTXVideo ; https://ltx.io/blog/how-to-generate-2d-animation-with-ai-video-models ; https://zenvanriel.com/ai-engineer-blog/ltx-2-3-open-source-video-generation-guide/
- Anime upscalers (2x-AnimeSharp, RealESRGAN-anime, linework): https://civitai.com/models/1245815/2x-animesharpv4 ; https://openmodeldb.info/?t=anime
- Interpolation for flat 2D (GMFSS Fortuna > RIFE for anime): https://github.com/Fannovel16/ComfyUI-Frame-Interpolation ; https://www.runcomfy.com/comfyui-nodes/ComfyUI-Frame-Interpolation/GMFSS-Fortuna-VFI
