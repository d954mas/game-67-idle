---
name: generated-game-ui-assets
description: "Use when generating, cutting, validating, integrating, or reviewing reusable game UI asset kits from AI art: UI source sheets, icon sheets, slice9 panels/buttons, art bibles, crop manifests, runtime manifests, chroma/alpha cleanup, contact sheets, pixel audits, responsive UI layout audits, desktop/portrait screenshot proof, or fixing cropped/fringed generated UI assets."
---

# Generated Game UI Assets

Use this skill as the narrow production pipeline for AI-generated runtime UI.
It coordinates `game-visual-art-direction`, `game-asset-pipeline`, and
`game-runtime-automation`; use those skills for their deeper domain details.

## References (load only when the row matches the task)

Deep rule manuals live next to this skill so the always-loaded body stays short.
Open the matching section only when the task needs it:

- `references/ui-asset-rules.md` # Slice9 Rules — building/resizing/validating slice9 panels, buttons, progress bars, overlay decoration, `usage_policy`.
- `references/ui-asset-rules.md` # Atlas And Reuse Rules — pack groups, atlas metadata, labeled review atlas, aliasing, trim/bleed/extrusion, scale variants.
- `references/ui-asset-rules.md` # Icon And Sprite Rules — icon gutters, edge-proof review, purple/green edge-color policy, key-color isolation, pivots.
- `references/ui-asset-rules.md` # Responsive UI Rules — desktop vs portrait composition, touch targets, `ui.tree` action-bounds audit.

## Workflow

1. Read project rules, active task, active project art direction, and current
   screenshots. Do not generate final UI before the target screen and runtime
   harness are known.
   For visual-first UI work, write the 5-line session contract first: goal,
   non-goal, proof, stop condition, and likely files. The proof must include
   source/runtime manifests, pixel audit, and native screenshot/product gate
   evidence.
2. Research references first when the visual/gameplay target is new or the
   lead rejected the current result. Store reusable source notes under
   `gamedesign/sources/` and project-specific findings under the active
   project folder. Keep the UI/UX production note in view when designing or
   reviewing reusable game UI:
   `gamedesign/sources/game_ui_ux_design_guidelines_research_2026-06-14.md`.
3. Create or update an art bible before generation. Record palette, materials,
   line weight, border/corner language, icon silhouette rules, mobile density,
   forbidden motifs, and responsive composition rules. Also record the screen
   UX contract: player intent, first readable action, persistent/contextual
   HUD split, modal behavior, reward feedback, blocked/locked state, content
   safe areas, and target desktop/portrait layouts.
4. Create or update one art job packet with:
   `node tools/assets/new_art_job.mjs --id <job-id> --family <family> --project-dir <project-dir>`.
   Keep accepted source sheet paths, expected crop/runtime manifests, and
   commands in that packet. For disputed edge cleanup, record durable edge
   proof images in `expected_outputs.edge_proofs` and matching JSON reports in
   `expected_outputs.edge_proof_reports` only when the report has zero bad
   marks; keep failing reports as candidate/rejected evidence. The packet must
   also record generator provenance:
   provider/model or workflow, workflow file/json, seed or no-seed reason, prompt, negative
   prompt, source family role, accepted source image, and rejected candidate
   notes.
5. After selecting an accepted source sheet, create a generation record with:
   `node tools/assets/new_generation_record.mjs --id <source-id> --project-dir <project-dir> --source-family "<family>" --source-family-role "<role>" --accepted-source <path> --provider <provider> --model <model-or-workflow> --workflow-path <workflow.json> --prompt-packet <prompt.json> --seed <seed> --prompt "<prompt>" --negative-prompt "<negative prompt>"`.
   Add the record path to `expected_outputs.generation_records`. Procedural or
   programmer-art scaffolds must use `--final-art-source procedural` plus
   `--procedural-exception`; they cannot close a generated-art task. Generated
   or artist records need a real `--workflow-path` or non-empty
   `--workflow-json`; placeholder `{}` provenance is not final-art ready. If
   the provider does not expose seed data, use `--no-seed-reason` instead of an
   invented unknown seed. Use canonical `source_family` values such as
   `blank UI kit sheet`, `isolated icon sheet`, or `ui decor overlay sheet`;
   put specific variants in `source_family_role`.
   In strict validation, a referenced JSON prompt packet must parse as
   `game.source_sheet_prompt_packet`, include prompt, negative prompt, source
   family, source family role, and a non-empty acceptance checklist, match the
   art job id/family, and agree with the generation record's source family
   role.
6. Before generating a source family, compile a prompt packet from the art job:
   `node tools/assets/plan_source_sheet_prompt.mjs --job <job> --source-family "<family>" --output <prompt.md> --json-output <prompt.json>`.
   If source intake recommends a safer key color, pass
   `--intake-audit <audit.json>` or `--key-color <#rrggbb>`. Use the packet's
   prompt, negative prompt, and acceptance checklist instead of an ad hoc chat
   prompt. Prompt packets should record `key_color_source` and
   `intake_key_color_action` when an intake audit drives the next key color.
   Prompt packets must also expose `source_sheet_layout`: a row-major
   positioning contract with flat background policy, outer margin, gutter
   minimums, rows/slots, and cut policy. The generator prompt may describe the
   visual style, but the source sheet layout must stay machine-readable so
   later intake/crop planning is not based on chat memory.
   The prompt planner must refuse another chroma prompt when the intake action
   is `split_preserve_or_dual_plate_alpha`; use dual-plate/split workflow
   instead, or mark an intentional diagnostic override explicitly with
   `diagnostic_chroma_override: true`.
   Pass the JSON packet path into `new_generation_record.mjs` with
   `--prompt-packet` so provenance links back to the contract-derived prompt.
7. Generate source families, not one gameplay screenshot or one mixed sheet:
   screen backgrounds, blank resizable bases, isolated icon sheet, UI decor
   overlay sheet, state overlay sheet, bar/progress system sheet, map/world
   layer sheet, sprite/FX sheet if needed. Use full mockups only as visual
   targets; full mockups only as visual targets. Do not claim final generated
   UI from a mixed source sheet. Source-family prompts should position assets
   in clean rows/columns with declared gutters and no composed runtime screen;
   exact crop coordinates are still decided after source-sheet intake, not
   trusted from the generator.
8. Reject generated sheets with baked text, fake letters, fused icons/buttons,
   tight gutters, uncuttable ornate long edges, inconsistent states, weak
   icon silhouettes, watermarks, or chroma background not isolated from art.
   For icon/decor/sprite families, also create a semantic/style review and run
   `node tools/assets/audit_asset_semantic_style.mjs --review <review.json>`
   before crop planning. This gate catches wrong-subject icons, mixed icon
   styles, and fused silhouettes that pixel/chroma audits cannot judge.
9. Run source-sheet intake before slicing:
   if the generated background is visibly or measurably non-flat, first run
   `py -3.12 tools/assets/normalize_source_sheet_chroma.py --source <raw-sheet> --output <clean-sheet>` and keep both raw and clean copies.
   `py -3.12 tools/assets/audit_source_sheet_intake.py --source <source-sheet> --json-output <audit.json> --report <audit.md>`.
   This catches non-flat chroma backgrounds, unsafe key-color holes or hue
   conflicts inside art, merged components, clipped components, and too-small
   gutters; it does not replace human slice9/art review. If a family truly
   needs purple/magic colors inside components, record an explicit preserve
   policy instead of relying on accidental chroma tolerance.
   Use the reported `next_prompt_key_color` as input to the next generation
   prompt when `key_color_action` is `regenerate_with_next_prompt_key_color`.
   If the action is `split_preserve_or_dual_plate_alpha`, stop trying adjacent
   chroma colors and split/preserve the art or switch to dual-plate alpha.
   If exact key-color-like pixels remain inside component bounds after
   normalization, classify them before rejecting the source. Clean internal
   holes are repairable and should use an explicit `remove_key_holes` or
   soft-matte pass plus visual proof on several backgrounds. Key color in
   material, outlines, cast shadows, or semitransparent shadow ramps is risky:
   benchmark repair before accepting it. Do not solve risky cases only by
   widening key tolerance or clearing more edge pixels; that destroys material
   shading and soft shadows. Regenerate with the reported safer key color and
   larger gutters, request true alpha, split shadows into a separate source
   family, or use dual-plate alpha when repair fails visually.
   Use the key color from the prompt packet when auditing generated sheets.
   Add `--profile --profile-output tmp/asset-profiles/<name>.json` when
   source-sheet intake feels slow or when comparing component detection fixes;
   the sidecar records per-stage timing, the analysis engine (`numpy` fast
   path or portable `python` fallback), and the slowest stage without churning
   durable JSON/Markdown evidence. Use `--profile-inline` only for
   throwaway/local debug reports.
   Intake JSON/Markdown must expose `problem_summary` and
   `recommended_next_step` so key-color failures route quickly to either a
   safer prompt key, more gutter/border regeneration, or dual-plate/split
   alpha extraction instead of burying the decision in many component errors.
   Built-in image generation can produce a visually flat but pixel-varied
   chroma background; normalize it before intake instead of hand-tuning crop
   boxes around detector noise. Small satellite pieces of one icon can be
   merged by the intake audit, but full-size neighboring assets must still meet
   gutter rules.
   If a family repeatedly fails key-color fringe audits, switch the next prompt
   packet to a dual-plate alpha job instead of trying more chroma colors: one
   light-background plate and one dark-background plate, same dimensions and
   pixel-aligned subject, followed by difference matting, blob cleanup, alpha
   hardening, and the same crop/slice/runtime audits. Use
   `py -3.12 tools/assets/dual_plate_alpha.py --light <light.png> --dark <dark.png> --output <rgba.png> --json-output <report.json> --report <report.md> --profile`
   for the deterministic extraction step. The report must show `verdict:
   pass`, no problems, and `transparent_nonzero_rgb_pixels: 0`; hidden RGB
   under transparent alpha can reappear as 1-2px fringe after premultiplied
   resize or atlas filtering.
   When intake passes for a multi-component icon/decor/sprite source sheet,
   create a named crop plan from the detected components before hand-writing
   crop rectangles:
   `py -3.12 tools/assets/plan_runtime_crops_from_intake.py --intake-audit <audit.json> --ids-file <ids.txt> --kind <icon|decor|sprite> --source-id <source-id> --source-role <role> --output-dir <runtime-dir> --json-output <crop-plan.json> --report <crop-plan.md>`.
   Keep ids in visual row-major order in an ids file so long source sheets do
   not depend on fragile long CLI strings, and review the Markdown plan before
   writing the runtime crop manifest.
10. Slice from a manifest. Every runtime UI asset needs a named crop entry:
   `id`, `kind`, `rect`, `output`; slice9 entries also need `slice9`,
   `content`, and `target_preview_sizes`; icons need semantic role, size class,
   trim padding, and component isolation policy.
   For icon/decor/sprite crop plans produced from intake components, build
   runtime PNGs and manifests with:
   `py -3.12 tools/assets/build_runtime_assets_from_crop_plan.py --crop-plan <crop-plan.json> --crop-manifest <crop-manifest.json> --asset-manifest <asset-manifest.json> --art-job <job.json> --contact-sheet <contact.png>`.
   This step must preserve generated source pixels, remove border-connected
   chroma, trim to alpha bounds with padding, scrub transparent RGB, and write
   atlas metadata so the normal pixel/atlas audits can run next.
   The runtime manifest must reference every crop output with the same `id`,
   `kind`, and `path`; strict validation rejects missing or mismatched runtime
   assets. Slice9 crop and runtime entries also need explicit `stretch_policy`
   and `usage_policy`: what can stretch/tile, where fixed ornaments live, the
   minimum runtime size, and disallowed uses such as compact secondary buttons.
   Do not integrate or claim runtime generated UI from an empty crop manifest,
   empty runtime manifest, or unrun pixel audit. The order is source sheet,
   intake, crop manifest, runtime manifest/assets, pixel audit, and only then
   runtime integration.
11. Build runtime PNGs deterministically. For chroma-key art, remove only
   border-connected key color, isolate intended icon components, trim by alpha
   bounds, add padding, remove edge fringe, remove source-key color spill
   according to the manifest key color, remove green-screen spill even when the
   manifest does not declare a source key, bleed non-key edge RGB into
   transparent pixels, and resize previews in premultiplied-alpha space before
   packing. Reuse `tools/assets/chroma_key_alpha.py` for this cleanup instead
   of duplicating local keying logic in builders and audits.
   Alpha bleed can be an intermediate cleanup step, but final runtime PNGs and
   clean atlases must scrub `alpha == 0` pixels back to RGB 0 so hidden colors
   cannot reappear during premultiplied conversion, mip/filtering, or atlas
   repacking.
   Runtime builders for generated-source art must not redraw panels, buttons,
   icons, or textures with procedural shapes and present them as generated
   outputs; procedural drawing is allowed only for debug overlays, labels,
   contact-sheet backgrounds, or explicitly recorded scaffold exceptions.
   Cleanup is a hygiene step for otherwise valid source art. It is not a way to
   rescue source sheets where green spill is baked into semitransparent shadows
   or object shading; those must go back to source generation, true alpha,
   split layers, or dual-plate alpha.
12. Produce contact sheet, slice9 stretched previews, and a composition proof
   before integration. The composition proof must show base + anchored decor
   overlays + state overlays + runtime text at minimum, normal, large, and at
   least one hostile aspect/portrait size when responsive UI is in scope.
13. Run the gates by tier; do NOT walk the whole battery every iteration.
    The gates split into three tiers (see "Gate Tiers" below). A normal UI
    iteration runs only DRAFT, plus INTEGRATE when wiring an asset into the
    runtime screen. The full FINAL-ART battery is opt-in and only required when
    shipping a reusable kit / claiming visual completion. To see the exact
    ordered command sequence for a tier without running any image tool, use the
    thin orchestrator:
    `node tools/assets/run_ui_asset_tier.mjs --tier draft|integrate|final --plan --job <job> --crop-manifest <crop-manifest> --runtime-manifest <runtime-manifest> --source-sheet <source-sheet>`.
14. Update the art bible and task log with source sheet, manifests, runtime
    outputs, previews, audits, screenshots, product gates, and remaining gaps.

## Gate Tiers

Tier the gates by what the iteration is actually doing. A normal UI iteration
runs only DRAFT, plus INTEGRATE when wiring the asset into the runtime screen.
It does NOT run the full battery. The full FINAL-ART battery is opt-in and
required only when shipping a reusable kit / claiming visual completion. Use
`run_ui_asset_tier.mjs --plan` to print the exact command sequence for a tier.

Per-iteration cost: a normal iteration is ~2 commands (DRAFT) or ~5 commands
(DRAFT + INTEGRATE), not the ~14-gate / 17-report final battery.

### DRAFT tier (every iteration, cheap, <=2 commands)

Goal: see the asset in context fast. Run only:

- source-sheet intake (and normalize first if the background is non-flat):
  `py -3.12 tools/assets/normalize_source_sheet_chroma.py --source <raw-sheet> --output <clean-sheet>` (only when needed), then
  `py -3.12 tools/assets/audit_source_sheet_intake.py --source <source-sheet> --json-output <audit.json> --report <audit.md>`
- build runtime PNGs + a contact sheet to eyeball the cut:
  `py -3.12 tools/assets/build_runtime_assets_from_crop_plan.py --crop-plan <crop-plan.json> --crop-manifest <crop-manifest.json> --asset-manifest <asset-manifest.json> --art-job <job.json> --contact-sheet <contact.png>`

The draft contract `node tools/assets/validate_art_job.mjs --job <job>` (no
flags) is a cheap structural check you may also run here. Do not run the strict
or final-art battery, the pixel audit, or any proof PNG at this tier.

### INTEGRATE tier (when wiring into the runtime screen, ~3 commands)

Goal: prove the cut asset is clean and composes at real sizes before it lands
in the screen. Run, in order:

- strict contract after slicing:
  `node tools/assets/validate_art_job.mjs --job <job> --strict`
  This strict gate requires `expected_outputs.asset_audit` evidence and reads
  JSON audit reports to confirm `verdict: pass` with no listed problems, a
  `crop_manifest` matching the art job, and coverage for every crop id in the
  manifest.
- pixel audit:
  `py -3.12 tools/assets/audit_generated_ui_assets.py --crop-manifest <crop-manifest> --json-output <audit.json> --report <audit.md>`
  This audit must fail final runtime PNGs whose fully transparent pixels keep
  any nonzero RGB, not only source-key/purple/green classified edge colors.
- runtime composition proof:
  `py -3.12 tools/assets/render_ui_composition_proof.py --asset-manifest <runtime-manifest> --output <proof.png> --json-output <proof.json> --report <proof.md>`
  Overlay sprites must not overlap the content safe area unless the layout or
  overlay explicitly sets `allow_content_overlap`. For production layouts set
  `require_overlay_resize_policy: true` and give every decorative/icon overlay
  exactly one of `size`, `max_size`, or `scale`. This gate must fail if slice9
  margins leave no usable content area at target sizes, runtime labels do not
  fit, overlays fall outside their anchored base, or the UI only works as a
  static source-size crop.

### FINAL-ART tier (only when shipping a reusable kit)

Goal: a complete, reusable, production generated UI kit. This is the full
battery and is the only tier that runs `--final-art`. Run all of:

- source-sheet intake recorded in `expected_outputs.source_sheet_intake_audit`
  (final-art validation requires it; the report `source` must match the art
  job's expected source art or crop source).
- strict contract (as above).
- pixel audit (as above).
- edge proof preview for 1-2px fringe review (conditional, see below):
  `py -3.12 tools/assets/render_ui_asset_edge_proof.py --crop-manifest <crop-manifest> --output <edge-proof.png> --json-output <edge-proof.json> --report <edge-proof.md> --only-problems`
  Store accepted proof image paths in `expected_outputs.edge_proofs` and JSON
  report paths in `expected_outputs.edge_proof_reports` only when `counts.total`
  is zero.
- slice9 design policy audit:
  `node tools/assets/audit_slice9_design_policy.mjs --crop-manifest <crop-manifest> --runtime-manifest <runtime-manifest> --json-output <audit.json> --report <audit.md>`
  Record passing reports in `expected_outputs.slice9_design_audit`; final-art
  validation requires this evidence so cuttable but unscalable generated panels
  cannot pass as production UI. If a slice9 base declares `non_stretch_ornaments:
  separate_overlay_assets`, every `overlay_asset_id` must reference a real
  non-slice9 crop/runtime asset.
- runtime composition proof recorded in `expected_outputs.composition_proof`;
  the JSON report must point at the same runtime manifest and cover every
  slice9 base id.
- atlas metadata audit:
  `node tools/assets/audit_atlas_metadata.mjs --asset-manifest <runtime-manifest> --json-output <audit.json> --report <audit.md>`
  Record passing reports in `expected_outputs.atlas_metadata_audit`. This gate
  checks `pack_group`, `source_crop`, `original_size`, `trim_rect`, trim mode,
  alpha bleed, premultiplied-alpha handling, extrusion, shape/border padding,
  scale variant, alias links, slice9-safe rotation policy, and
  sprite/decor-overlay placement metadata.
- review atlas build:
  `py -3.12 tools/assets/build_ui_atlas_pack.py --asset-manifest <runtime-manifest> --output-dir <review-atlas-dir> --json-output <atlas-pack.json> --report <atlas-pack.md> --label-review`
  Record the JSON manifest in `expected_outputs.atlas_pack`. The builder writes
  atlas PNGs, labeled previews, JSON manifests, and Markdown reports atomically
  through temp-file replace. Labeled review packs must write
  `labeled_preview_policy` with `mode: label_overlay_only`, `allowed_delta:
  review_label_rects_only`, and `debug_outlines: false` at pack and atlas level;
  put exact asset names in `review_label.rect` free space outside the asset
  `padded_rect`, store `review_label.placement` as `right` or `bottom`, wrap
  long labels through `review_label.lines`, and keep `review_label.font_size`
  readable. This is review evidence, not the game's final runtime packer.
- review atlas audit:
  `py -3.12 tools/assets/audit_ui_atlas_pack.py --atlas-pack <atlas-pack.json> --asset-manifest <runtime-manifest> --json-output <audit.json> --report <audit.md>`
  Record passing JSON reports in `expected_outputs.atlas_pack_audit`. It proves
  runtime asset coverage, atlas bounds, padded-rect overlap, alias reuse,
  metadata consistency, extrusion pixels, that the clean atlas has no label
  pixels and fully transparent clean-atlas pixels have zero RGB, and that
  labeled review rects stay outside asset `padded_rect`s. The labeled preview
  may differ from the clean atlas only inside declared `review_label.rect`s.
- source family coverage audit:
  `node tools/assets/audit_source_family_coverage.mjs --job <job> --json-output <audit.json> --report <audit.md>`
  Record passing reports in `expected_outputs.source_family_coverage_audit` so
  one mixed generated sheet cannot substitute for separate blank bases, icons,
  and decor overlays. For an intentional partial cut, declare
  `expected_outputs.runtime_scope.mode` as `partial_runtime_slice` with
  `included_source_families`, `deferred_source_families`, and a concrete
  `reason`; a scoped pass prints `partial-runtime-slice-valid`. If this audit
  fails, queue prompts with
  `node tools/assets/plan_missing_source_family_prompts.mjs --job <job> --coverage-audit <audit.json> --output-dir <project>/art/prompts`.
- generated-source derivation audit:
  `py -3.12 tools/assets/audit_generated_source_derivation.py --crop-manifest <crop-manifest> --json-output <audit.json> --report <audit.md>`
  Record this in `expected_outputs.source_derivation_audit`; final-art
  validation requires a passing JSON report whose `crop_manifest` matches the
  art job and covers every source-derived `slice9`, `border`, `tile`, and
  `sprite` crop id. This is what proves source-derived PNGs were cut from the
  accepted source rather than redrawn.
- runtime usage audit after integration:
  `node tools/assets/audit_runtime_ui_asset_usage.mjs --asset-manifest <runtime-manifest> --usage <runtime-usage.json> --json-output <audit.json> --report <audit.md>`
  Build `game.runtime_ui_asset_usage` from actual runtime rects and layout
  modes. This gate fails if a large-only panel/button is drawn below
  `usage_policy.min_size` or used in compact dense UI.
- final generated/artist art gate (the single verdict for the whole kit):
  `node tools/assets/validate_art_job.mjs --job <job> --final-art`
- native/runtime build for playable work; desktop and portrait screenshots when
  responsive/mobile is in scope; product-read gates
  `node tools/product_gate/review.mjs ...`; responsive layout audit when
  `ui.tree` is available
  `node tools/product_gate/responsive_layout_audit.mjs --ui-tree <tree.json> --surface portrait --primary <id> --button <id>...`.

### Conditional proofs (do not render proof PNGs per asset per iteration)

The edge-proof and composition-proof PNG artifacts exist for failure
investigation, not as per-iteration deliverables:

- Generate the edge-proof PNG only at FINAL-ART tier or when investigating a
  reported 1-2px fringe. `render_ui_asset_edge_proof.py --only-problems` keeps
  full JSON coverage but renders only the bad sides into the PNG/Markdown, so a
  clean kit produces no per-asset proof noise.
- The composition proof writes a PNG and FAILS on problems by default; that
  failing image IS the on-failure artifact. Do not also bank a passing proof
  PNG per asset per iteration. `--no-fail` only exists to capture an image when
  you deliberately want one despite problems.
- Profiling sidecars (`--profile --profile-output tmp/asset-profiles/<name>.json`,
  `--profile-inline` only for throwaway debug) stay opt-in for slow runs and
  must not churn durable JSON/Markdown review evidence.

## Deep Rules (in references/ui-asset-rules.md)

The slice9, atlas/reuse, icon/sprite, and responsive-UI rule manuals moved to
`references/ui-asset-rules.md` to keep this body short. Load the matching
section before doing that kind of work. Two contracts stay in the body because
the workflow above leans on them every responsive iteration:

- Desktop and portrait are separate compositions using the same reusable
  assets; do not squeeze desktop HUDs into phone layout.
- Portrait shows fewer simultaneous status values, one full-width primary action,
  secondary actions below, and short journal/objective text. A passing
  screenshot is not enough when clickable geometry is wrong — use the `ui.tree`
  action-bounds audit. Everything deeper (slice9 `usage_policy`, atlas pack
  metadata, edge-color/key isolation policy) lives in the reference manual.

## Failure Response

If the lead reports cropped icons, key-color outlines, purple edge halo, ugly
UI, unclear first action, or mobile density:

- Stop feature/content expansion.
- Reopen the source sheet, crop manifest, contact sheet, pixel audit, and
  latest desktop/portrait screenshots.
- Fix the earliest failed stage in the pipeline. Do not compensate in runtime
  code for a bad source sheet or missing manifest rule.
- For green halos around outlines, holes, shadows, or semishadows, check the
  source-sheet intake first. Clean holes should be removed by an explicit
  holes/soft-matte extraction mode. For outlines, shadows, semishadows, or
  material contamination, render a visual cutout benchmark; if repair damages
  form or shading, regenerate/resheet or switch to dual-plate/true alpha.
- If the fix swaps in procedural shapes or two-color programmer panels, mark it
  as a temporary debug scaffold and reopen source generation. Technical slice9
  correctness is not an art pass; `--final-art` must stay red until generated
  or artist source sheets with complete provenance replace the scaffold.
- Add a validator, audit, or skill rule for any repeated failure before moving
  on.
- "Generated/free assets allowed" means a polished runtime set, not permission
  to ship rough crops. Per-crop pixel/edge/transparency audits prove crop
  hygiene, not art quality. A clean crop of the wrong or rough art still fails
  the screen: the binding check is the assembled screen vs the art bible /
  fake shot (AGENTS.md definition of done), not the per-crop audits.

## Report Shape

Report source art, art bible, crop/runtime manifests, preview sheets, pixel
audit, responsive layout audit, screenshots, product gates, validations run,
and the next visual gap.
