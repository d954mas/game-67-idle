---
name: game-visual-art-direction
description: Use when defining, generating, reviewing, integrating, or improving game visual direction, art assets, UI kits, fake shots, sprites, icon sets, generated visuals, child-friendly visual polish, release-quality presentation, or replacing placeholder/procedural visuals with production-style bitmap assets.
---

# Game Visual Art Direction

Use this skill when visual quality is part of the game task, especially for
polished prototypes, child-testable builds, generated art, UI, sprites, fake
shots, or release-quality presentation.

For reusable generated runtime UI kits, also use
`.codex/skills/generated-game-ui-assets/`; it owns the end-to-end UI asset kit
workflow from art bible through slice9, pixel audit, responsive layout audit,
and runtime proof.

## Workflow

1. Read local project rules and the active design docs before generating art.
2. Write a 5-line visual session contract before coding or integrating art:
   goal, non-goal, proof, stop condition, and likely files. The proof must name
   a native screenshot/product gate/art audit. The stop condition must say that
   product gate fail or lead rejection freezes feature/content expansion unless
   the lead accepts the debt.
3. Identify the accepted visual target: references, fake shots, art bible,
   lineup, or screenshots. If none exists, create one visual target first.
   If the target is a named reference, keep final-art generation/integration
   locked behind a reference deconstruction in the active project wiki. In
   brief, the durable doc must reach its Definition of Ready before final art:
   declare a study mode; gather evidence through the Source Ladder and the
   Reference Evidence Board (observed frames/screenshots, not secondary
   summaries) for first screen, control target, response, reward, progression
   UI, and friction; record screen grammar, visual composition, reward/UI hierarchy,
   borrow/avoid/copy-risk, and a mismatch audit against the current build; then
   a Reference Digest (mode, sources checked, 3-5 observed visual facts,
   current-build mismatch, borrow/avoid/copy-risk, next native screenshot
   proof). Run Reference Intake when the user rejects the visuals as unlike the
   reference. Parallel reference work (gathering images/frames/transcripts/
   native mismatch captures) may run, but final reference-driven art stays
   locked until the digest, mismatch audit, and next native proof exist.
   Full reference-deconstruction method and gates: `gamedesign/knowledge/reference_deconstruction.md`.
4. Before visual runtime changes, compare the current native screenshot or
   capture plan against the accepted target and write a mismatch list. After
   every meaningful render change, capture a new native screenshot, update the
   mismatch list, and run or record the product-read gate verdict before adding
   more features/content. For UI-heavy work, define live-state coverage from
   `gamedesign/knowledge/live_state_acceptance_matrix.md` and pass it to the
   product gate with `--state-matrix`, `--require-state`, `--covered-state`,
   and `--not-covered-state`.
5. For multi-asset work, create or update an art request packet/art job before
   generation. If no packet exists, scaffold one with
   `tools/assets/new_art_job.mjs`. Record intended use, reusable kind
   (`sprite`, `slice9`, `icon`, `tile`, `border`, `background`,
   `full_mockup`), candidate policy, must-not-bake items, crop ids, expected
   runtime composition, and slice9 insets.
   For generated game UI, do not begin from one full-screen mockup unless it is
   explicitly only a visual target. Start from source families: blank UI kit
   sheet, isolated icon sheet, map/world layer sheet, and sprite/FX sheet.
   The job must pass `node tools/assets/validate_art_job.mjs --job <job>`
   in draft mode before final generation.
6. State the runtime harness separately from the visual source of truth.
   Visual work may use generated images, but playable validation follows the
   project's primary runtime rules.
7. Produce visual assets before polishing placeholder render code when the user
   asks for beautiful, final, generated, release-quality, or child-testable
   visuals.
8. Save project-bound generated assets into durable project folders, not only
   temp or default generator output paths.
9. Inspect generated outputs before integration. Reject assets with unreadable
   text, wrong subject, weak silhouettes, random logos, watermarks, or style
   drift.
   For multi-icon/decor/sprite families, record this as a semantic/style review
   and run `node tools/assets/audit_asset_semantic_style.mjs --review <review.json>`
   before slicing or runtime integration.
   Reject procedural/programmer-art fallbacks as final generated art. They may
   unblock geometry tests, but they do not satisfy a visual-generation task
   unless the lead explicitly accepts a recorded exception.
10. Create or update a small runtime asset checklist: which generated assets are
   source art, which are cropped/packed runtime assets, and which screen uses
   them.
11. Integrate the smallest asset path that proves the real visual direction in
   the primary runtime.
12. Validate with screenshots from the primary runtime and compare against the
    accepted visual target.
13. If the lead rejects the current visuals as ugly, unclear, toy-like,
    debug-looking, or not product-quality, freeze feature expansion. Create a
    short visual/product failure report, a rescue task, and the next screenshot
    proof before writing more gameplay content.

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
- The first visual pass starts with screenshot-vs-target mismatch, not code
  polish. Do not claim progress from technical changes unless the new native
  screenshot closes a named mismatch.
- The main gameplay screenshot must communicate the core action without the
  agent explaining it.
- If the screenshot cannot pass the player-read questions (where am I, what do
  I do, what changed, what reward did I get, why does this look like a game),
  do not call visual work done and do not continue adding content. When
  available, create the durable gate with `node tools/product_gate/review.mjs`
  or `node tools/ai.mjs gate`, then use `node tools/ai.mjs close-slice` for
  handoff evidence. For beautiful, casual, generated-UI, fake-shot, or
  child-testable prototype work, use `--visual-strict` and score all six axes:
  `composition`, `readability`, `ui_controls`, `action_direction`,
  `art_quality`, and `audience_fit`. A pass needs every score at least 4/5 and
  no blocker/major visual issue; otherwise record a fail with concrete
  `--visual-issue` entries and freeze feature/content expansion. Strict UI
  passes should also include live-state coverage; uncovered required states are
  explicit debt, not implied by one screenshot.
  When a separate design/UI critic pass would help, first generate a reusable
  critic packet with `node tools/ai.mjs critic`; use the packet as the critic
  prompt and convert the findings into the strict product gate.

## Reusable UI Gate

Generated UI for a game must be reusable runtime UI, not only screenshot art:

- Keep the current research note in view for generated UI work:
  `gamedesign/sources/generated_game_ui_asset_pipeline_research_2026-06-14.md`.
- Keep the game UI/UX production research note in view for hierarchy,
  decomposition, slice9, atlas, and reuse decisions:
  `gamedesign/sources/game_ui_ux_design_guidelines_research_2026-06-14.md`.
- Create or update an art bible before source generation: palette, line weight,
  material language, border thickness, corner style, icon silhouette rules,
  state colors, and forbidden motifs.
- Define the screen UX contract before source generation: player intent,
  persistent HUD, contextual HUD, modal surfaces, primary action, feedback,
  blocked/locked state, and the first screenshot questions the UI must answer.
- Treat a full generated screenshot as reference/fake shot only. Runtime UI
  must come from separately generated/cropped source families.
- Split runtime UI into explicit families: screen backgrounds, resizable bases,
  decor overlays, icons, state overlays, runtime text, and hit targets/layout.
  Beautiful fixed ornaments belong in overlay sprites with anchors, not in
  slice9 stretch regions.
- Prefer slice9-ready panels and buttons: separate blank button backgrounds,
  panel frames, corners, edges, and center fills where the runtime needs
  resizable elements. Record stretch zones, fixed ornament policy, minimum
  usable size, and disallowed uses; generated buttons that only work as large
  primary controls must not be reused as compact/mobile secondary controls.
- Do not bake labels, counters, or icons into reusable button backgrounds.
  Generate icons separately and compose them in runtime.
- Do not make the full gameplay board one fixed art image when the game needs a
  dynamic board. Generate border, tile, highlight, empty-slot, and background
  parts separately.
- Generate distinct visual states for controls when needed: idle, hover/press,
  disabled, affordable, locked, and selected.
- Asset manifests should record intended slice9 insets, content safe areas,
  target preview sizes, slice9 stretch/overlay usage policy, semantic icon
  roles, pivots/anchors, and composition rules, not just PNG paths.
- Accepted source sheets must also have a generation record: provider/model or
  workflow, workflow file/json, seed or no-seed reason, prompt, negative prompt, source family
  role, accepted image path, and rejected candidate notes. Create it with
  `node tools/assets/new_generation_record.mjs` and reference the record path
  from the art job's `expected_outputs.generation_records`. Generated/artist
  records need real workflow provenance, not empty `{}` placeholders.
- Reject UI sheets that have fused buttons/icons, fake text, no gutters between
  assets, uncuttable ornate panels, inconsistent state colors, or decorations
  occupying the text/content safe area.
- Reject icon/source sheets whose gutters are too small for extraction. A sheet
  that requires overlapping crop rectangles, keeps neighboring shadows, clips
  silhouettes, or leaves chroma-key fringe is not an accepted runtime source
  even if the individual drawings look attractive.
- Accept generated source sheets only after the crop contact sheet shows no
  clipped silhouettes, no adjacent asset fragments, and no visible key-color
  outline at gameplay preview size.
- Before slicing, use `tools/assets/normalize_source_sheet_chroma.py` when a
  generated sheet has non-flat chroma background, then run
  `tools/assets/audit_source_sheet_intake.py` to catch gross source-sheet
  failures: non-flat chroma backgrounds, merged components, clipped components,
  or too-small gutters. A pass here is necessary but not a beauty or slice9-art
  pass.
- For generated UI kits, require both human preview evidence and the pixel
  audit from `tools/assets/audit_generated_ui_assets.py`; a clean-looking
  contact sheet alone is not enough because 1-3 px clipping and chroma fringe
  can survive visual review.
- A technical audit pass is not a beauty pass. If the runtime screenshot reads
  as two-color programmer UI, the art task remains open even if slice9/pixel
  gates pass.
- `node tools/assets/validate_art_job.mjs --job <job> --final-art` must pass
  before claiming a final generated/artist UI art pass. It should fail while
  accepted source records are procedural debug scaffolds or have partial/unknown
  generation provenance.
- Reusable UI kits need composition proof, not only asset proof. Capture at
  least desktop and portrait screenshots when the target includes mobile or
  responsive play. Portrait must be a distinct composition: reduced HUD values,
  one full-width primary action row, secondary actions below, and no dense
  desktop stat strings squeezed into panels.
- When the runtime exposes UI bounds, add a responsive layout audit for the
  key action nodes. Screenshot gates judge player-read; UI-tree audits catch
  compressed touch targets, overlaps, and portrait primary actions that are not
  actually full-width.

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
- screenshot-vs-target mismatches are updated after the latest render change.
- strict visual product gates use the six-axis rubric and record blocker/major
  issues instead of relying on generic "looks better" claims.

## Report Shape

Report:

- generated/source asset paths;
- runtime asset paths or integration files;
- screenshot evidence path;
- what visual gap remains before release quality.
