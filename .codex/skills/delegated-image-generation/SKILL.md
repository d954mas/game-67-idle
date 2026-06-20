---
name: delegated-image-generation
description: "Use when the agent must GENERATE real raster art: fake shots, icon/source sheets, sprites, or UI art, but has no native image model. Prefer Codex CLI imagegen via `codex exec` and `scripts/codex_imagegen.sh`; use Antigravity/agy CLI only as fallback. Load references for exact commands, real-generation prompts, verify-by-size checks, and dead-ends. Owns raster CREATION only: cutting/manifests go to generated-game-ui-assets; art-direction judgment to game-visual-art-direction."
---

# Delegated Image Generation

Use when the agent must create real raster game art but has no native image
model. Delegate generation, verify the PNG, then hand off.

## Load Only What Applies

- `references/generation-paths.md`: Path A `codex exec`, Path B `agy`, compare
  mode, Path C REST/backend, `START=$(date +%s)`, exact commands.
- `references/verification-and-prompts.md`: real raster forcing, size/eyeball
  checks, composable prompts, Dead-ends, gemini CLI, `GEMINI_API_KEY`,
  `NODE_OPTIONS=--use-system-ca`.
- `references/throughput-and-handoff.md`: batches, source-sheet workflow,
  sidecars, `generated-game-ui-assets` handoff, and visual gate.

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
- Clean source images are not enough: assembled screens still need visual gate.

## Minimal Report

State path used, output PNG, size/fake check, visual judgment, handoff path, and
rejected attempts or fallbacks.
