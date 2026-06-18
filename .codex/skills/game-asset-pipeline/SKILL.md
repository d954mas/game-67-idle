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
   does not exist, scaffold it with `tools/assets/job/new_art_job.mjs` before
   adding crop coordinates or pack ids.
   For generated UI, run
   `node tools/assets/job/validate_art_job.mjs --job <art-job>` before generation
   or slicing. Fix the contract instead of carrying missing decisions in chat.
4. After accepting generated or artist source art, create its provenance record
   with `node tools/assets/job/new_generation_record.mjs` and reference that record
   from the art job. Procedural/programmer-art scaffolds require an explicit
   procedural exception and cannot close a generated-art task. Generated/artist
   records need a real workflow path or non-empty workflow JSON; `{}` is only
   acceptable for procedural debug records. If the provider exposes no stable
   seed, record a no-seed reason with `--no-seed-reason` instead of an unknown
   pseudo-seed.
5. Before asking an image model for a new source family, compile the prompt
   packet from the art job:
   `node tools/assets/job/plan_source_sheet_prompt.mjs --job <art-job> --source-family "<family>" --output <prompt.md> --json-output <prompt.json>`.
   Use `--intake-audit` or `--key-color` when a previous source-sheet audit
   found a safer chroma key. When recording the accepted source, pass the JSON
   prompt packet path to `new_generation_record.mjs --prompt-packet`. Prompt
   packets should preserve `key_color_source`, `intake_key_color_action`, and
   a machine-readable `source_sheet_layout` so color and source positioning
   decisions are not lost. If `intake_key_color_action` is
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
11. For reusable generated runtime UI kits (source sheets, slice9 panels/
    buttons, icons, decor overlays, review atlases), do not re-walk the UI gate
    sequence here. Follow `.codex/skills/generated-game-ui-assets/` for the full,
    ordered UI-asset gate sequence: source-sheet intake, named crop plan, runtime
    asset build, pixel audit, edge proof, generated-source derivation audit,
    slice9 design-policy audit, source-family coverage audit, atlas metadata
    audit, labeled review atlas build/audit, and runtime usage audit. This skill
    owns the general asset hygiene around those gates (locations, provenance,
    pack building) and defers the UI-specific gate details to that skill so the
    sequence is documented in exactly one place.
12. Before claiming a final generated/artist art pass for any asset, run:
    `node tools/assets/job/validate_art_job.mjs --job <art-job> --final-art`.
    This must fail while any runtime source is procedural debug art or has
    partial/unknown generation provenance, and (for UI kits) while the
    UI-asset gates above have not all recorded passing evidence in the job's
    `expected_outputs`.

## Rules

- Keep raw source assets separate from generated runtime assets.
- Keep project-specific asset generation and generated C/runtime ids separate.
  Do not embed one concept's runtime PNGs or enum ids into another concept's
  generated header/source, even as a temporary shortcut. Crop specs, chroma
  cleanup, remaps, manifests, source provenance, audit commands, and generated
  C texture arrays belong to a project-specific builder such as
  `tools/assets/build_<game_id>_ui_assets.py`; do not hide one game's asset
  pipeline inside another game's builder.
- When splitting or auditing this boundary, use
  `node tools/assets/job/audit_project_asset_boundaries.mjs` to reject cross-project
  ids/imports in builders or generated files.
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
  colors as an intermediate cleanup step and resize/downscale in
  premultiplied-alpha space so filtering cannot sample the old key color back
  into visible pixels. Before writing final runtime PNGs, scrub fully
  transparent pixels back to RGB 0. Reuse `tools/assets/chroma_key_alpha.py`
  for this shared cleanup.
- Final review atlas audits must reject hidden RGB under alpha 0 in the clean
  atlas; this catches key-color ghosts that are invisible in the PNG viewer but
  can leak back through filtering or premultiplied conversion. They must also
  reject visible pixels outside declared packed `padded_rect`s; clean atlas free
  space is not a place for labels, stains, or untracked art fragments. Labeled
  previews must be pixel-identical to clean atlases outside label rects, without
  debug outlines over assets. The overlay-only label policy must be declared in
  pack JSON, not carried only in prose or chat context.
- Source-sheet intake must flag unsafe chroma choices before slicing. Exact
  key-color inside component bounds can be normal cutout background when it is a
  clean internal hole; route that to a deliberate `remove_key_holes`/soft-matte
  repair pass and visual proof. Broad key/halo hue conflicts inside material,
  outlines, or shadows need either a safer background color or an explicit
  preserve/masking policy. Use the intake audit's
  `next_prompt_key_color` when `key_color_action` is
  `regenerate_with_next_prompt_key_color`; if the action is
  `split_preserve_or_dual_plate_alpha`, stop cycling chroma colors and switch
  method.
- Normalizing a non-flat chroma background does not make contaminated source
  art safe. If green/key pixels remain only in clean background holes, repair
  them with an explicit holes/soft-matte mode and prove the result visually. If
  they are baked into object outlines, cast shadows, or semitransparent shadow
  ramps after normalization, mark the source as risky and benchmark the repair
  before accepting it. Do not rely only on wider key tolerance or aggressive
  edge deletion; that usually cuts away legitimate soft shadows and material
  shading. Prefer safer-key regeneration, true alpha, split shadow
  sprites/layers, or dual-plate alpha extraction when the repair damages form.
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
  the layout or overlay explicitly allows that overlap. Use
  `--profile --profile-output tmp/asset-profiles/<name>.json` on slow
  composition proof runs so sidecar timing and cache-hit stats show whether the
  bottleneck is image loading, repeated slice9 assembly, sheet rendering, or
  output saving without dirtying durable JSON/Markdown proof evidence. Use
  `--profile-inline` only for throwaway local debug reports.
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

The full per-gate checklist for integrating reusable generated UI assets lives
in `.codex/skills/generated-game-ui-assets/` (its Workflow gate order and
Report Shape). Do not duplicate that checklist here. From the general asset
pipeline's side, hold these cross-cutting rules:

- Treat the UI-asset gate evidence as the integration bar: a UI kit is not
  final art until that skill's source-sheet intake, slicing, pixel, edge,
  derivation, slice9 design-policy, source-family coverage, atlas metadata, and
  review-atlas gates have recorded passing JSON in the art job's
  `expected_outputs`, and `validate_art_job.mjs --final-art` passes.
- Procedural/programmer-art source is only ever a recorded debug exception, not
  a closed generated-art task.
- A full UI kit claim requires runtime-ready assets for every required source
  family; if icons or decor overlays are accepted as source art but not cut
  into the runtime manifests, record the work as `partial_runtime_slice` rather
  than calling it the complete kit.
- Native screenshot proof exists for playable/native work once integrated, with
  a distinct compact portrait composition when mobile/portrait is in scope.

## Pack Builder Changes

When editing a pack builder:

- Add resource ids with stable names.
- Keep cache directories and output paths project-relative.
- Fail loudly on missing required source assets.
- Write generated PNG/JSON/Markdown outputs atomically through a temp file in
  the same directory followed by replace/rename. Audits, previews, or users may
  read files while a build is running; they must see either the old complete
  file or the new complete file, never a truncated image or partial manifest.
- Reuse `tools/assets/atomic_io.py` for Python asset tools instead of copying
  local temp-file helpers into each script.
- Apply the same atomic write rule to audit/report tools, not only pack
  builders; review loops often run build, audit, and preview commands close
  together.
- Print enough output for a user or agent to know what was generated.
