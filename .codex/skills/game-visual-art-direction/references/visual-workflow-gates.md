# Visual Workflow And Gates

Load this reference when visual quality, generated art, fake shots, UI art,
release-quality presentation, or child-testable polish is part of the task.

## 5-Line Visual Session Contract

Before coding or integrating art, write a 5-line visual session contract:

- goal
- non-goal
- proof
- stop condition
- likely files

The proof must name a native screenshot/product gate/art audit. The stop
condition must say that product gate fail or lead rejection freezes
feature/content expansion unless the lead accepts the debt.

## Reference-Driven Visual Work

If the target is a named reference, final-art generation/integration stays
locked behind reference deconstruction in the active project wiki.

The durable doc must reach its Definition of Ready before final art:

- declare study mode;
- gather evidence through the Source Ladder and Reference Evidence Board;
- use observed frames/screenshots, not secondary summaries;
- cover first screen, control target, response, reward, progression UI, and
  friction;
- record screen grammar, visual composition, reward/UI hierarchy,
  borrow/avoid/copy-risk, and mismatch audit against the current build;
- end in a Reference Digest: mode, sources checked, 3-5 observed visual facts,
  current-build mismatch, borrow/avoid/copy-risk, next native screenshot proof.

Run Reference Intake when the user rejects visuals as unlike the reference.
Parallel reference work may gather images/frames/transcripts/native mismatch
captures, but final reference-driven art stays locked until the digest, mismatch
audit, and next native proof exist.

Full method: `gamedesign/knowledge/reference_deconstruction.md`.

## Screenshot-Vs-Target Loop

Before visual runtime changes, compare the current native screenshot or capture
plan against the accepted target and write a screenshot-vs-target mismatch list.
After each meaningful render change, capture a new native screenshot, update the
named mismatch list, and run or record the product-read gate verdict before
adding more features/content.

For UI-heavy work, define live-state coverage from
`gamedesign/knowledge/live_state_acceptance_matrix.md` and pass it to the
product gate with `--state-matrix`, `--require-state`, `--covered-state`, and
`--not-covered-state`.

## Fast Art Job Loop

Use one art job as the unit of work for generated visual/UI passes:

```text
accepted visual target -> art job -> candidate batch -> selected source sheet
-> crop/slice manifest -> runtime assets/pack -> native screenshot evidence
```

- Generate 3-6 candidates when exploring style or a new asset family.
- Reject obvious failures before slicing or code integration.
- Keep selected source sheets, rejected-output notes, crop boxes, slice9
  margins, pack ids, and screenshot evidence in files referenced by the job.
- When parallelizing, split research, slicing, runtime integration, and visual
  QA by file ownership; keep all lanes writing back to the same job.
- Do not spend runtime/code time polishing an asset family until accepted source
  art and manifest exist.

## Art Request Packet

Create an art request packet before generation when producing more than one
asset or any reusable UI kit. The packet may be JSON or Markdown, but it must be
discoverable in the project.

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
chat history. Scaffold with `tools/assets/job/new_art_job.mjs` and validate in
draft mode before final generation.

Save project-bound generated assets into durable project folders, not only temp
or default generator output paths. Maintain a small runtime asset checklist:
which generated assets are source art, which are cropped/packed runtime assets,
and which screen uses them.

Accepted source sheets need generation records: provider/model or workflow,
workflow file/json, seed or no-seed reason, prompt, negative prompt, source
family role, accepted image path, and rejected candidate notes. Generated or
artist records need real workflow provenance, not empty `{}` placeholders.

## Art-First Gate

Produce visual assets before polishing placeholder render code when the user
asks for generated visual, UI, pretty, beautiful, polished, release-quality, or
child-testable work.

- Do not treat shape-renderer rectangles, debug buttons, raw text panels, or
  programmer art as the main visual solution.
- Placeholder rendering is only a temporary integration layer while real
  generated/runtime assets are prepared.
- If the current screenshot looks like tooling rather than a game, generate or
  improve the art/UI source assets first, then integrate them.
- The first visual pass starts with screenshot-vs-target mismatch, not code
  polish.
- Do not claim progress from technical changes unless the new native screenshot
  closes a named mismatch.
- The main gameplay screenshot must communicate the core action without the
  agent explaining it.

If the screenshot cannot answer where the player is, what to do, what changed,
what reward appeared, and why this looks like a game, the visual work is not
done. Use `node tools/product_gate/review.mjs`, then
`node tools/product_gate/close_slice.mjs` for handoff evidence.

For lead-rejected visual work, tag the task with `lead-rejection` and close only
with `node tools/product_gate/close_slice.mjs --resolved-rejection "<exact rejected issue
and proof>"`. The closeout tool blocks strict pass without that proof.

For beautiful/casual/generated-UI/fake-shot/child-testable work, use
`--visual-strict` and score `composition`, `readability`, `ui_controls`,
`action_direction`, `art_quality`, and `audience_fit`. A pass needs every score
at least 4/5 and no blocker/major visual issue. Strict UI passes should also
include live-state coverage.

When a separate critic pass would help, run the vision art-lead critic with
`node tools/product_gate/visual_critic_run.mjs` (emit a prompt, or run a vision model); feed its
`game.visual_critique` into the strict product gate (`gate ... --critique`).

## Reusable UI Gate

Generated UI for a game must be reusable runtime UI, not only screenshot art.
For full UI-kit production, use `.codex/skills/generated-game-ui-assets/`.

- Keep the generated UI research note in view:
  `gamedesign/sources/generated_game_ui_asset_pipeline_research_2026-06-14.md`.
- Keep the UI/UX production note in view:
  `gamedesign/sources/game_ui_ux_design_guidelines_research_2026-06-14.md`.
- Create or update an art bible before source generation: palette, line weight,
  material language, border thickness, corner style, icon silhouette rules,
  state colors, forbidden motifs.
- Define the screen UX contract before source generation: player intent,
  persistent HUD, contextual HUD, modal surfaces, primary action, feedback,
  blocked/locked state, and first screenshot questions.
- Treat a full generated screenshot as reference/fake shot only. Runtime UI
  must come from separately generated/cropped source families.
- Split runtime UI into screen backgrounds, resizable bases, decor overlays,
  icons, state overlays, runtime text, and hit targets/layout.
- Prefer slice9-ready panels and buttons: separate blank button backgrounds,
  panel frames, corners, edges, and center fills. Record stretch zones, fixed
  ornament policy, minimum usable size, and disallowed uses.
- Do not bake labels, counters, or icons into reusable button backgrounds.
  Generate icons separately and compose them in runtime.
- Do not make a dynamic gameplay board one fixed art image. Generate border,
  tile, highlight, empty-slot, and background parts separately.
- Generate border, tile, highlight, empty-slot, and background families as
  separate runtime parts when the board is dynamic.
- Generate distinct visual states when needed: default/rest, hover/press,
  disabled, affordable, locked, and selected.
- Asset manifests should record slice9 insets, content safe areas,
  `target_preview_sizes`, usage policy, semantic icon roles, pivots/anchors,
  and composition rules, not just PNG paths.
- Reject fused UI, fake text, no gutters, uncuttable ornate panels, inconsistent
  state colors, and decorations occupying text/content safe areas.
- Before slicing, run `normalize_source_sheet_chroma.py` when needed, then
  `audit_source_sheet_intake.py`.
- Require a runtime screenshot/product gate before calling runtime generated
  UI done.
- `node tools/assets/job/validate_art_job.mjs --job <job> --final-art` must
  pass before claiming final generated/artist UI art (it validates generation
  provenance + runtime-ready coverage).

## Generated Asset Rules

- Use the `imagegen` skill/tool for raster art generation unless an asset is
  better produced from existing vector/code-native sources.
- For transparent or crop-ready sprites, prefer a flat chroma-key background
  and local background removal according to the imagegen skill.
- Keep raw generated source art separate from runtime-ready cropped sprites,
  atlases, packs, and generated headers.
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
- locked/unlocked/progress/affordable states are distinct;
- screenshot has no incoherent overlap or cropped important content;
- native/runtime evidence exists for playable screens, not only source art;
- screenshot-vs-target mismatches are updated after the latest render change;
- strict visual product gates record blocker/major issues instead of relying on
  generic "looks better" claims.

## Report Shape

Report generated/source asset paths, runtime asset paths or integration files,
screenshot evidence path, product/readability gate evidence, and the remaining
visual gap before release quality.
