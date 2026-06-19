---
name: delegated-image-generation
description: "Use when the agent must GENERATE real raster art: fake shots, icon/source sheets, sprites, or UI art, but has no native image model. Prefer Codex CLI imagegen via `codex exec` and `scripts/codex_imagegen.sh`; use Antigravity/agy CLI only as fallback. Load references for exact commands, real-generation prompts, verify-by-size checks, and dead-ends."
---

# Delegated Image Generation

Use this skill when the agent must create real raster game art but has no native
image model in the current tool surface. Delegate generation, pick the PNG off
disk, verify it visually, then hand it to the normal asset pipeline.

## Load Only What Applies

- `references/generation-paths.md`: Path A `codex exec`, Path B `agy`, compare
  mode, and Path C REST/backend commands.
- `references/verification-and-prompts.md`: real-generation forcing, size plus
  eyeball checks, composable prompts, dead-ends, and environment gotchas.
- `references/throughput-and-handoff.md`: batches, source-sheet first workflow,
  sidecars, asset-pipeline handoff, visual gate, and maintenance.

## Default Route

1. Use Path A first: `.codex/skills/delegated-image-generation/scripts/codex_imagegen.sh`.
2. If Path A is unavailable or suspect, read verification guidance before
   retrying or falling back.
3. Use Path B `agy` only as fallback or comparison.
4. Use Path C only when Path A is broken or REST features/high volume are
   explicitly needed.
5. Pick the PNG off disk, store raw generations in `tmp/`, and move only
   accepted durable assets into the project asset/design folder.

## Non-Negotiables

- Generate a real raster image; do not write or run any drawing code, SVG, PIL,
  ImageMagick, System.Drawing, shapes, or vectors as a substitute.
- Never trust the delegated CLI transcript as proof. Verify the output file by
  size and by looking at the PNG.
- Treat file size only as a fake detector, not as quality proof.
- Do not retry known dead-ends such as weak codex prompts or gemini CLI image
  generation; use the documented wrappers and gotchas.
- Generate composable parts for runtime use, not baked UI composites, unless
  the task explicitly asks for a fake shot or review image.
- Clean source images are not enough: assembled game screens still need the
  product/visual gate.

## Minimal Report

State the path used, output PNG path, size/fake check result, visual judgment,
handoff path, and any rejected attempts or fallbacks.
