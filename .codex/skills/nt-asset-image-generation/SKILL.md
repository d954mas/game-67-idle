---
name: nt-asset-image-generation
description: "Use ONLY after a source-first search (shared asset library + free CC0/OFL sources) finds no fit, to generate real raster art for AI Studio asset workflows: fake shots, icon/source sheets, sprites, UI art, or source images for later asset tools. Generation is the last resort, not the default. Prefer Codex CLI imagegen via `codex exec` and `scripts/codex_imagegen.sh`; use Antigravity/agy CLI only as fallback. Owns raster creation only; nt-asset-workflow coordinates asset tools, backlog storage handoff, and ai_studio/quality visual acceptance."
---

# NT Asset Image Generation

Use when the agent must create real raster game art for an asset workflow but
has no native image model. Generate, verify the PNG, then hand off to
`nt-asset-workflow`.

## Load Only What Applies

- `references/generation-paths.md`: Path A `codex exec`, Path B `agy`, compare
  mode, Path C REST/backend, `START=$(date +%s)`, exact commands.
- `references/verification-and-prompts.md`: real raster forcing, size/eyeball
  checks, composable prompts, Dead-ends, gemini CLI, `GEMINI_API_KEY`,
  `NODE_OPTIONS=--use-system-ca`.
- `references/throughput-and-handoff.md`: batches, source-sheet workflow,
  sidecars, `nt-asset-workflow` handoff, and quality evidence.

## Source First (before generating)

Generation is the last resort. First run
`node ai_studio/assets/backlog/storage/search.mjs --query <need>`: reuse a library hit,
or search free CC0/OFL sources and intake one. Only generate what you could not
source, and record the source decision in the task or game-owned asset notes.

## Default Route

1. Use Path A first: `codex_imagegen.sh`.
2. If Path A is unavailable or suspect, read verification guidance before
   retrying or falling back.
3. Use Path B `agy` only as fallback or comparison.
4. Use Path C only when Path A is broken or REST features/high volume are
   explicitly needed.
5. Pick the PNG off disk; keep raw generations in `tmp/`; move only accepted
   assets into durable folders.

## Non-Negotiables

- Generate a real raster image; do not write/run drawing code, SVG, PIL,
  ImageMagick, System.Drawing, shapes, or vectors.
- Never trust the delegated CLI transcript. Verify size and look at the PNG.
- Treat file size only as a fake detector, not as quality proof.
- Do not retry known dead-ends; use documented wrappers and gotchas.
- Generate composable runtime parts unless the task asks for a fake shot/review.
- Clean source images are not enough: assembled screens still need selected
  quality-rule evidence.
- Resolution headroom (anti-noise supersampling): the asset's target in-game
  size must be at most HALF the generated size — use the model's native large
  sizes instead of requesting small ones, and record the target size in the
  canvas element `meta`. Downscaled export (Lanczos) averages VAE noise away.
  If the target is at or near the model's max (e.g. full-screen backgrounds),
  headroom is impossible — say so in the report; the cleanup ladder applies
  instead of silent as-is delivery.

## Canvas Handoff (mandatory)

Every ACCEPTED generation lands on a canvas project (`ai_studio/assets/canvas/`
ops — CLI or direct import), never only in loose files:

- Pick the task-appropriate canvas: the current game's draft canvas, or the
  canvas the task names. If none fits, CREATE a new canvas project titled
  after the task (`cli.mjs create --title "<task>"`).
- Add with full provenance `meta`: `{origin: "ai", tool, prompt, refs,
  generated, license}`. Sourced (non-generated) images get `{origin:
  "sourced", source: <url/path>, license}` instead. The prompt must never be
  lost — the canvas element is its durable home.
- Raw rejects stay in `tmp/`; only accepted results go onto the canvas.

## Minimal Report

State path used, output PNG, size/fake check, visual judgment, canvas project
and element id, handoff path, and rejected attempts or fallbacks.
