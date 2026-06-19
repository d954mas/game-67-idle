# Asset Source And Cutout Rules

Load this reference when work touches generated source art, crop manifests,
chroma/alpha cleanup, slice9 metadata, review atlases, or final-art claims.

## Source And Runtime Boundary

- Keep raw source assets separate from generated runtime assets.
- Keep project-specific asset generation and generated C/runtime ids separate.
- Do not embed one concept's runtime PNGs or enum ids into another concept's
  generated header/source, even temporarily.
- Crop specs, chroma cleanup, remaps, manifests, source provenance, audit
  commands, and generated C texture arrays belong to a project-specific builder
  such as `tools/assets/build_<game_id>_ui_assets.py`.
- Use `node tools/assets/job/audit_project_asset_boundaries.mjs` to reject
  cross-project ids/imports in builders or generated files.
- Do not reference scratch/temp files as final game assets.
- Generated headers and pack files should be reproducible from source assets and
  builder code.
- If generated files are ignored by git, confirm the build task recreates them.
- If generated files are committed by project convention, keep diffs small and
  explain why.
- A game-local loader shortcut must be justified by a measured failure, missing
  engine capability, or explicit iteration-only boundary.

## Art Job And Provenance

For generated multi-asset work, identify the art job/request packet. If missing,
scaffold it with `tools/assets/job/new_art_job.mjs`.

After accepting generated or artist source art, create a provenance record with
`node tools/assets/job/new_generation_record.mjs` and reference it from the art
job. Procedural/programmer-art scaffolds require an explicit procedural
exception and cannot close a generated-art task. Generated/artist records need a
real workflow path or non-empty workflow JSON; `{}` is only acceptable for
procedural debug records. If no stable seed is exposed, record a no-seed reason
with `--no-seed-reason` instead of an unknown pseudo-seed.

Before asking an image model for a new source family, compile a prompt packet:

```powershell
node tools/assets/job/plan_source_sheet_prompt.mjs --job <art-job> --source-family "<family>" --output <prompt.md> --json-output <prompt.json>
```

Use `--intake-audit` or `--key-color` when a previous source-sheet audit found a
safer chroma key. Prompt packets should preserve `key_color_source`,
`intake_key_color_action`, and machine-readable `source_sheet_layout`. If
`intake_key_color_action` is `split_preserve_or_dual_plate_alpha`, do not create
another normal chroma prompt unless this is an explicit diagnostic override with
`diagnostic_chroma_override: true`.

## Crop And Cutout

- For generated UI, keep crop rectangles, pivots, trim rules, and slice9 margins
  in a manifest; do not preserve them only in chat or screenshots.
- Manual crop rectangles are not enough for generated icon/sprite sheets. Icon
  crops need trim padding and component isolation policy, or an explicit
  no-trim/no-isolation reason.
- Reject source sheets whose icons or small sprites do not have enough gutters
  for safe alpha trim and component isolation. Tight adjacent shadows are a
  source-generation failure, not a runtime integration detail.
- Icon crops need semantic role, source size class, and state/rarity role when
  relevant.
- Sprite/marker crops need pivot or anchor before code uses them.

Chroma-key cleanup must be border-connected so intentional interior colors are
not deleted. Remove antialias/key fringe at transparent edges before packing.
Also audit and remove soft purple edge halos, not only exact chroma-key pixels.
Fill transparent edge RGB with neighboring non-key edge colors as an
intermediate cleanup step and resize/downscale in premultiplied-alpha space.
Before writing final runtime PNGs, scrub fully transparent pixels back to RGB 0.
Reuse `tools/assets/chroma_key_alpha.py` for shared cleanup.

Source-sheet intake must flag unsafe chroma choices before slicing. Exact
key-color inside component bounds can be normal cutout background when it is a
clean internal hole; route that to deliberate `remove_key_holes`/soft-matte
repair and visual proof. Broad key/halo hue conflicts inside material, outlines,
or shadows need safer background color or explicit preserve/masking policy. Use
the intake audit's `next_prompt_key_color` when `key_color_action` is
`regenerate_with_next_prompt_key_color`; if action is
`split_preserve_or_dual_plate_alpha`, stop cycling chroma colors and switch
method.

Normalizing a non-flat chroma background does not make contaminated source art
safe. If green/key pixels remain only in clean background holes, repair them
with explicit holes/soft-matte mode and prove visually. If baked into outlines,
cast shadows, or semitransparent shadow ramps after normalization, mark the
source risky and benchmark repair before accepting it. Prefer safer-key
regeneration, true alpha, split shadow sprites/layers, or dual-plate alpha when
repair damages form.

The principled cutout must resolve one- or two-pixel dark purple, dark
maroon/magenta, red-blue halos, and saturated green-screen spill on the outer
alpha contour at the source, not only bright magenta fringe. Intentional green
edges need `preserve_green_edges`; intentional purple edges need
`preserve_purple_edges`, but preserve policy must not suppress source-key or
green-screen leak removal.

## Slice9 And Composition

Slice9 crops need content safe area, target preview sizes, minimum size
implications, stretch/tile policy, fixed-ornament policy, and disallowed uses.
Separate overlay ornament ids must point to real non-slice9 assets, not prose
notes. If a generated asset is `large_only`, do not use it for compact
secondary/mobile buttons; generate a compact source family or split decoration
into overlay sprites.

Slice9 UI needs runtime composition proof at the smallest supported layout. A
standalone preview can still fail when portrait composition squeezes text or
stacks controls without spacing. Composition proof should fail decorative
overlays that overlap content safe areas unless explicitly allowed.

Use `--profile --profile-output tmp/asset-profiles/<name>.json` on slow
composition proof runs so timing and cache-hit stats show whether the bottleneck
is image loading, repeated slice9 assembly, sheet rendering, or output saving
without dirtying durable proof evidence. Use `--profile-inline` only for
throwaway local debug reports.

## Generated UI Production Gate

The full reusable generated UI checklist lives in
`.codex/skills/generated-game-ui-assets/`. Do not duplicate it here.

From the general asset pipeline side:

- UI kit final art requires the UI skill's source-sheet intake, slicing, pixel,
  edge, derivation, slice9 design-policy, source-family coverage, atlas metadata,
  and review-atlas gates to record passing JSON in the art job's
  `expected_outputs`, and `validate_art_job.mjs --final-art` to pass.
- Procedural/programmer-art source is only a recorded debug exception, not a
  closed generated-art task.
- A full UI kit claim requires runtime-ready assets for every required source
  family. If icons/decor overlays are accepted as source art but not cut into
  runtime manifests, record `partial_runtime_slice`.
- Native screenshot proof exists for playable/native work once integrated, with
  compact portrait composition when mobile/portrait is in scope.
