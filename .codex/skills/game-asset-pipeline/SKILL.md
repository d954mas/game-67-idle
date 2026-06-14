---
name: game-asset-pipeline
description: Use when adding, converting, packing, referencing, validating, or organizing game assets such as textures, sprites, models, fonts, audio, atlases, generated asset headers, manifests, bundles, or engine-specific asset packs. Triggers include asset build scripts, pack builders, raw versus final asset locations, generated code, missing resources, and runtime asset loading failures.
---

# Game Asset Pipeline

Use this skill to keep source assets, generated outputs, and runtime packs understandable.

For reusable generated runtime UI kits, also use
`.codex/skills/generated-game-ui-assets/`; it coordinates the full source
sheet -> slice9/icon -> audit -> responsive proof workflow.

## Workflow

1. Identify the asset source of truth: `assets/`, `raw/`, `art/`, `gamedesign/art/`, or local equivalent.
2. Identify generated/runtime outputs: pack files, generated headers, atlases, bundles, caches.
3. For generated multi-asset work, identify the art job/request packet. If it
   does not exist, scaffold it with `tools/assets/new_art_job.mjs` before
   adding crop coordinates or pack ids.
   For generated UI, run
   `node tools/assets/validate_art_job.mjs --job <art-job>` before generation
   or slicing. Fix the contract instead of carrying missing decisions in chat.
4. After accepting generated or artist source art, create its provenance record
   with `node tools/assets/new_generation_record.mjs` and reference that record
   from the art job. Procedural/programmer-art scaffolds require an explicit
   procedural exception and cannot close a generated-art task. Generated/artist
   records need a real workflow path or non-empty workflow JSON; `{}` is only
   acceptable for procedural debug records. If the provider exposes no stable
   seed, record a no-seed reason with `--no-seed-reason` instead of an unknown
   pseudo-seed.
5. Before asking an image model for a new source family, compile the prompt
   packet from the art job:
   `node tools/assets/plan_source_sheet_prompt.mjs --job <art-job> --source-family "<family>" --output <prompt.md> --json-output <prompt.json>`.
   Use `--intake-audit` or `--key-color` when a previous source-sheet audit
   found a safer chroma key. When recording the accepted source, pass the JSON
   prompt packet path to `new_generation_record.mjs --prompt-packet`. Prompt
   packets should preserve `key_color_source` and `intake_key_color_action` so
   color decisions are not lost. If `intake_key_color_action` is
   `split_preserve_or_dual_plate_alpha`, do not create another normal chroma
   prompt unless this is an explicit diagnostic override with
   `diagnostic_chroma_override: true`. Strict art-job validation reads that JSON
   and checks its schema, required prompt fields, acceptance checklist, job
   identity, source-family match, and unsafe chroma override policy.
6. Read the existing pack/build script before adding new asset logic.
7. Do not assume the pack/material path is too slow. If the engine or project
   has pack builders and caches, inspect the builder and run or measure the
   smallest pack build before choosing a direct PNG/runtime shortcut.
8. Add the smallest asset path that proves the runtime integration.
9. Regenerate packs with the project task or preset.
10. Verify both generated files and runtime loading behavior when possible.
11. For UI assets, run strict validation after source art is accepted and cut:
   `node tools/assets/validate_art_job.mjs --job <art-job> --strict`.
   Strict mode should pass before runtime integration is considered done. It
   requires `expected_outputs.asset_audit` and reads JSON reports to confirm
   they passed, reference the art job's crop manifest, and cover every crop id.
12. Before claiming a final generated/artist art pass, run:
    `node tools/assets/validate_art_job.mjs --job <art-job> --final-art`.
    This must fail while any runtime source is procedural debug art or has
    partial/unknown generation provenance.
13. Before writing crop rectangles for generated source sheets, run:
    `py -3.12 tools/assets/normalize_source_sheet_chroma.py --source <raw-sheet> --output <clean-sheet>`
    if the source has a non-flat chroma background, then run:
    `py -3.12 tools/assets/audit_source_sheet_intake.py --source <source-sheet>`.
    This catches merged components, clipped items, bad chroma backgrounds, and
    too-small gutters before slicing work begins. Record passing JSON reports
    in `expected_outputs.source_sheet_intake_audit` for final-art claims. The
    report source must match the art job source art or crop source. Add
    `--profile` for slow or disputed intake runs so JSON/Markdown show
    per-stage timing, the analysis engine (`numpy` fast path or portable
    `python` fallback), and the slowest stage.
14. For generated sprites/icons, treat crop extraction as a solved production
    step, not a visual guess: remove background by transparent/alpha or
    border-connected chroma, isolate the intended component, trim to alpha
    bounds, add output padding, remove edge fringe, remove green-screen spill
    even when the crop manifest does not declare a source key, and preview the
    result before runtime use.
    For dual-plate extraction, run
    `py -3.12 tools/assets/dual_plate_alpha.py --light <light.png> --dark <dark.png> --output <rgba.png> --json-output <report.json> --report <report.md> --profile`
    and reject reports whose `verdict` is not `pass`, whose `problems` list is
    non-empty, or where `transparent_nonzero_rgb_pixels` is above zero. Hidden
    RGB under transparent alpha can leak back as 1-2px fringe during
    premultiplied resizing or atlas filtering.
15. For generated UI PNGs, run the pixel audit after slicing:
    `py -3.12 tools/assets/audit_generated_ui_assets.py --crop-manifest <crop-manifest>`.
    The audit should pass before integrating or regenerating runtime headers.
    Add `--profile` for slow or disputed runs so the JSON/Markdown report shows
    per-asset timing and the slowest asset without changing audit verdicts.
16. For 1-2 pixel edge disputes, generate a zoomed edge proof:
    `py -3.12 tools/assets/render_ui_asset_edge_proof.py --crop-manifest <crop-manifest> --output <edge-proof.png> --json-output <edge-proof.json> --report <edge-proof.md>`.
    Add `--profile` for slow or disputed cleanup runs so JSON/Markdown capture
    total, render-strip, compose, per-asset, and per-side timing, and stdout
    prints the slowest asset side.
    Record durable proof paths in the art job (`expected_outputs.edge_proofs`
    for primary outputs or candidate evidence for non-primary candidates), and
    record matching JSON reports in `expected_outputs.edge_proof_reports` so
    strict validation can verify the measured counts beside the image. Only
    accepted reports with zero bad marks belong in `expected_outputs`; failing
    reports stay in candidate/rejected evidence while cleanup continues.
17. For generated-source UI crops, run the derivation audit after slicing:
    `py -3.12 tools/assets/audit_generated_source_derivation.py --crop-manifest <crop-manifest>`.
    This catches builders that read a generated sheet but output procedural
    redraws instead of source-derived PNGs. Record passing JSON reports in
    `expected_outputs.source_derivation_audit` before claiming final art. The
    report crop manifest must match the art job crop manifest and cover every
    source-derived `slice9`, `border`, `tile`, and `sprite` crop id.
18. For generated slice9 assets, run the design-policy audit after crop and
    runtime manifests are updated:
    `node tools/assets/audit_slice9_design_policy.mjs --crop-manifest <crop-manifest> --runtime-manifest <runtime-manifest> --json-output <audit.json> --report <audit.md>`.
    Add `--profile` when comparing policy changes or investigating slow
    manifests; it writes total/per-asset timing and prints the slowest slice9
    asset.
    This gate does not judge beauty; it requires explicit stretch-zone,
    fixed-ornament, overlay, min-size, content safe area, min/stress preview
    coverage, and disallowed-use policy so ornate art is not silently used in
    a size or role where it will stretch badly. Record passing JSON reports in
    `expected_outputs.slice9_design_audit` before final-art validation. If the
    policy declares `non_stretch_ornaments: separate_overlay_assets`, every
    `overlay_asset_id` must reference a real non-slice9 crop/runtime asset.
19. For final generated UI claims, run the source-family coverage audit:
    `node tools/assets/audit_source_family_coverage.mjs --job <art-job> --json-output <audit.json> --report <audit.md>`.
    Jobs should declare `expected_outputs.required_source_families`, usually
    blank UI kit sheet, isolated icon sheet, and UI decor overlay sheet. This
    gate fails one mixed source sheet, candidate/debug records, and missing
    accepted source families. Record passing JSON reports in
    `expected_outputs.source_family_coverage_audit` before final-art
    validation. If the runtime manifest intentionally cuts only part of those
    source families, declare `expected_outputs.runtime_scope.mode` as
    `partial_runtime_slice`, with `included_source_families`,
    `deferred_source_families`, and a concrete `reason`; otherwise final-art
    validation must fail when required icon or decor source families have no
    runtime-ready crop/runtime assets. If it fails, run
    `node tools/assets/plan_missing_source_family_prompts.mjs --job <art-job> --coverage-audit <audit.json> --output-dir <project>/art/prompts`
    so the next generation pass has concrete prompt packets for the missing
    source families.
20. For generated UI review readiness, build the labeled review atlas after
    the runtime manifest passes atlas metadata audit:
    `py -3.12 tools/assets/build_ui_atlas_pack.py --asset-manifest <runtime-manifest> --output-dir <review-atlas-dir> --json-output <atlas-pack.json> --report <atlas-pack.md> --label-review`.
    Add `--profile` while optimizing atlas size or slow pack builds so the
    JSON/Markdown report captures timing, occupancy ratio, and padded-asset
    ratio, and stdout prints the slowest pack group.
    Record the JSON pack manifest in `expected_outputs.atlas_pack`; final-art
    validation requires it for generated UI. The manifest should preserve atlas
    rects, padded rects, extrusion, slice9 margins, content safe areas, source
    paths, alias reuse, label-review purpose, and `review_label.rect` metadata
    for names placed outside asset `padded_rect`s. Long labels should preserve
    exact `review_label.text` metadata and render wrapped `review_label.lines`
    in the preview so verbose ids do not widen the atlas. This is proof/review
    output, not the game's final runtime atlas packer.
21. Audit generated UI review atlases before final-art claims:
    `py -3.12 tools/assets/audit_ui_atlas_pack.py --atlas-pack <atlas-pack.json> --asset-manifest <runtime-manifest> --json-output <audit.json> --report <audit.md>`.
    Add `--profile` when atlas audit feels slow so JSON/Markdown preserve audit
    timing and stdout prints the slowest atlas group.
    Record passing JSON in `expected_outputs.atlas_pack_audit`; final-art
    validation requires it. This catches missing packed assets, out-of-bounds
    rects, padded-rect overlaps, alias mismatches, metadata mismatches, and
    broken extrusion pixels. For labeled review atlases, it also catches labels
    missing from metadata, wrong label text, wrapped `review_label.lines` that
    do not fit their label rects, missing labeled preview images, labels
    overlapping any art or other labels, labels accidentally baked into the
    clean atlas, or label rects without visible pixels in the labeled preview.

## Rules

- Keep raw source assets separate from generated runtime assets.
- Do not reference scratch/temp files as final game assets.
- Generated headers and pack files should be reproducible from source assets and builder code.
- If generated files are ignored by git, confirm the build task recreates them.
- If generated files are committed by project convention, keep diffs small and explain why.
- A game-local loader shortcut must be justified by a measured failure, missing
  engine capability, or explicit iteration-only boundary.
- For generated UI, keep crop rectangles, pivots, trim rules, and slice9 margins
  in a manifest; do not preserve them only in chat or screenshots.
- Manual crop rectangles are not enough for generated icon/sprite sheets. Icon
  crops need a trim/padding rule and component isolation policy, or an explicit
  no-trim/no-isolation reason in the manifest.
- Reject source sheets whose icons or small sprites do not have enough gutters
  for safe alpha trim and component isolation. Tight adjacent shadows are a
  source-generation failure, not a runtime integration detail.
- Chroma-key cleanup must be border-connected so intentional interior colors
  are not deleted. Remove antialias/key fringe at transparent edges before
  packing. Also audit and remove soft purple edge halos, not only exact
  chroma-key pixels. Fill transparent edge RGB with neighboring non-key edge
  colors and resize/downscale in premultiplied-alpha space so filtering cannot
  sample the old key color back into visible pixels. Reuse
  `tools/assets/chroma_key_alpha.py` for this shared cleanup.
- Source-sheet intake must reject unsafe chroma choices before slicing: exact
  key-color holes inside component bounds are not normal background, and broad
  key/halo hue conflicts inside art need either a safer background color or an
  explicit preserve/masking policy. Use the intake audit's
  `next_prompt_key_color` when `key_color_action` is
  `regenerate_with_next_prompt_key_color`; if the action is
  `split_preserve_or_dual_plate_alpha`, stop cycling chroma colors and switch
  method.
- Pixel audits must catch one- or two-pixel dark purple, dark maroon/magenta,
  red-blue halos, and saturated green-screen spill on the outer alpha contour,
  not only bright magenta fringe. Intentional saturated green edges need an
  explicit `preserve_green_edges` manifest policy. Intentional purple edges
  need `preserve_purple_edges`, but that policy must not suppress source-key or
  green-screen leak checks.
- Slice9 crops need more than margins: record content safe area, target preview
  sizes, minimum size implications, whether the center/edges stretch or tile,
  fixed-ornament policy, and disallowed uses. Separate overlay ornament ids
  must point to real non-slice9 assets, not only prose notes. If a generated
  asset is `large_only`, do not use it for compact secondary/mobile buttons;
  generate a separate compact source family or split decoration into overlay
  sprites.
- Slice9 UI needs runtime composition proof at the smallest supported layout.
  A button/panel that passes standalone preview can still fail if portrait
  composition squeezes text or stacks controls without spacing. Composition
  proof should fail decorative overlays that overlap content safe areas unless
  the layout or overlay explicitly allows that overlap. Use `--profile` on
  slow composition proof runs so timing and cache-hit stats show whether the
  bottleneck is image loading, repeated slice9 assembly, sheet rendering, or
  output saving.
- Icon crops need semantic role, source size class, and state/rarity role when
  relevant.
- Sprite/marker crops need pivot or anchor before code uses them.
- For generated art jobs, keep selected source sheets, rejected-output notes,
  runtime asset ids, pack commands, and screenshot evidence referenced from the
  same job contract.
- Keep full screenshots/fake shots separate from source sheets. A screenshot is
  a visual target; crop/slice manifests should point to accepted source sheets
  designed for cutting.

## Generated UI Production Gate

Before UI assets are integrated:

- The art job draft validator passes.
- Accepted generated/artist source sheets have generation records created with
  `tools/assets/new_generation_record.mjs`; procedural source is recorded only
  as a debug exception.
- The final-art validator passes before the work is described as final
  generated or artist art.
- Source-sheet intake audit exists and passes before crop rectangles are
  treated as production slicing data.
- Source families are accepted: blank UI kit, isolated icons, map/world layer,
  and sprite/FX where needed.
- Final UI art has source-family coverage evidence proving blank bases, icons,
  and decor overlays are separate accepted source families rather than one
  mixed sheet.
- The crop manifest has named entries with `id`, `kind`, `rect`, `output`, and
  kind-specific metadata.
- The runtime manifest references every crop output with the same `id`, `kind`,
  and `path`; missing crop outputs are strict-validation failures.
- Slice9 crop/runtime entries have matching `stretch_policy` and `usage_policy`;
  a passing `slice9_design_audit` is required before final generated/artist art
  can be claimed.
- Every `slice9` entry has margins, content safe area, and target preview sizes.
- Every generated `icon` entry has trim padding and component isolation policy
  unless the manifest records why the asset is already clean.
- The runtime manifest references only runtime-ready files, not temp
  generation outputs.
- Labeled review atlas evidence exists for final generated UI claims. It should
  group by `pack_group`, write clean atlas PNGs with extruded padded sprite
  rects, write separate `labeled_preview_path` images for human review, place
  exact names in `review_label.rect` free space outside asset `padded_rect`s,
  list linked aliases on the physical source label, preserve exact
  `review_label.text`, wrap rendered preview names through `review_label.lines`,
  and preserve slice9/content metadata without pretending to be the game's final
  runtime packer. When atlas
  economy or speed is under review, build it with `--profile` so
  occupancy/timing evidence is stored with the pack manifest.
- Atlas pack audit evidence exists and passes before final generated UI claims.
  It should prove coverage, bounds, non-overlap, extrusion pixels, and
  exact non-overlapping review labels. For labeled review atlases, the audit
  should prove that wrapped `review_label.lines` fit their rects and labels are
  visible only in the labeled preview, not in the clean atlas texture.
- A full UI kit claim requires runtime-ready assets for every required runtime
  source family. If icons or decor overlays are accepted as source art but not
  cut into the runtime crop/asset manifests, record the work as
  `partial_runtime_slice` and do not describe it as the complete generated UI
  kit.
- Contact sheet or preview evidence exists for crops and stretched slice9
  states.
- Pixel audit evidence exists for generated runtime PNGs and reports no clipped
  icon alpha bounds, chroma-key edge fringe, purple edge halo, green-screen
  spill, or unsafe transparent-edge RGB.
- Edge proof preview evidence exists when the lead/user reports 1-2 pixel
  fringe, because ordinary contact sheets can hide single-pixel defects.
  Slow edge-proof runs should include `--profile` evidence so bottlenecks are
  visible before rewriting tools.
- Generated-source derivation audit evidence exists when a runtime PNG is
  claimed to come from generated source art rather than a procedural scaffold.
- Native screenshot proof exists for playable/native work once integrated.
- When mobile/portrait is in scope, native portrait screenshot proof exists and
  demonstrates a distinct compact composition rather than a scaled desktop HUD.
- When a DevAPI/UI tree exists, responsive layout audit evidence exists for
  the key slice9 action nodes: minimum touch size, no overlap, and portrait
  primary action above secondary choices.

## Pack Builder Changes

When editing a pack builder:

- Add resource ids with stable names.
- Keep cache directories and output paths project-relative.
- Fail loudly on missing required source assets.
- Print enough output for a user or agent to know what was generated.
