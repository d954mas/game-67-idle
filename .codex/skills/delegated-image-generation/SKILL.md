---
name: delegated-image-generation
description: "Use when the agent must GENERATE real raster art: fake shots, icon/source sheets, sprites, or UI art, but has no native image model. Prefer Codex CLI imagegen via `codex exec` and `scripts/codex_imagegen.sh`; use Antigravity/agy CLI only as fallback. Load references for exact commands, real-generation prompts, verify-by-size checks, and dead-ends."
---

# Delegated Image Generation

Use this skill when the agent must create real raster game art but has no native
image model in the current tool surface. Delegate generation, verify the PNG,
then hand it to the normal asset pipeline.

## Load Only What Applies

- `references/generation-paths.md`: Path A `codex exec`, Path B `agy`, compare
  mode, Path C REST/backend, `START=$(date +%s)`, and exact commands.
- `references/verification-and-prompts.md`: real raster image forcing, size
  plus eyeball checks, composable prompts, Dead-ends, gemini CLI, and env
  gotchas like `GEMINI_API_KEY` and `NODE_OPTIONS=--use-system-ca`.
- `references/throughput-and-handoff.md`: batches, source-sheet first workflow,
  sidecars, `generated-game-ui-assets` handoff, and visual gate.

## Default Route

1. Use Path A first: `codex_imagegen.sh`.
2. If Path A is unavailable or suspect, read verification guidance before
   retrying or falling back.
3. Use Path B `agy` only as fallback or comparison.
4. Use Path C only when Path A is broken or REST features/high volume are
   explicitly needed.
5. Pick the PNG off disk, keep raw generations in `tmp/`, and move only
   accepted assets into durable project folders.

## Non-Negotiables

- Generate a real raster image; do not write or run any drawing code, SVG, PIL,
  ImageMagick, System.Drawing, shapes, or vectors.
- Never trust the delegated CLI transcript. Verify size and look at the PNG.
- Treat file size only as a fake detector, not as quality proof.
- Do not retry known dead-ends; use documented wrappers and gotchas.
- Generate composable parts for runtime use, not baked UI composites, unless
  the task asks for a fake shot or review image.
- Clean source images are not enough: assembled screens still need visual gate.

## Minimal Report

State the path used, output PNG path, size/fake check result, visual judgment,
handoff path, and any rejected attempts or fallbacks.
