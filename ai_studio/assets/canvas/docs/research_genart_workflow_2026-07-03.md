# Research: established practice for AI game-art generation (consistent 2D asset production)

Date: 2026-07-03. For the lead's question: "как принято работать с генерацией
арта? Нужны рефы, описание стиля, описание картинки?" — validated against our
actual stack (gpt-image-2 via Codex OAuth backend / sk- REST; NO local SD /
LoRA / ControlNet; NO usable seed).

Our stack, confirmed from code (`.codex/skills/nt-asset-image-generation/scripts/`):
- `generate_image.py`: params that EXIST are `--prompt --out --size --quality
  {low,medium,high,auto} --format --model --background {transparent,opaque,auto}
  --input-image (repeatable, <=5) --responses-model`. **No `n` (hardcoded 1),
  no seed.** Batch = `gen_batch.py` fan-out of independent single calls.
- Transparency: codex backend REJECTS `transparent` on every model (line 105-107)
  → generate on a flat key colour + chroma-key in post; only the sk- REST path
  (gpt-image-1.5) yields true alpha.
- **Reference images are wired ONLY on the codex-backend path** (`gen_codex`
  attaches `input_image`, lines 108-110). `gen_rest` (`/v1/images/generations`,
  lines 135-160) builds NO input-image field → refs are silently ignored on the
  sk- key path. This is a bug relative to intent; see verdict.
- Reproducibility already handled by the `gen_hash` sidecar (prompt+size+quality+
  format+model+background+ref-bytes) → a re-run with matching sidecar skips the
  call. This is "cache the bytes", which is exactly the right pattern (below).

---

## Q1 — Prompt anatomy for game assets

OpenAI's own prompting guides are the primary source and they converge on a
**fixed field order**: `background/scene → subject → key details → constraints`;
for product/asset work expand to **scene/context → subject → style/medium →
composition/framing → technical quality cues → hard constraints (exclusions)**.
[OpenAI cookbook — GPT Image prompting guide](https://developers.openai.com/cookbook/examples/multimodal/image-gen-models-prompting-guide),
[gpt-image-1.5 prompting guide](https://developers.openai.com/cookbook/examples/multimodal/image-gen-1.5-prompting_guide)

Concrete rules that transfer to our stack:
- **Length:** long prompts work well on gpt-image-class models, BUT structure
  beats volume — "use short labeled segments or line breaks instead of one long
  paragraph"; "prioritize a skimmable template over clever prompt syntax." Start
  from a clean base, refine with small single-change follow-ups. So: a
  medium-length, LABELED, multi-line block, not a 300-word run-on.
- **Negatives:** gpt-image has no negative-prompt field; state exclusions in
  natural language — "no watermark", "no extra text", "no logos", "no extra
  elements". gpt-image (instruction-following) obeys explicit "no X" far better
  than pure-diffusion models did. Caveat: for the background prefer the POSITIVE
  spec ("flat solid magenta #FF00FF background") over "no background", and always
  add "NO drop/contact shadow" for keyable art (matches our own
  `references/verification-and-prompts.md`).
- **Prefix/suffix style locks (our T0208): YES, this is established practice.**
  The icon-set consensus is explicit: "only the subject should be variable and
  inputted by the user; all other style components — tone, style, colors, mood,
  lighting — should be defined as fixed prompts." That is exactly a reusable
  style block. [Structured icon-set prompting](https://starryai.com/en/blog/ai-icon-generator),
  [Zen van Riel — define style once, reference it each time](https://zenvanriel.com/ai-engineer-blog/style-consistency-in-ai-image-generation/)

## Q2 — Reference images: when refs beat text

- **What refs do in gpt-image edit mode:** inputs are processed at high fidelity;
  the model "applies targeted edits while preserving the parts you did not ask to
  change." Style-transfer = "keep the visual language of a reference (palette,
  texture, brushwork, grain) while changing the subject", and you must "describe
  what must stay (style cues) and what must change (new content) + hard
  constraints (background, framing, no extra elements)."
  [Replicate gpt-image-2](https://replicate.com/openai/gpt-image-2),
  [fal — prompting gpt-image-2](https://fal.ai/learn/tools/prompting-gpt-image-2)
- **Refs beat text when the style is hard to name** (exact palette, brush
  texture, line finish) — VLM/text descriptions drift on precisely those. Text is
  enough for coarse style ("flat vector, bold outline"); refs are needed for a
  specific look you must MATCH.
- **How many:** gpt-image accepts multiple inputs; combine "styles, subjects, or
  references into a single output." Community/our-code cap is **≤5**, which is the
  right ceiling — reference each input by index and describe how they interact.
  Same-style exemplar hygiene matters: mixed-style ref packs pull the model in
  conflicting directions.
- **Content vs style vs character refs are different jobs:** a style ref
  (aesthetic, discard subject), a composition/content ref (keep layout), a
  character-sheet ref (keep identity: "lock face/body/proportions, allow only
  outfit"). Say which one each ref is.

## Q3 — Style consistency across an asset SET (the killer problem)

The set-consistency recipe that survives a **no-seed / no-LoRA** stack, strongest
lever first:

1. **Generate the whole set as ONE sheet in a single image, then slice.** All
   tiles share one denoising pass → strongest possible cohesion. Tools do exactly
   this ("generate a set of up to six icons in a consistent style"). **We already
   have slicing** (canvas regions + `crop_regions.py`), so this is our #1 lever.
   Limit: one 1024² frame divides into a few sharp tiles — good for ≤~9 icons,
   then supersample. [Recraft — consistent image sets](https://www.recraft.ai/blog/how-to-create-image-sets)
2. **Anchor image + feed it back as a reference.** Generate/accept ONE hero asset,
   then pass it as an `--input-image` on every sibling: "same art style as the
   reference, new subject = X, do not redesign the style." OpenAI's own guidance:
   build a "character anchor" and "repeat the preserve list on each iteration to
   reduce drift." (Works only on our codex-backend path — see Q1 bug.)
3. **One verbatim style block reused** in every prompt (the T0208 lock) — the
   text baseline that anchors 1 and 2.
4. **Post-gen palette quantization to a locked palette** — snap every result to
   the same N-colour palette so colour never drifts regardless of the roll. This
   is our clean-art ladder step (`T0207`), and is the industry "palette transfer /
   palette lock" move. [Palette lock across sprites](https://spriteflow.io/)

**Seed pinning: does NOT work here — biggest myth to kill.** The codex backend
exposes no seed at all; even on REST proxies "seed reduces variance but does not
pin output — it is not reproducible... if you need the exact same image, cache
the bytes." [Runware gpt-image-2 docs](https://runware.ai/docs/models/openai-gpt-image-2)
Our `gen_hash` sidecar IS that cache. So reproducibility = cache, consistency =
sheet+anchor+block+quantize. **Do not design any feature around a seed.**

Does NOT transfer to us (mark clearly): "train your art style once" tools
(SpriteFlow, Scenario, Layer) are LoRA/fine-tune under the hood — we have no
training path, so ignore that entire class of promise; our only levers are the
four above. [Scenario](https://www.scenario.com/blog/ai-sprite-generator),
[game-art tool survey](https://aloa.co/ai/comparisons/ai-image-comparison/top-ai-art-tools-game-developers)

## Q4 — Style extraction (our "Extract style" button)

Practice is real and well-supported: a VLM reads an exemplar and writes a reusable
style description covering palette, medium, composition, lighting, line weight,
shading, texture, proportions. [MindStudio — reverse-engineer a visual style](https://www.mindstudio.ai/blog/reverse-engineer-ai-image-prompts-chatgpt),
[3D AI Studio image-to-prompt](https://www.3daistudio.com/Tools/ImageToPrompt)

Critical reliability caveat from the research: VLMs "reliably identify lighting
direction, colour palette, composition style, and broad aesthetic categories" but
are "less reliable on exact colour values, specific lens characteristics, or niche
stylistic references." **Consequence for our design: the extracted style preset
must carry BOTH the text description AND the exemplar image itself as ref(s), and
the palette must be SAMPLED from real pixels (we have quantize tooling), never
trusted as VLM-guessed hex.**

## Q5 — Workflow shape

NN/g's field study of real users: generation is an **exploration** stage — people
make **20–80 candidates**, then select one; two strategies: "prompt repetition"
(same prompt, exhaust variations) and "prompt variation" (small prompt tweaks).
Then sequence/refine (inpaint/edit) the winner. [NN/g — 4 stages of AI image gen](https://www.nngroup.com/articles/ai-imagegen-stages/)

Production hygiene: "version control tracks every workflow change... workflows can
be exported as JSON for reuse"; separate what changes from what is invariant and
"restate the invariants on every iteration." [Scaling AI image workflows](https://medium.com/ai-analytics-diaries/the-ai-image-workflow-that-actually-scales-why-generation-is-only-step-one-961a14ed3636)

Maps cleanly onto our card lifecycle draft→generating→done + run history: the
"repetition" strategy = `gen_batch` fan-out of the SAME prompt (each call is
independently random since n=1) to produce a candidate rail; the run record must
store the exact final prompt + refs + model + size (our `gen_hash` inputs) so any
run is reproducible-by-cache and auditable.

---

## Recommended prompt template (what "Expand prompt" should emit)

Emit LABELED segments in this fixed order (skimmable, editable before generation).
The `[STYLE]` and `[CONSTRAINTS]` segments come verbatim from the active style
preset (T0208); the expander fills the rest from the simple prompt.

```
[TASK]        <asset role>, real raster image (not code-drawn).
[SUBJECT]     <who/what, pose, expression, key props — from the user's idea>
[STYLE]       <preset style block: medium, line, shading, palette, proportions>
[COMPOSITION] <framing, view angle, single subject, % of frame, even lighting>
[BACKGROUND]  Flat solid <KEY COLOUR e.g. magenta #FF00FF> background, subject
              fully inside frame, crisp silhouette. NO drop/contact shadow, NO
              ground plane, NO vignette.        (omit + set alpha only on REST)
[CONSTRAINTS] No text, no watermark, no logo, no border/frame, no extra objects.
              Original design only.
[OUTPUT]      <WxH> <format>, <quality>.
```

Worked example — from the Russian input **"рыжий кот с мечом для карточной
игры"** with a "hand-painted card art" preset active:

```
[TASK]        Single character illustration for a 2D card game. Real raster image (not code-drawn).
[SUBJECT]     A ginger tabby cat standing upright on hind legs, gripping a steel
              longsword raised in one paw, confident heroic stance, friendly
              expressive face.
[STYLE]       Hand-painted 2D card-game illustration; clean bold dark outlines;
              soft cel shading, light from top-left; warm saturated palette
              (ginger orange, steel grey, forest green accents); slightly
              oversized head, mascot proportions; smooth matte finish, no grain.
[COMPOSITION] Full body, centered, front three-quarter view, single subject
              occupying ~80% of the frame, even studio lighting, no perspective
              distortion.
[BACKGROUND]  Flat solid magenta (#FF00FF) background, subject fully inside the
              frame, crisp silhouette. NO drop/contact shadow, NO ground plane,
              NO vignette.
[CONSTRAINTS] No text, no watermark, no logo, no card frame or border, no extra
              objects. Original design only.
[OUTPUT]      1024x1024 PNG, high quality.  (generate at 2x, export-fit to box)
```

Notes baked into the emit logic: (a) inject the preset's ref images as
`--input-image` automatically; (b) restate the exclusion list every run; (c) key
colour = magenta/green per our "cutout art on neutral-free bg" rule; (d) supersample
default (generate 2x of the box, export down — clean-art ladder step 1).

## Recommended style-extraction schema (what "Extract style" should save)

```json
{
  "name": "hand-painted card art",
  "style_block": "<the one-line [STYLE] string injected into prompts>",
  "medium": "hand-painted 2D illustration",
  "line": "clean bold dark outline, medium weight",
  "shading": "soft cel shading, light from top-left",
  "palette": {"described": "warm saturated", "swatches": ["#c8631e", ...]},
  "//": "swatches SAMPLED from exemplar pixels via quantize, NOT VLM-guessed",
  "saturation_mood": "warm, high saturation",
  "proportions": "oversized head, mascot",
  "detail_texture": "smooth matte, low grain",
  "finish": "matte, no film grain",
  "bg_convention": "flat solid key colour, no shadow",
  "avoid": ["photoreal", "gradients", "noise", "3D render"],
  "refs": ["files/<exemplar>.png", "..."],   // <=5, the anchor image(s)
  "constraints_block": "No text, no watermark, no logo, no border, original only."
}
```
Fields chosen to match what VLMs describe RELIABLY (palette/lighting/composition/
line/proportions) and to force the two things text alone gets wrong: real-pixel
swatches and the exemplar image carried as a ref.

---

## Verdict on T0239 / T0208

**T0208 (style locks) — mostly right, three corrections.**
- Prefix/suffix + ≤5 refs is validated by practice; ≤5 is the correct cap and
  matches `generate_image.py`. Keep it.
- Correction 1: store the lock as LABELED `[STYLE]` + `[CONSTRAINTS]` segments,
  not an opaque prefix/suffix concat — the model follows labeled structure better.
- Correction 2: **a lock MUST bundle the exemplar image(s) as refs and a
  pixel-sampled palette, not text alone.** Text style descriptions drift; the
  anchor image is the strongest lever we have. This is the schema above.
- Correction 3: "model routing flat→Recraft, painterly→gpt-image" is sound in
  principle (Recraft/vector backends genuinely hold flat-icon consistency better),
  but **we have NO Recraft/vector backend today** — it is aspirational. Until one
  exists, get flat-icon consistency from one-sheet + slice + palette quantize, not
  from routing.

**T0239 (generation placeholder) — good shape, add these fields/behaviors.**
- Expand-prompt: emit the labeled template above; weave the preset `[STYLE]` +
  auto-attach preset refs; RESTATE exclusions; show editable. Do not reference a
  seed anywhere.
- Add card fields: `bg_key` (which key colour), `supersample` (gen at 2x),
  `n_candidates` (implemented as `gen_batch` fan-out of the same prompt, since
  per-call n=1), and a **frozen prompt snapshot per run** (exact final prompt +
  refs + model + size — i.e. the `gen_hash` inputs) so an old run reproduces via
  cache even after the preset later changes. Reuse the existing `gen_hash`
  sidecar as the run's identity/cache key — do not invent a new one.
- Extract-style: save the schema above (text + refs + sampled palette), not a bare
  string; feed both the description AND the exemplar refs into siblings.
- Open question "scale result into box vs resize box": generate at the box's
  aspect (nearest supported gpt-image size) at 2x, then export-fit into the box
  with the existing Lanczos scaler — never distort; prefer resizing the box to the
  result's true aspect if they disagree.
- **Fix the REST refs bug first if refs are to work off an sk- key:** `gen_rest`
  ignores `--input-image` (wrong endpoint). Anchor-reference consistency (Q3
  lever #2) only works on the codex-backend path until `gen_rest` uses the edits
  endpoint. Flag in T0239's design so the card doesn't silently drop refs.

**Biggest correction to the current design:** stop treating seed/determinism as a
consistency mechanism — gpt-image gives none you can use. Consistency for a
matching set comes from (1) one-sheet-then-slice, (2) anchor image fed back as a
ref, (3) one verbatim style block, (4) palette quantization; reproducibility comes
from caching bytes (our `gen_hash`), never a re-roll. Design every card feature
around those four levers, not around pinning a random draw.

---

## Sources
- OpenAI cookbook — GPT Image prompting guide: https://developers.openai.com/cookbook/examples/multimodal/image-gen-models-prompting-guide
- OpenAI cookbook — gpt-image-1.5 prompting guide: https://developers.openai.com/cookbook/examples/multimodal/image-gen-1.5-prompting_guide
- Replicate gpt-image-2 (edit/preserve behavior): https://replicate.com/openai/gpt-image-2
- fal — prompting gpt-image-2: https://fal.ai/learn/tools/prompting-gpt-image-2
- Runware gpt-image-2 docs (seed is not reproducible; cache bytes): https://runware.ai/docs/models/openai-gpt-image-2
- Recraft — consistent image sets: https://www.recraft.ai/blog/how-to-create-image-sets
- StarryAI — structured icon-set prompting (fixed style, variable subject): https://starryai.com/en/blog/ai-icon-generator
- Zen van Riel — style consistency (define style once, reference each time): https://zenvanriel.com/ai-engineer-blog/style-consistency-in-ai-image-generation/
- MindStudio — reverse-engineer a visual style with a VLM: https://www.mindstudio.ai/blog/reverse-engineer-ai-image-prompts-chatgpt
- 3D AI Studio — image-to-prompt: https://www.3daistudio.com/Tools/ImageToPrompt
- NN/g — the 4 stages of AI image generation (20–80 candidate exploration): https://www.nngroup.com/articles/ai-imagegen-stages/
- Scaling AI image workflows / versioning: https://medium.com/ai-analytics-diaries/the-ai-image-workflow-that-actually-scales-why-generation-is-only-step-one-961a14ed3636
- SpriteFlow (palette lock across sprites; LoRA "train once" = does NOT transfer): https://spriteflow.io/
- Scenario / game-art tool survey (fine-tune-based consistency, marked non-transferable): https://www.scenario.com/blog/ai-sprite-generator , https://aloa.co/ai/comparisons/ai-image-comparison/top-ai-art-tools-game-developers
