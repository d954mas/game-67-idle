---
name: delegated-image-generation
description: "Use when you (the agent) need to GENERATE real raster art (fake shots, icon/source sheets, sprites, UI art) but have no native image model. PRIMARY path is the OFFICIAL codex CLI imagegen tool via `codex exec` (gpt-image-2, scripts/codex_imagegen.sh) -- sanctioned, free on the ChatGPT plan, real gpt-image-2. agy (Antigravity) is the fallback. Read this BEFORE re-deriving how to call codex/agy for images -- it records the working paths, the must-force-real-generation prompt, the verify-by-size rule, and the dead-ends."
---

# Delegated Image Generation

Use this skill when the agent must create real raster game art but has no native
image model in the current tool surface. Delegate generation, pick the PNG off
disk, verify it visually, then hand it to the normal asset pipeline.

## Load Only What Applies

- `references/generation-paths.md`: exact working commands for Path A official
  `codex exec` imagegen, Path B `agy` fallback, compare mode, and Path C REST or
  codex-backend alternative.
- `references/verification-and-prompts.md`: must-force-real-generation prompt,
  verify-by-size plus eyeball rule, composable asset prompting, dead-ends, and
  environment gotchas.
- `references/throughput-and-handoff.md`: batch generation, source-sheet first
  workflow, skip-if-exists sidecars, hand-off to `generated-game-ui-assets`, the
  `visual gate`, and maintenance rules.

## Default Route

1. Use Path A first: `.codex/skills/delegated-image-generation/scripts/codex_imagegen.sh`.
2. If Path A is unavailable or produces a suspected fake, read the verification
   reference before retrying or falling back.
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
