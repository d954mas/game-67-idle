---
name: generated-game-ui-assets
description: "Use when generating, cutting, validating, integrating, or reviewing reusable game UI asset kits from AI art: UI source sheets, icon sheets, slice9 panels/buttons, art bibles, crop manifests, runtime manifests, chroma/alpha cleanup, contact sheets, pixel audits, responsive UI layout audits, desktop/portrait screenshot proof, or fixing cropped/fringed generated UI assets."
---

# Generated Game UI Assets

Use this skill as the narrow production pipeline for AI-generated runtime UI.
It coordinates `game-visual-art-direction`, `game-asset-pipeline`, and
`game-runtime-automation`; use those skills for their deeper domain details.

## Workflow

1. Read project rules, active task, active project art direction, and current
   screenshots. Do not generate final UI before the target screen and runtime
   harness are known.
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
12. Produce contact sheet, slice9 stretched previews, and a composition proof
   before integration. The composition proof must show base + anchored decor
   overlays + state overlays + runtime text at minimum, normal, large, and at
   least one hostile aspect/portrait size when responsive UI is in scope.
13. Run gates in order:
    - draft contract: `node tools/assets/validate_art_job.mjs --job <job>`
    - source sheet intake before slicing:
      `py -3.12 tools/assets/audit_source_sheet_intake.py --source <source-sheet> --json-output <audit.json> --report <audit.md>`
      Record passing JSON reports in `expected_outputs.source_sheet_intake_audit`
      for final-art claims. Strict validation checks this report if listed;
      final-art validation requires it. The report `source` must match the
      art job's expected source art or crop source. Add
      `--profile --profile-output tmp/asset-profiles/<name>.json` for slow
      intake runs so the sidecar shows stage timings without dirtying durable
      review evidence.
    - strict contract after slicing:
      `node tools/assets/validate_art_job.mjs --job <job> --strict`
      This strict gate requires `expected_outputs.asset_audit` evidence and
      reads JSON audit reports to confirm `verdict: pass` with no listed
      problems, a `crop_manifest` matching the art job, and coverage for every
      crop id in the manifest.
    - final generated/artist art gate before claiming visual completion:
      `node tools/assets/validate_art_job.mjs --job <job> --final-art`
    - pixel audit:
      `py -3.12 tools/assets/audit_generated_ui_assets.py --crop-manifest <crop-manifest> --json-output <audit.json> --report <audit.md>`
      Add `--profile --profile-output tmp/asset-profiles/<name>.json` when a
      generated UI audit feels slow or when comparing extraction fixes; the
      sidecar records per-asset timing and stdout prints the slowest asset
      without churning durable JSON/Markdown evidence. Use `--profile-inline`
      only for throwaway/local debug reports. This audit must fail final
      runtime PNGs whose fully transparent pixels keep any nonzero RGB, not
      only source-key/purple/green classified edge colors.
    - edge proof preview for 1-2px fringe review:
      `py -3.12 tools/assets/render_ui_asset_edge_proof.py --crop-manifest <crop-manifest> --output <edge-proof.png> --json-output <edge-proof.json> --report <edge-proof.md>`
      Add `--profile --profile-output tmp/asset-profiles/<name>.json` when
      edge proofs feel slow or when comparing cleanup fixes; the sidecar
      records total, render-strip, compose, per-asset, and per-side timing plus
      the analysis engine (`numpy` fast path or portable `python` fallback)
      without churning durable JSON/Markdown evidence. Use `--profile-inline`
      only for throwaway/local debug reports.
      Use `--only-problems` for large disputed sheets when the JSON report is
      the coverage evidence and the human PNG should focus only on bad sides.
    - slice9 design policy audit:
      `node tools/assets/audit_slice9_design_policy.mjs --crop-manifest <crop-manifest> --runtime-manifest <runtime-manifest> --json-output <audit.json> --report <audit.md>`
      Add `--profile` when comparing policy changes; it records total and
      per-slice9 timing and prints the slowest asset.
      Record passing reports in `expected_outputs.slice9_design_audit`; final-art
      validation requires this evidence so cuttable but unscalable generated
      panels cannot pass as production UI. If a slice9 base declares
      `non_stretch_ornaments: separate_overlay_assets`, every
      `overlay_asset_id` must reference a real non-slice9 crop/runtime asset.
    - runtime composition proof:
      `py -3.12 tools/assets/render_ui_composition_proof.py --asset-manifest <runtime-manifest> --output <proof.png> --json-output <proof.json> --report <proof.md>`
      Add `--profile --profile-output tmp/asset-profiles/<name>.json` when
      composition proof feels slow; the sidecar writes timing, cache-hit stats,
      and the slowest base/size item is printed without dirtying durable
      JSON/Markdown proof evidence. Use `--profile-inline` only for throwaway
      local debug reports. Overlay sprites
      must not overlap the content safe area unless the layout or overlay explicitly sets
      `allow_content_overlap`.
      For production generated UI layouts, set
      `require_overlay_resize_policy: true` and give every decorative/icon
      overlay exactly one of `size`, `max_size`, or `scale`; the proof report
      must expose overlay source size, rendered size, resize mode, and rect.
      This prevents source-size generated icons or ornaments from silently
      forcing fake oversized panels/buttons.
      Record the proof image and report with preview/review evidence. This gate
      must fail if slice9 margins leave no usable content area at target sizes,
      runtime labels do not fit, overlays fall outside their anchored base, or
      the UI only works as a static source-size crop. Final-art validation
      requires this evidence in `expected_outputs.composition_proof`, and the
      JSON report must point at the same runtime manifest and cover every
      slice9 base id.
    - atlas metadata audit:
      `node tools/assets/audit_atlas_metadata.mjs --asset-manifest <runtime-manifest> --json-output <audit.json> --report <audit.md>`
      Record passing reports in `expected_outputs.atlas_metadata_audit`;
      final-art validation requires this evidence. This gate checks
      `pack_group`, `source_crop`, `original_size`, `trim_rect`, trim mode,
      alpha bleed, premultiplied-alpha handling, extrusion, shape/border
      padding, scale variant, alias links, slice9-safe rotation policy, and
      sprite/decor-overlay placement metadata.
    - review atlas build:
      `py -3.12 tools/assets/build_ui_atlas_pack.py --asset-manifest <runtime-manifest> --output-dir <review-atlas-dir> --json-output <atlas-pack.json> --report <atlas-pack.md> --label-review`
      Add `--profile --profile-output tmp/asset-profiles/<name>.json` when
      optimizing atlas size or slow pack builds; the sidecar records timing
      plus atlas occupancy/asset-area ratios and stdout prints the slowest pack
      group without making the durable review JSON/Markdown dirty on every
      rerun. Use `--profile-inline` only for throwaway/local debug reports.
      The builder must write atlas PNGs, labeled previews, JSON manifests, and
      Markdown reports atomically through temp-file replace so parallel audits
      or human preview cannot read truncated output.
      Record the JSON manifest in `expected_outputs.atlas_pack`; final-art
      validation requires this evidence. This produces grouped review/proof
      atlas PNGs from `pack_group`, preserves slice9/content metadata, writes
      extruded padded rects, reuses alias regions, and can draw id labels in
      reserved `review_label.rect` free space outside each asset `padded_rect`
      so the lead can name which assets to take. Store `review_label.placement`
      as `right` or `bottom` so the preview uses nearby free space without
      covering art. Long labels should keep exact `review_label.text` metadata
      and render wrapped `review_label.lines` in the preview so verbose ids do
      not widen the atlas. Labels must be readable at whole-atlas review size,
      keep a visible outer atlas edge margin so bottom/right labels do not look
      cropped in image viewers, and record `review_label.font_size`; tiny
      debug-font labels are not acceptable review evidence. This is review
      evidence, not the game's final runtime packer. Labeled review packs must
      write `labeled_preview_policy` with `mode:
      label_overlay_only`, `allowed_delta: review_label_rects_only`, and
      `debug_outlines: false` at pack and atlas level. Markdown reports must
      expose the labeled preview path, overlay-only policy, asset id index,
      `review_label.rect`, placement, and wrapped `review_label.lines` so the
      lead can choose assets without opening JSON.
    - review atlas audit:
      `py -3.12 tools/assets/audit_ui_atlas_pack.py --atlas-pack <atlas-pack.json> --asset-manifest <runtime-manifest> --json-output <audit.json> --report <audit.md>`
      Add `--profile --profile-output tmp/asset-profiles/<name>.json` when
      atlas audit feels slow; the sidecar records audit timing and analysis
      engine (`numpy` fast path or portable `python` fallback), and stdout
      prints the slowest atlas group without churning durable review evidence.
      Use `--profile-inline` only for throwaway/local debug reports. For labeled review atlases this audit must
      write JSON/Markdown reports atomically through temp-file replace, and must
      also prove the labeled preview exists, label text matches the asset id
      and linked aliases, wrapped `review_label.lines` fit inside the label
      rect, label rects keep a readable atlas-edge margin, label rects do not
      overlap art or other labels, label rects contain visible pixels there,
      the clean atlas has no label pixels, and fully
      transparent clean-atlas pixels have zero RGB so hidden key colors cannot
      leak back through filtering. The clean atlas must also have no visible
      pixels outside packed `padded_rect`s, so stray labels, stains, or orphan
      sprites cannot masquerade as runtime art. The labeled preview may differ
      from the clean atlas only inside declared `review_label.rect`s, so review
      labels cannot accidentally repaint asset pixels or free atlas space; do
      not draw debug outlines over packed art in the labeled preview. The audit
      must reject labeled review packs that omit or weaken
      `labeled_preview_policy`.
      Record passing JSON reports in `expected_outputs.atlas_pack_audit`;
      final-art validation requires this evidence. This verifies runtime asset
      coverage, atlas bounds, padded-rect overlap, alias reuse, metadata
      consistency, extrusion pixels, and labeled review rects staying outside
      asset `padded_rect`s.
    - source family coverage audit:
      `node tools/assets/audit_source_family_coverage.mjs --job <job> --json-output <audit.json> --report <audit.md>`
      Record passing reports in `expected_outputs.source_family_coverage_audit`;
      final-art validation requires this evidence so one mixed generated sheet
      cannot substitute for separate blank bases, icons, and decor overlays.
      If the runtime manifest intentionally cuts only part of those source
      families, declare `expected_outputs.runtime_scope.mode` as
      `partial_runtime_slice`, with `included_source_families`,
      `deferred_source_families`, and a concrete `reason`. Without that scoped
      exception, final-art validation must fail when required icon or decor
      source families do not have matching runtime-ready crop/runtime assets.
      A scoped pass prints `partial-runtime-slice-valid`; report it as a
      validated partial slice, not as a complete generated UI kit.
      If this audit fails, create a generation prompt queue with
      `node tools/assets/plan_missing_source_family_prompts.mjs --job <job> --coverage-audit <audit.json> --output-dir <project>/art/prompts`
      and generate the missing source families from those packets.
    - generated-source derivation audit:
      `py -3.12 tools/assets/audit_generated_source_derivation.py --crop-manifest <crop-manifest> --json-output <audit.json> --report <audit.md>`
      Record this in `expected_outputs.source_derivation_audit` for final-art
      claims; final-art validation requires a passing JSON report whose
      `crop_manifest` matches the art job and covers every source-derived
      `slice9`, `border`, `tile`, and `sprite` crop id.
    - runtime usage audit after integration:
      `node tools/assets/audit_runtime_ui_asset_usage.mjs --asset-manifest <runtime-manifest> --usage <runtime-usage.json> --json-output <audit.json> --report <audit.md>`
      Create the `game.runtime_ui_asset_usage` file from actual runtime rects
      and layout modes, not intended design sizes. This gate must fail if a
      generated large-only panel/button is drawn below `usage_policy.min_size`
      or used in compact dense UI.
    - native/runtime build for playable work
    - desktop and portrait screenshots when responsive/mobile is in scope
    - product-read gates:
      `node tools/product_gate/review.mjs ...`
    - responsive layout audit when `ui.tree` is available:
      `node tools/product_gate/responsive_layout_audit.mjs --ui-tree <tree.json> --surface portrait --primary <id> --button <id>...`
14. Update the art bible and task log with source sheet, manifests, runtime
    outputs, previews, audits, screenshots, product gates, and remaining gaps.

## Slice9 Rules

- Do not uniform-scale generated panels/buttons in runtime when the UI needs
  resizing. Use slice9 geometry or split corners/edges/center.
- Do not replace failed generated UI with procedural/programmer art and call it
  done. Code-generated art is allowed only as debug scaffolding or a recorded
  exception; final generated UI must come from generated or artist-authored
  source art, with code limited to cutting, validating, packing, and composing.
- If a builder reads a generated sheet and then creates a new panel/button with
  drawing primitives, that output is procedural scaffold, not generated art.
- For generated-source crop manifests, run
  `audit_generated_source_derivation.py` so source-derived PNGs are compared against
  the accepted source crop after chroma cleanup. A pass here proves the builder
  cut the source art; a fail usually means trim/resize policy is missing or the
  builder redrew the asset.
- Keep labels, counters, prices, timers, quest names, and state values in code.
- Keep content safe areas clear of ornate corners and gems.
- Validate minimum sizes: target width must exceed left+right margins and
  target height must exceed top+bottom margins.
- If long-edge ornamentation stretches visibly, regenerate cleaner long edges
  or split decorative caps from stretchable centers.
- Do not bury a usage limitation in prose. If a generated button is only safe
  as a large primary action, set `usage_policy.size_class` to `large_only` and
  list compact button roles in `disallowed_uses`.
- After runtime integration, validate actual placements against `usage_policy`
  with `audit_runtime_ui_asset_usage.mjs`. A desktop screenshot can still be
  wrong if the code squeezes a large-only generated button into a 260x64 rect
  while the manifest says its minimum safe size is 280x104.
- Do not treat one uniform edge-padding threshold as enough. Low controls such
  as buttons may need side-specific padding gates so horizontal ornaments are
  protected without destroying vertical proportions. Large panels and icon
  frames still need all-side padding proof.
- Minimum preview sizes must be product-realistic. If `left + right` or
  `top + bottom` margins leave no center at a listed target size, the asset is
  not valid for that size even if the PNG audit passes.
- Every slice9 base must have a content safe area and target previews that
  include the declared minimum runtime size plus at least one stress size
  around 125% of a minimum dimension. A source-size contact sheet is not enough:
  the design-policy audit should fail missing min/stress preview coverage.
- Keep slice9 base art structurally boring: corners, straight edges, fill, and
  repeatable texture only. Unique center gems, side medallions, banners,
  badges, labels, locks, and cap ornaments must be exported as separate overlay
  sprites with anchors, not baked into the stretchable base texture. Record that
  contract in `stretch_policy`; the audit should fail if the manifest relies on
  chat notes instead of machine-readable policy.
- Treat beautiful fixed decoration as composition data. A panel top plaque,
  side gem, screw, lock, rarity crest, divider, glow strip, or button cap needs
  its own crop id, `anchor`, `z_order`, `allowed_base_ids`, and
  `offset_bounds` min/max rules.
  If it cannot be named as a separate overlay asset id that exists in the
  manifest, it is probably unsafe inside a resizable base.
  `overlay_family` alone is planning prose, not proof: final slice9 policy
  evidence needs concrete `overlay_asset_ids`.
- Runtime composition proof must treat content safe areas as reserved for text,
  prices, counters, and state values. Decorative overlays that cross those
  bounds need explicit `allow_content_overlap`; otherwise the proof should fail
  before integration.
- Progress bars are systems, not one strip: track base, fill strip/tile, left
  cap, right cap, marker/handle, disabled/locked overlay, optional glow, and
  runtime label. Each part needs a semantic id and atlas metadata.

## Atlas And Reuse Rules

- Pack by runtime lifetime and screen family: `ui_common`, `ui_panel_family`,
  `ui_icons_core`, `ui_map`, `ui_fx`, or a project-specific equivalent. Avoid
  one giant atlas when many screens use only a small subset.
- Every atlas/runtime entry needs metadata for `id`, `kind`, `pack_group`,
  source crop, atlas rect, trim/original size, pivot/anchor, slice9 margins,
  content safe area, state role, and source family.
- Run `audit_atlas_metadata.mjs` before treating a generated UI kit as final
  art. The runtime manifest should make trim, bleed, extrusion, padding,
  rotation, scale variant, alias policy, and sprite/decor-overlay placement
  metadata machine-readable.
- Build a labeled review atlas with `build_ui_atlas_pack.py --label-review`
  before final-art claims when the lead needs to inspect outputs. This atlas is
  a proof/contact artifact: it records `atlas_rect`, `padded_rect`, extrusion,
  slice9 margins, content safe areas, source paths, physical entry count, and
  alias count. The labeled preview must put exact asset names in
  `review_label.rect` free space outside the asset `padded_rect`; do not place
  labels over the art or over other labels. Preserve the exact id and alias
  list in `review_label.text`; wrap only the rendered preview text through
  `review_label.lines`; store `review_label.placement` as `right` or `bottom`;
  and keep labels readable enough for the lead to choose assets directly from
  the atlas. The Markdown report must also include a human-readable asset id
  index with the labeled preview path and label rectangles, because the lead
  should be able to say which ids to integrate from one review artifact.
  It must not be presented as the game's final runtime atlas. Use
  `--profile --profile-output tmp/asset-profiles/<name>.json` while optimizing
  atlas economy so telemetry preserves occupancy and padded-asset ratios
  without committing timing-only evidence churn.
- Audit review atlases with `audit_ui_atlas_pack.py`; a proof image is not
  trusted until coverage, bounds, overlap, alias reuse, and extrusion pixel
  checks pass, including exact review-label text and non-overlapping review
  labels. Labeled review atlases must prove wrapped `review_label.lines` fit in
  their label rects and keep labels out of the clean atlas image; labels belong
  only in `labeled_preview_path`. Clean atlas pixels with alpha 0 must also
  have RGB 0, and visible clean-atlas pixels must be inside declared packed
  `padded_rect`s; hidden green/purple key colors under transparency or visible
  orphan pixels in free atlas space are packing failures even when the image
  looks visually acceptable. The labeled preview must be pixel-identical to the
  clean atlas outside declared label rects; it is a review overlay, not a second
  editable atlas, and it should not add debug outlines over assets. The pack
  JSON and each labeled atlas entry must declare the same
  `labeled_preview_policy` so this is machine-readable, not chat context.
- Use trim only with padding, alpha bleed, edge extrusion, and shape padding.
  Tight alpha crops without bleed/extrude are a known cause of 1-2 pixel halos
  and neighboring-pixel leaks.
- Prefer overlays over duplicated full controls. Common variants should be
  base button + state overlay + selected/locked/affordable overlay + icon +
  runtime label unless the material or silhouette truly changes.
- Alias duplicate regions where the same pixels serve different semantic ids.
  Store the semantic ids in metadata instead of duplicating the bitmap. In a
  review atlas, alias entries should point to the same rect as the physical
  source and the physical source label must list linked aliases.
- Record scale variants deliberately (`1x`, `2x`, mobile/desktop). Keep layout
  coordinates and atlas variants stable enough to avoid fractional artifacts.

## Icon And Sprite Rules

- Generated icon sheets need generous gutters. If expanded crop rects catch
  neighboring shadows, reject the source or isolate the intended component.
- Manual crop rectangles are only a starting point. Runtime icon output must
  pass alpha padding, key-fringe, purple edge-halo, transparent-edge RGB bleed
  audit, and fully transparent RGB-zero audit.
- For disputed 1-2 pixel edges, generate an edge proof image with zoomed
  top/right/bottom/left alpha-boundary strips on a checkerboard. The proof
  should mark the same bad edge classes as the pixel audit, including purple
  halo, source-key spill, saturated green-screen spill, and hidden bad RGB in
  transparent edge pixels. Normal contact sheets are too weak for this class of
  defect. Use `--asset-id` and `--side` to create small proof images for the
  exact reported edge. Write `--json-output` and `--report` when comparing
  fixes so the review records per-side counts by reason, not only a screenshot.
  Add `--profile --profile-output tmp/asset-profiles/<name>.json` for slow
  proof runs so the slowest asset side is printed and timing plus the analysis
  engine (`numpy` fast path or portable `python` fallback) is preserved in a
  sidecar instead of dirtying durable JSON/Markdown evidence.
  Add `--only-problems` when a full proof sheet is too tall to review; this
  keeps every side in JSON while omitting clean sides from the PNG/Markdown.
  Store accepted proof image paths in `expected_outputs.edge_proofs` and JSON
  report paths in `expected_outputs.edge_proof_reports` only when
  `counts.total` is zero; reports with bad marks document candidates to reject
  or keep debugging rather than accepted outputs.
- Preserve intentional purple/magic colors with explicit manifest policy; do
  not globally delete interior colors because they resemble the key background.
  `preserve_purple_edges` only suppresses intentional purple/magenta edge
  checks; source-key and green-screen edge leaks must still fail.
- Preserve intentional saturated green edge colors with explicit
  `preserve_green_edges` manifest policy. Otherwise visible green-screen spill
  and hidden green RGB in transparent edge pixels are extraction failures even
  when the crop manifest did not declare `green_screen.key`.
- Reject source sheets where the chroma/key background is too close to the
  intended art palette. Exact key-color pixels inside component bounds are a
  source failure unless deliberately authored and separately masked; broad
  key/halo hue conflicts should be rare and documented.
- Treat visible 1-2 pixel dark purple, dark maroon/magenta, or red-blue edge lines as extraction
  failures, not acceptable polish noise. The audit should catch both bright
  magenta fringe and very dark low-saturation halos on the outer alpha contour,
  including near-black purple pixels such as `#26022d` when they touch transparency.
- Record pivots/anchors before code uses sprites or map markers.

## Responsive UI Rules

- Desktop and portrait are separate compositions using the same reusable
  assets. Do not squeeze desktop HUDs into phone layout.
- Portrait should show fewer simultaneous status values, one full-width primary action,
  secondary actions below, and short journal/objective text.
- A screenshot that looks acceptable is not enough if clickable geometry is
  wrong. Use `ui.tree` layout audit for action bounds when available.
- Product pass requires both player-read evidence and no obvious overlap,
  clipped text, or unusable touch targets.

## Failure Response

If the lead reports cropped icons, key-color outlines, purple edge halo, ugly
UI, unclear first action, or mobile density:

- Stop feature/content expansion.
- Reopen the source sheet, crop manifest, contact sheet, pixel audit, and
  latest desktop/portrait screenshots.
- Fix the earliest failed stage in the pipeline. Do not compensate in runtime
  code for a bad source sheet or missing manifest rule.
- If the fix swaps in procedural shapes or two-color programmer panels, mark it
  as a temporary debug scaffold and reopen source generation. Technical slice9
  correctness is not an art pass; `--final-art` must stay red until generated
  or artist source sheets with complete provenance replace the scaffold.
- Add a validator, audit, or skill rule for any repeated failure before moving
  on.

## Report Shape

Report source art, art bible, crop/runtime manifests, preview sheets, pixel
audit, responsive layout audit, screenshots, product gates, validations run,
and the next visual gap.
