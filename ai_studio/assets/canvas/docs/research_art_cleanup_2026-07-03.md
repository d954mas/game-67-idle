# Research: post-generation art cleanup — watermarks, color artifacts, quantization

Date: 2026-07-03 · Repo: game-67-idle · Scope: cleanup of the lead's OWN gpt-image/Codex
generations in the private canvas pipeline (no third-party content). Extends
`ai_studio/taskboard/items/active/T0207-clean-art-post-gen-cleanup-op-bg-solidify-palett.md`
(Quantize + Denoise already decided as INTERACTIVE canvas tools; bg-solidify internal-only).

## Environment ground truth (verified in `.venv`, not assumed)

Pinned deps (`ai_studio/assets/tools/image/requirements.txt`): **numpy 2.1.1, scipy 1.17.1,
Pillow 12.2.0**. Probed live:

- `libimagequant` = **False** — `quantize(method=LIBIMAGEQUANT)` raises `ValueError:
  dependency required by this method was not enabled at compile time`. Best-quality quantizer
  is NOT available in our Pillow build.
- **opencv (`cv2`) = absent.** Any bilateral/NLM-via-cv2 is a NEW binary dependency.
- **RGBA quantize is FASTOCTREE-only**: `MEDIANCUT`/`MAXCOVERAGE` raise `ValueError: Fast
  Octree ... are the only valid methods for quantizing RGBA images`. This is the alpha pitfall
  (§3) — load-bearing, verified.
- **Metadata is auto-stripped by our pipeline**: `Image.fromarray(np.asarray(img))` drops all
  `.info`/text chunks, and a plain `img.save(path)` (no `pnginfo=`/`exif=`) writes zero text
  chunks. Verified: a PNG carrying `Software=gpt-image-1` + `parameters=...` came back with
  empty `text`/`info` after a numpy round-trip and after a plain resave. Every keyer/quantize/
  denoise/crop/render op round-trips through numpy → **all embedded provenance metadata dies
  as a side effect** (§1b).
- Feasibility of the proposed toolset all confirmed live: split-alpha MEDIANCUT quantize keeps
  alpha **byte-exact**; `quantize(palette=<P-image>)` shared-palette path works; `convert(
  "YCbCr")`, `ImageFilter.MedianFilter`, and `scipy.cluster.vq.kmeans2(minit="++", seed=0)`
  all work in-venv (no new deps).

---

## 1. AI watermarks on his own generations

### 1a. Visible marks
gpt-image-1 (OpenAI API — the lead's path via Codex/`scripts/codex_imagegen.sh`) produces
**clean images with NO visible watermark**; provenance is metadata-only, not a stamp on pixels
([MindStudio](https://www.mindstudio.ai/blog/what-is-gpt-image-1-openai),
[OpenAI image-generation API](https://openai.com/index/image-generation-api/)). So the corner-
logo/inpaint-corner problem **does not exist for our current generator**.

The visible-mark case only appears if the lead switches generators: **Google Gemini / "Nano
Banana"** stamps a visible corner **sparkle** glyph on consumer outputs (the community
`remove-ai-watermarks` tool exists specifically to strip "Gemini / Nano Banana sparkle")
([wiltodelta/remove-ai-watermarks](https://github.com/wiltodelta/remove-ai-watermarks)). If that
ever enters the pipeline, the fix is trivial and non-ML: **crop** the marked corner, or key/
inpaint that rectangle — but do NOT build for it now (YAGNI; our generator is unmarked).

### 1b. Metadata provenance (C2PA / XMP / EXIF)
gpt-image-1 and DALL·E 3 embed **C2PA Content Credentials** as file metadata (a manifest in an
XMP/metadata chunk), invisible and detectable via Content Credentials verifiers
([OpenAI help: C2PA + SynthID](https://help.openai.com/en/articles/8912793-c2pa-in-chatgpt-images),
[OpenAI content-provenance](https://openai.com/index/advancing-content-provenance/),
[Mike Cvet — examining C2PA in DALL·E 3](https://mikecvet.medium.com/examining-c2pa-provenance-metadata-in-dall-e-3-images-64ed51159091)).
This metadata **cannot be disabled at the API**, but **stripping is trivial and we already do
it**: any re-encode without copying `pnginfo`/`exif` removes it, and every numpy-based op in our
pipeline re-encodes. Verified empirically above. **No dedicated strip tool is warranted.**

> Repo caveat worth flagging (grounded in `ai_studio/assets/canvas/README.md`): `exportElements`
> has a **1x-png-of-png byte-identical Node COPY fast path** (no re-encode). That is the ONE path
> where original C2PA metadata SURVIVES to the exported file. Everything else (any cleanup op,
> alpha cutout, slice, render, or a scaled/re-encoded export) strips it. If metadata-clean export
> ever matters, run one cleanup op first, or have the copy fast-path re-encode. Minor.

### 1c. Invisible pixel-domain watermark (SynthID-style)
OpenAI's provenance stack now also references **SynthID** (imperceptible pixel-embedded signal)
alongside C2PA ([OpenAI help article](https://help.openai.com/en/articles/8912793-c2pa-in-chatgpt-images));
Google's SynthID is **designed to survive resize, crop, compression, filtering, and added noise**
because the signal is holographically spread across the whole image
([Google DeepMind SynthID](https://deepmind.google/models/synthid/),
[DataCamp SynthID guide](https://www.datacamp.com/tutorial/synthid)).

**Verdict for us:** irrelevant to quality and not worth touching. (a) It is **imperceptible by
definition** — an invisible watermark does not degrade sprite/UI art; there is no visual defect
to repair. (b) The lead is NOT trying to defeat provenance (his own art, private use), so its
survival is a non-issue. (c) Incidentally, our own ladder perturbs it heavily anyway — keying
zeroes/flattens large regions to an exact key color, quantize snaps to ≤N colors, and 2x→1x
downscale resamples — but we neither rely on nor need that. **Do not build watermark-defeat
tooling** (§5). Keep this factual: nothing we generate carries a mark that visibly degrades the
art or survives as a *visible* artifact.

---

## 2. Color artifacts from diffusion/VAE on flat game art

Cause: the VAE encoder injects **input-independent high-frequency noise even on uniform-color
regions** and rings/overshoots at **sharp boundaries**; decode adds low-amplitude speckle,
near-duplicate banding steps in gradients, and off-palette pixel scatter
([Denoising ViT-AE, arXiv 2511.12633](https://arxiv.org/pdf/2511.12633);
general diffusion-artifact context [Apatero/SeedVR2](https://apatero.com/blog/seedvr2-removing-artifacts-complete-guide-2025)).
On FLAT/cel/UI art these read as: a "solid" fill that is actually 40–300 near-identical colors,
1–2px speckles, and slightly ragged color at edges (our keyer already handles the key-side halo —
see `key_matte.py` `_limit_despill` / `_extend_clean_edge_colors`, the halo lesson: kill spill at
the root by extending clean edge colors + a sharp channel-limit despill, NOT by blurring).

Repairs implementable in PIL/numpy/scipy (no GPU/ML), and where each is safe:

| Artifact | Repair (in-venv) | Safe for pixel-crisp/flat/UI | Safe for painterly |
|---|---|---|---|
| VAE speckle / salt-pepper | **quantize snap** (primary); or `MedianFilter(3)` / `scipy.ndimage.median_filter` | quantize ✓ ; median ✓ small only (erodes 1px detail if ≥5) | median 3 ✓ ; quantize ✗ |
| "Solid" fill = 100s of near-colors + gradient banding | **quantize / snap-to-palette** (collapses the band into flat cels) | ✓ (this is the fix) | ✗ (kills gradients — use chroma-denoise instead) |
| Off-palette pixel scatter | quantize with dither OFF; or dominant-color snap (k-means label → centroid) | ✓ | partial |
| Chroma noise / color fringing | **YCbCr split → denoise Cb/Cr only, keep Y sharp** (`convert("YCbCr")` + median/box on chroma) | ✓ | ✓ (best painterly option) |
| Edge fringe / key halo | already handled in `key_matte.py` (despill + clean-edge extend + bleed) | ✓ (existing) | n/a |
| JPEG 8×8 blocking | mild chroma low-pass; full DCT-deblock is heavy | rarely needed (gpt-image is PNG) | rarely |

Key finding: **for flat/UI/sprite art, quantization IS the color-artifact repair** — snapping to
N colors simultaneously erases VAE speckle, collapses banding steps, and removes off-palette
scatter. A separate denoise pass is usually redundant for flat art. **Bilateral filter** is the
textbook edge-preserving "posterize flat domains, keep sharp edges" filter for cel art
([scikit-image denoise](https://scikit-image.org/docs/stable/auto_examples/filters/plot_denoise.html)),
but there is **no built-in bilateral in PIL/scipy** and cv2/skimage are absent — so bilateral =
custom numpy (slow) or a new dep; **for flat art, quantize+median already covers it**, so skip
bilateral unless a painterly case demands it. Median (impulse noise, edge-preserving) and
YCbCr-chroma denoise are the two denoise modes worth shipping — both in-venv, no new deps.

---

## 3. Quantization for game art

- **PIL modes** ([Pillow Image docs](https://pillow.readthedocs.io/en/stable/reference/Image.html)):
  `MEDIANCUT` (default, **RGB only**), `MAXCOVERAGE` (**RGB only**), `FASTOCTREE` (RGB+RGBA),
  `LIBIMAGEQUANT` (best, **unavailable in our build** — verified). Practical set: **MEDIANCUT**
  (good default quality on RGB), **FASTOCTREE** (only RGBA-capable), plus **scipy `kmeans2`** for
  a controlled/custom palette (deterministic with `seed=0`, `minit="++"`; verified).
- **Dithering**: `Dither.FLOYDSTEINBERG` (default) vs `Dither.NONE`. For **sprites/flat/UI: dither
  OFF** — dithering sprinkles a noise pattern that looks wrong on flat art and re-inflates the
  unique-color count, defeating the palette. **Dither ON** only to preserve a smooth gradient in
  painterly art at low N.
- **Alpha-aware pitfall (verified, load-bearing):** our art is KEYED to RGBA before cleanup. You
  **cannot** MEDIANCUT/MAXCOVERAGE an RGBA image (raises); FASTOCTREE *can*, but it treats alpha
  as a 4th octree channel — spending palette slots on alpha levels and lumping semi-transparent
  edge pixels with color clusters. **Do NOT quantize RGBA directly.** Correct approach (verified
  byte-exact): **split alpha off, quantize the RGB with MEDIANCUT (or shared palette), reattach
  the ORIGINAL alpha unchanged.** Also build the palette on **opaque interior pixels only**
  (mask `alpha < threshold`) so the anti-aliased/transparent fringe never steals palette entries.
- **Per-sheet shared palette (verified):** `Image.quantize(palette=<P-mode image>, dither=NONE)`
  re-maps any image onto an existing palette. Build ONE palette (MEDIANCUT or kmeans over all
  sheet pixels) and apply it to every sliced sprite → color-consistent sheet. This is the game-
  art-specific win over per-sprite quantize.
- **When quantize repairs (not just reduces):** snap-to-palette is the cleanup for flat art (§2).
  This unifies "reduce palette" and "clean color noise" into ONE interactive op.

---

## 4. Recommended post-gen cleanup ladder

Order is derived from how the ops interact (keyer distance math, alpha, resample), NOT arbitrary:

```
generate on MAGENTA/GREEN key bg   (upstream convention — neutral bg = no edge decontam; per memory)
        │
        ▼
1. KEY / ALPHA CUTOUT   ← FIRST.  (existing alphaCutout op)
        │   Why first: keying is border-connected key-DISTANCE. Denoise before keying blurs the
        │   key boundary and shifts the color distribution → harder key. Quantize before keying
        │   can merge near-key art into the key → key eats art or misses. bg-solidify already runs
        │   as an INTERNAL in-memory pre-pass inside the keyer (snap near-key before cutting) —
        │   that HELPS the key and writes no file. Keep as-is.
        ▼
2. QUANTIZE  (interactive)   ← primary color-artifact repair for flat/UI art.
        │   Split alpha, quantize RGB (MEDIANCUT / shared palette / kmeans), reattach alpha
        │   byte-exact. Palette built on opaque pixels only. Dither OFF for sprites.
        ▼
3. DENOISE  (interactive, OPTIONAL)   ← mostly for PAINTERLY art, or residual speckle quantize
        │   left. Modes: median(3) | YCbCr-chroma. For flat art usually SKIPPED (quantize did it).
        │   For painterly: denoise INSTEAD of quantize (quantize kills gradients).
        ▼
4. EXPORT scale (2x→1x Lanczos)   ← already in export settings.
            Ordering subtlety to document: Lanczos downscale re-introduces intermediate colors at
            edges (blends palette colors). For a STRICT palette output, quantize should be the LAST
            color op before a NEAREST-resample export, OR re-quantize after downscale. For smooth
            downscale, accept the edge blend. Call this out in the op help; do not auto-decide.
```

Two presets map cleanly onto the T0207 "presets not knobs" decision:
- **Flat / UI / sprite:** Key → Quantize(dither off, auto-N) → export. Denoise off.
- **Painterly:** Key(only if bg present) → Denoise(chroma, mild) → export. Quantize off (or high N).

**Auto-detected** (defaults, no tuning): key color from border (existing `estimate_border_key_color`
in `bg_fix/normalize_background.py`); quantize N auto from the opaque unique-color histogram (e.g.
knee of the sorted-count curve, clamped to a sane range); flat-vs-painterly hint from unique-color
count / gradient energy → chooses the preset default.

**Op parameters** (advanced, collapsed): quantize `colors`, `dither`, `method`, `palette` (shared);
denoise `strength`, `mode`.

**Loud-refusal cases** (loud error, no silent fallback — matches repo law):
- `quantize` requested with `method=libimagequant` → refuse naming that it's not compiled in.
- Any attempt to quantize **alpha** / an RGBA buffer directly without split → not offered; alpha is
  always preserved byte-exact (implementation invariant, add a pixel test like the alpha ops have).
- Non-finite/negative `colors`/`strength`, or `colors` outside [2,256] → refuse (as `render_group`/
  `parseScaleSpec` do).
- Denoise kernel larger than the sprite → refuse/clamp loudly.
- (Interactive, NOT a hard refuse:) low-N quantize + dither-off on a gradient-heavy (painterly)
  image — surface a warning in the live preview (unique-color count is high), let the split/hold
  compare show the damage; the human decides. Consistent with the interactive-preview decision.

---

## 5. What NOT to build (one line each)

- **Bilateral/NLM via OpenCV** — cv2 absent; new heavy binary dep for what quantize+median already
  cover on flat art.
- **scikit-image just for denoise** — big dep; median (PIL/scipy) + kmeans (scipy) + YCbCr split
  cover our cases with zero new deps.
- **libimagequant quantizer** — not compiled into our Pillow 12.2.0; needs a custom Pillow rebuild.
  Use FASTOCTREE / MEDIANCUT / scipy-kmeans.
- **Any ML restorer** (SeedVR2, diffusion artifact removers, Real-ESRGAN, GAN denoise) — needs
  GPU/torch runtimes; violates the deterministic-tools, no-ML-runtime law.
- **Watermark-defeat / SynthID removal / adversarial perturbation** — pointless (his own art,
  imperceptible, no quality impact) and fights nothing; the C2PA metadata is already stripped by
  every re-encode.
- **A dedicated strip-metadata tool** — redundant; any numpy op already strips it (verified). At
  most, a one-line note that the export byte-copy fast path is the sole metadata-preserving path.
- **JPEG DCT-deblock solver** — gpt-image outputs PNG; blocking is rare and the fix is heavy.

---

## 6. Concrete additions to T0207 (op names, params, feasibility, packet sizing)

All feasibility below was probed live in `.venv` — no unproven claims. Ops follow the existing
canvas pattern (`ai_studio/assets/canvas/tools/*.py` spawned via the T0218 config-only `_bridge`
python; new immutable content-addressed file + one journaled src-swap + `meta`/`tool_runs`
provenance + before/after report; interactive preview writes nothing to disk — exactly like
`alphaCutout`).

**OP A — `quantize` (interactive; already decided).**
`tools/quantize_image.py` reusing `_bridge`. Params:
- `colors` (int 2–256, slider) · `dither` (bool, default **false**) ·
- `method` (`"mediancut"` default | `"kmeans"` | `"octree"`) ·
- `palette` (optional: element/file ref → shared per-sheet palette via `quantize(palette=…)`) ·
- alpha handling = **split-preserve, non-negotiable** (split alpha, quantize RGB, reattach exact
  alpha; palette built on `alpha>threshold` pixels only).
- Report: input→output unique-color count, palette size, changed-pixel %.
Feasibility: **PIL `quantize` + `scipy.cluster.vq.kmeans2` — in-venv, verified.** No new deps.
Packet: **1 fast-worker, MEDIUM** (split-alpha wrapper + shared-palette path + kmeans path +
debounced preview + alpha-byte-exact pixel test + report). Ship MEDIANCUT+octree first; kmeans and
shared-palette can be a second packet if scope is tight.

**OP B — `denoise` (interactive; already decided).**
`tools/denoise_image.py`. Params:
- `strength` (slider → maps to `median` kernel 3/5, or chroma sigma) ·
- `mode` (`"median"` default | `"chroma"` = YCbCr Cb/Cr denoise, Y untouched) ·
- alpha preserved byte-exact (denoise RGB only; leave the keyer's edge work intact).
- Report: changed-pixel %, mode/strength.
Feasibility: **`ImageFilter.MedianFilter` / `scipy.ndimage.median_filter` / `convert("YCbCr")` —
in-venv, verified.** No bilateral (would need cv2/custom — out of scope). No new deps.
Packet: **1 fast-worker, SMALL–MEDIUM** (two modes + preview + a "foreground pixels untouched
beyond strength" pixel test).

**OP C — `palette-extract` helper (optional, enables shared palettes).**
Small pure tool: kmeans/MEDIANCUT over a selection or whole sheet → a stored P-mode palette
element that `quantize(palette=…)` consumes. Feasibility: scipy/PIL, in-venv. Packet: **1
fast-worker, SMALL.** Only build if per-sheet color consistency is prioritized.

**No watermark/metadata op.** Add a one-paragraph note to T0207 that provenance metadata is
stripped by any op's re-encode (verified), and flag the `exportElements` 1x-png byte-copy fast
path as the single metadata-preserving exception.

**Docs to touch when these land:** `ai_studio/assets/canvas/README.md` (new op entries + ladder
note), the imagegen skill convention ("flat/UI generations MUST be quantized with a before/after
in the report; painterly left alone unless asked"), and T0207 Log.
