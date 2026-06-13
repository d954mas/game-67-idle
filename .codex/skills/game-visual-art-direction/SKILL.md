---
name: game-visual-art-direction
description: Use when defining, generating, reviewing, integrating, or improving game visual direction, art assets, UI kits, fake shots, sprites, icon sets, generated visuals, child-friendly visual polish, release-quality presentation, or replacing placeholder/procedural visuals with production-style bitmap assets.
---

# Game Visual Art Direction

Use this skill when visual quality is part of the game task, especially for
polished prototypes, child-testable builds, generated art, UI, sprites, fake
shots, or release-quality presentation.

## Workflow

1. Read local project rules and the active design docs before generating art.
2. Identify the accepted visual target: references, fake shots, art bible,
   lineup, or screenshots. If none exists, create one visual target first.
   If the target is a named reference, require a reference deconstruction with
   screenshots/video frames, screen grammar, visual composition, reward/UI
   hierarchy, copy-risk, and mismatch audit against the current build before
   generating or integrating art. For central visual references, require the
   four-pass method: source packet, player transcript, systems extraction, and
   translation gate with the next screenshot proof. Treat the Reference Study
   Definition of Ready as a final-art gate: if the durable doc cannot name the
   source matrix, observed frames/screenshots, current native capture,
   borrow/avoid/copy-risk, current-build mismatch, and next native proof, do not
   generate or integrate final art from that reference yet.
   If the user rejects the visuals as unlike the reference, run Reference Intake
   before another generation/integration pass: state the visual question,
   selected study mode, durable doc path, source packet, current native capture
   plan/path, no-final-art boundary, and first proof screenshot. Label claims as
   observed, inferred, user-provided, or unknown; unknown/inferred visual claims
   cannot become final-art direction without a recorded exception.
   Before generating or integrating final reference-driven art, provide a
   Reference Digest with the mode, sources checked, 3-5 observed visual facts,
   current-build mismatch, borrow/avoid/copy-risk, and next native screenshot
   proof. If the digest is vague, the visual reference is not ready.
   For central/deep visual refs, require the Source Ladder before conclusions:
   user-provided material, official/store/trailer visuals, raw gameplay or
   screenshot evidence, then supporting guides, reviews, lectures,
   deconstructions, wikis, or community notes. Secondary summaries cannot
   replace actual screenshots/frames for composition, control target, reward
   placement, or UI hierarchy claims.
   Central/deep visual refs also need a Reference Evidence Board: cite frames
   or screenshots for first screen, first input/control target, visible
   response, reward feedback, upgrade/progression UI, and friction/blocked
   state before deriving final art direction. If the frames/timestamps cannot
   be named, do not generate or integrate final reference-driven art yet.
   Parallel reference work may gather images, frames, transcripts, and current
   native mismatch captures, but final reference-driven art generation or
   integration stays locked until the Reference Digest, mismatch audit,
   borrow/avoid/copy-risk, and next native proof exist.
3. For multi-asset work, create or update an art request packet/art job before
   generation. If no packet exists, scaffold one with
   `tools/assets/new_art_job.mjs`. Record intended use, reusable kind
   (`sprite`, `slice9`, `icon`, `tile`, `border`, `background`,
   `full_mockup`), candidate policy, must-not-bake items, crop ids, expected
   runtime composition, and slice9 insets.
4. State the runtime harness separately from the visual source of truth.
   Visual work may use generated images, but playable validation follows the
   project's primary runtime rules.
5. Produce visual assets before polishing placeholder render code when the user
   asks for beautiful, final, generated, release-quality, or child-testable
   visuals.
6. Save project-bound generated assets into durable project folders, not only
   temp or default generator output paths.
7. Inspect generated outputs before integration. Reject assets with unreadable
   text, wrong subject, weak silhouettes, random logos, watermarks, or style
   drift.
8. Create or update a small runtime asset checklist: which generated assets are
   source art, which are cropped/packed runtime assets, and which screen uses
   them.
9. Integrate the smallest asset path that proves the real visual direction in
   the primary runtime.
10. Validate with screenshots from the primary runtime and compare against the
    accepted visual target.

## Fast Art Job Loop

Use one art job as the unit of work for generated visual/UI passes:

```text
accepted target -> art job -> candidate batch -> selected source sheet
-> crop/slice manifest -> runtime assets/pack -> native screenshot evidence
```

- Generate 3-6 candidates when exploring style or a new asset family.
- Reject obvious failures before slicing or code integration.
- Keep selected source sheets, rejected-output notes, crop boxes, slice9
  margins, pack ids, and screenshot evidence in files referenced by the job.
- When parallelizing, split research, slicing, runtime integration, and visual
  QA by file ownership, but keep all lanes writing back to the same job.
- Do not spend runtime/code time polishing an asset family until the accepted
  source art and manifest exist.

## Art Request Packet

Create an art request packet before generation when producing more than one
asset or any reusable UI kit. The packet may be JSON or Markdown, but it must be
discoverable in the project and reusable by another agent.

Required fields:

- accepted visual target or reference image;
- asset family and reusable kind;
- candidate policy and selected source sheet path;
- must-not-bake list for labels, counters, icons, state values, and debug text;
- expected crop ids and transparent/chroma-key background rule;
- slice9 insets for resizable UI components;
- expected runtime composition;
- QA rejection rules for unreadable text, fused UI, watermarks, wrong subject,
  random letters, weak silhouette, and style drift.

Do not keep crop coordinates, slice9 margins, or reusable UI composition only in
chat history.

## Art-First Gate

When the user asks for generated visual, UI, pretty, beautiful, polished,
release-quality, or child-testable game work:

- Do not treat shape-renderer rectangles, debug buttons, raw text panels, or
  programmer art as the main visual solution.
- Placeholder rendering is allowed only as a temporary integration layer while
  generated/runtime assets are being prepared.
- If the current screenshot looks like tooling rather than a game, generate or
  improve the art/UI source assets first, then integrate them.
- The main gameplay screenshot must communicate the core action without the
  agent explaining it.

## Reusable UI Gate

Generated UI for a game must be reusable runtime UI, not only screenshot art:

- Prefer slice9-ready panels and buttons: separate blank button backgrounds,
  panel frames, corners, edges, and center fills where the runtime needs
  resizable elements.
- Do not bake labels, counters, or icons into reusable button backgrounds.
  Generate icons separately and compose them in runtime.
- Do not make the full gameplay board one fixed art image when the game needs a
  dynamic board. Generate border, tile, highlight, empty-slot, and background
  parts separately.
- Generate distinct visual states for controls when needed: idle, hover/press,
  disabled, affordable, locked, and selected.
- Asset manifests should record intended slice9 insets or composition rules,
  not just PNG paths.

## Generated Asset Rules

- Use the `imagegen` skill/tool for raster art generation unless the asset is
  better produced from existing vector/code-native sources.
- For transparent or crop-ready sprites, prefer a flat chroma-key background
  and local background removal according to the `imagegen` skill.
- Keep raw generated source art separate from runtime-ready cropped sprites,
  atlases, packs, or generated headers.
- Use stable names such as `67-world-ui-sheet-v1.png`,
  `tiny-67-sprite-v1.png`, or `collection-card-locked-v1.png`.
- Do not reference scratch files as final runtime assets.

## Visual Review Checklist

Before calling visual work done, check:

- art matches the current game concept and audience;
- reference-driven art matches the deconstructed screen grammar, not just the
  theme or asset list;
- every core character has a clear silhouette at gameplay size;
- UI controls look like game objects, not debug buttons;
- text is readable and minimal;
- locked/unlocked/progress/affordable states are visually distinct;
- the screenshot has no incoherent overlap or cropped important content;
- native/runtime evidence exists for playable screens, not only source art.

## Report Shape

Report:

- generated/source asset paths;
- runtime asset paths or integration files;
- screenshot evidence path;
- what visual gap remains before release quality.
