# Tools Layout

`tools/` intentionally contains several layers. Do not treat every file here as
equally portable. This file is the source of truth for the layers; the sections
below describe what moves to a new game vs what stays project-specific.

## Tool Contract

AI tooling must help the game work move faster. Defaults should be quiet,
bounded, and advisory:

- default commands print short actionable output;
- slow, broad, destructive, artifact-generating, or deep-retrospective behavior
  must require an explicit flag or explicit user/task need;
- scripts must not turn stale generated diagnostics into blockers for normal
  game work;
- generated scratch outputs go under `tmp/` or another ignored path unless the
  lead explicitly promotes them;
- validation scripts should run the narrow proof first and reserve broad/final
  checks for release, portable-base, or shared-behavior changes;
- a script that finds a problem should report the next useful action, not create
  a new process obligation by default.

When adding or changing a tool, prefer `summary`/passive output as the default
and `--verbose`, `--deep`, `--all`, or an explicit subcommand for exhaustive
inspection.

## Layers

### `portable_ai_pipeline`

Generic workflow tools for AI-assisted development. These move to a clean new
game project:

- `tools/ai.mjs` - fast facade for `start`, `focus`, `context`, `checkpoint`,
  `run`, `validate`, `status`, and `reflect`
- `tools/ai.test.mjs`
- `tools/ai_profile/`
- `tools/taskboard/`
- `tools/game_context/`
- `tools/product_gate/`
- `tools/bootstrap/export_base.mjs`
- `tools/pipeline_validate.mjs`
- `tools/skills_eval.mjs`
- `tools/skills_sync.mjs`
- `tools/assets/new_art_job.mjs`
- `tools/assets/plan_source_sheet_prompt.mjs`
- `tools/assets/plan_missing_source_family_prompts.mjs`
- `tools/assets/new_generation_record.mjs`
- `tools/assets/validate_art_job.mjs`
- `tools/assets/audit_slice9_design_policy.mjs`
- `tools/assets/audit_atlas_metadata.mjs`
- `tools/assets/build_ui_atlas_pack.py`
- `tools/assets/audit_ui_atlas_pack.py`
- `tools/assets/audit_runtime_ui_asset_usage.mjs`
- `tools/assets/audit_source_family_coverage.mjs`
- `tools/assets/chroma_key_alpha.py`
- `tools/assets/dual_plate_alpha.py`
- `tools/assets/normalize_source_sheet_chroma.py`
- `tools/assets/audit_source_sheet_intake.py`
- `tools/assets/plan_runtime_crops_from_intake.py`
- `tools/assets/build_runtime_assets_from_crop_plan.py`
- `tools/assets/audit_generated_ui_assets.py`
- `tools/assets/render_ui_asset_edge_proof.py`
- `tools/assets/render_ui_composition_proof.py`
- `tools/assets/audit_generated_source_derivation.py`

`tools/product_gate/review.mjs` creates the durable screenshot/player-read
gate for visual, FTUE, and audience-test slices. Use it through
`node tools/ai.mjs gate` before expanding game content when the task depends on
whether the first screen reads as a product. Use `--visual-strict` for
beautiful, casual, generated-UI, fake-shot, or child-testable prototype work;
it requires six-axis visual scores and blocks a pass when text/readability,
UI controls, action direction, art quality, composition, or audience fit are
below the bar. `tools/product_gate/visual_critique_packet.mjs` creates a
reusable critic prompt/packet from a screenshot and target before the final
strict gate; use it through `node tools/ai.mjs critic` in normal agent work.
`tools/product_gate/close_slice.mjs` is exposed as
`node tools/ai.mjs close-slice`; use it before handoff/review so the task log
records the product gate, validation evidence, and next action.
`tools/product_gate/responsive_layout_audit.mjs` checks UI-tree geometry for
responsive layouts: required action nodes exist, touch targets are large
enough, selected buttons do not overlap, and portrait primary actions use a
full-width row above secondary actions.
`tools/product_gate/slice_hygiene.mjs` is the pre-review/pre-commit audit for
prototype slices. It reports diff size, evidence checklist coverage, push
target visibility, changed fail/stale review artifacts, and the 30-file
normal-slice threshold. Use `--strict` with build/probe evidence, the product
gate, and a screenshot before handoff/commit. A profiler guard
(`--profile-guard`, from `node tools/ai.mjs status --require-current-scope-usable`)
is optional/advisory and never blocks the slice (passive profiling does not
block normal work). Use `--snapshot` only when the lead intentionally wants an
end-of-experiment snapshot instead of scoped phase commits.

`tools/assets/new_generation_record.mjs` writes the provenance record for an
accepted generated or artist source sheet: provider/model or workflow,
workflow file/json, seed or no-seed reason, prompt, negative prompt, accepted source image, and
rejected candidate notes. Use it after selecting source art and before strict
validation; procedural/programmer-art records require an explicit exception
and are debug scaffolds, not final generated art. Generated/artist records must
use a real workflow path or non-empty workflow JSON; `{}` is only accepted for
procedural debug records. If the provider does not expose a stable seed, record
`--no-seed-reason` instead of inventing an unknown seed value. Use
`--prompt-packet` when the source came from a contract-derived prompt packet.

`tools/assets/plan_source_sheet_prompt.mjs` compiles a source-sheet prompt
packet from an art job's generation contract. Use it before image generation so
the prompt, negative prompt, chroma key, source family role, no-bake rules, and
acceptance checklist come from durable project data instead of chat memory.
When fed an intake audit, it prefers `next_prompt_key_color`, records
`key_color_source`, and carries `intake_key_color_action` into the prompt
packet for provenance. If the intake action is
`split_preserve_or_dual_plate_alpha`, it refuses to create another chroma
prompt by default; pass `--allow-chroma-after-preserve-risk` only for a
diagnostic override, otherwise switch to split/preserve or dual-plate alpha.
The override is written as `diagnostic_chroma_override: true` so strict
validation can distinguish deliberate diagnostics from accidental chroma loops.
Generation records can then reference the packet with `prompt_packet`; strict
art-job validation parses JSON prompt packets and checks schema, required
fields, job identity, acceptance checklist, source-family alignment, and unsafe
chroma override policy.
`tools/assets/plan_missing_source_family_prompts.mjs` turns a failing source
family coverage audit into a concrete prompt queue. It emits prompt packets for
required source families that do not yet have passing final-accepted generation
records, so the next action is generating missing blank bases, icon sheets, or
decor overlay sheets rather than inventing prompts in chat.

`tools/assets/chroma_key_alpha.py` is the shared chroma/alpha-border cleanup
module used by generated UI builders and source-derivation audits. It keeps
background removal, source-key spill removal, purple-halo decontamination,
transparent RGB bleed, and premultiplied-alpha resizing in one tested place.
Builders and audits should pass the crop manifest's actual key color; a green
source sheet can fail with green edge spill even when the old magenta/purple
checks pass.

`tools/assets/dual_plate_alpha.py` extracts transparent PNGs from two
pixel-aligned generated plates: one on a light background and one on a dark
background. Use it when chroma-key source sheets repeatedly fail fringe audits
or when the asset has delicate antialiasing, hair-like detail, glow, or soft
ornate edges. It writes an RGBA PNG plus optional JSON/Markdown report with
visible-pixel, alpha-bbox, hidden transparent-RGB, cleanup, and timing stats:

```powershell
py -3.12 tools/assets/dual_plate_alpha.py `
  --light path/to/light-plate.png `
  --dark path/to/dark-plate.png `
  --output path/to/output.png `
  --json-output path/to/report.json `
  --report path/to/report.md `
  --blob-min-area 12 `
  --profile
```

This mode requires a stronger generation contract than chroma: both plates must
have the same dimensions and the same subject placement. If the generator
changes the ornament shape, lighting, or pose between plates, reject the pair
before extraction. Treat the JSON report as a gate: accept only `verdict:
pass` with an empty `problems` list and `transparent_nonzero_rgb_pixels: 0`.
Hidden RGB under alpha can leak back as 1-2px fringe during premultiplied
resizing or atlas filtering. Blob cleanup zeroes RGB when it removes tiny alpha
components. Use `--no-fail` only to keep rejected diagnostic candidates.

`tools/assets/validate_art_job.mjs` validates the generated-art job contract:
source families, generation records, reusable kinds, crop/runtime manifests,
slice9 metadata, icon policies, listed edge-proof evidence, slice9 design
policy evidence, runtime composition proof evidence, and generated UI
audit evidence. Use `--strict` after slicing to prove the runtime asset
contract; strict mode requires every crop output to appear in the runtime
manifest with matching `id`, `kind`, and `path`. It also requires
`expected_outputs.asset_audit` and reads JSON reports for `verdict: pass`;
those reports must reference the same crop manifest as the art job and cover
every crop id. If `expected_outputs.edge_proofs` lists zoomed edge proof
images, strict mode also requires matching structured JSON reports in
`expected_outputs.edge_proof_reports`, and accepted reports must show
`counts.total: 0`; reports with bad marks belong in candidate/rejected evidence.
Use `--final-art` only when claiming final generated or artist
source art; it rejects procedural debug scaffolds, partial/unknown generation
provenance, missing generated-source derivation, source-sheet intake audit,
slice9 design policy, runtime composition proof, or source-family coverage
evidence, audit evidence that points at a different crop manifest/source/runtime
manifest, and source-derivation reports that do not cover every source-derived
`slice9`, `border`, `tile`, or `sprite` crop id.
`tools/assets/audit_slice9_design_policy.mjs` is the manifest-level gate for
slice9 art design. It requires each slice9 crop and runtime asset to declare
stretch-zone material policy, fixed-corner/non-stretch ornament policy,
allowed size class, minimum runtime size, content safe area, min-size preview,
stress preview, and disallowed uses. It also validates slice9 margins against
source and preview sizes so every checked size leaves a stretchable center.
Run with `--profile` when comparing policy changes; the report records total
and per-asset timing and prints the slowest slice9 asset. This catches the
failure where a generated panel is technically cuttable but has medallions,
gems, or heavy caps baked into areas that runtime resizing will stretch; those
decorations must be corner-only or separate overlay sprites. When a policy uses
`non_stretch_ornaments: separate_overlay_assets`, each `overlay_asset_id` must
exist in the crop/runtime manifest and reference a non-slice9 overlay asset.
`tools/assets/audit_atlas_metadata.mjs` checks runtime asset manifests before
packing/reuse. Every asset must declare `pack_group`, `source_crop`,
`original_size`, `trim_rect`, and `atlas_policy` fields for trim mode, alpha
bleed, premultiplied-alpha-safe processing, extrusion, shape/border padding,
scale variant, and rotation policy. Slice9 entries must set
`allow_rotation: false` and `trim_preserves_slice9: true`; aliases must point
to existing asset ids. This is the gate for atlas space savings without
reintroducing 1-2 pixel halos or broken slice margins.
`tools/assets/build_ui_atlas_pack.py` builds review/proof atlas PNGs from a
runtime asset manifest. It groups assets by `pack_group`, preserves
slice9/content metadata in a `game.ui_atlas_pack` manifest, writes extruded
padded rects around each sprite, can draw id labels with `--label-review` in
reserved `review_label.rect` free space outside the asset `padded_rect`, and
reuses `alias_of` entries without duplicating the physical bitmap. Physical
source labels list linked aliases so the labeled preview can be used to choose
asset ids without guessing; long labels keep exact `review_label.text` metadata
but render wrapped `review_label.lines` so a single verbose id does not widen
the atlas. Labels should use readable review text, recorded as
`review_label.font_size`, because the preview is meant for visual selection by
the lead, not just machine validation. This output is human validation evidence, not the game's final runtime atlas packer. Record the JSON manifest in
`expected_outputs.atlas_pack`; final-art validation requires it for generated UI.
`tools/assets/audit_ui_atlas_pack.py` validates the built review atlas. It
checks coverage against the runtime asset manifest, atlas image bounds, non-
overlapping physical padded rects, alias rect reuse, metadata consistency, and
that extrusion pixels match the source edge pixels. For labeled review atlases,
it also requires exact `review_label` text, visible label pixels in the labeled
preview, wrapped `review_label.lines` that fit inside `review_label.rect`, no
label pixels in the clean atlas, and label rects that stay outside asset
`padded_rect`s, outside other labels, and inside the atlas. Record passing JSON in
`expected_outputs.atlas_pack_audit`; final-art validation requires it.
`tools/assets/plan_runtime_crops_from_intake.py` converts a passing source
sheet intake audit into a named crop plan for icon, decor, or sprite sheets.
Pass an ids file in visual row-major order with `--ids-file` so large source
sheets do not require fragile long shell commands. The JSON/Markdown outputs
record component ids, detected bboxes, padded crop rects, output paths, trim
policy, chroma-key policy, and pack metadata skeletons. Use the plan as the
reviewable bridge between detector output and the final runtime crop manifest.
`tools/assets/build_runtime_assets_from_crop_plan.py` builds runtime PNGs,
crop manifests, asset manifests, and contact sheets from those crop plans. It
uses the shared chroma/alpha cleanup path, trims to alpha bounds with padding,
scrubs fully transparent RGB to zero, maps decor plans to `decor_overlay`
runtime metadata, and writes atlas fields so pixel audits and review-atlas
packing can run immediately.
`tools/assets/audit_runtime_ui_asset_usage.mjs` is the runtime placement gate.
It compares a `game.runtime_ui_asset_usage` file against an asset manifest's
`usage_policy`, then fails generated UI assets drawn below `min_size`, in the
wrong layout mode, or with a disallowed usage tag such as compact dense button
rows. Run it after integrating generated slice9 assets into native UI; it
catches the gap where asset manifests pass but runtime squeezes large-only art
into a too-small control.
`tools/assets/audit_source_family_coverage.mjs` checks that final generated UI
art is backed by separate accepted source families instead of one mixed
fake-shot/source sheet. Jobs list required families in
`expected_outputs.required_source_families`, usually blank UI kit sheet,
isolated icon sheet, and UI decor overlay sheet. The audit fails missing
families and records whose role/notes say mixed, candidate, temporary, partial,
or debug. Final-art validation requires a passing JSON report in
`expected_outputs.source_family_coverage_audit`.
`tools/assets/normalize_source_sheet_chroma.py`
normalizes only border-connected key-like background pixels to the exact chroma
key before slicing; keep the raw generated sheet alongside the cleaned copy.
`tools/assets/audit_source_sheet_intake.py` is the pre-slicing gross
source-sheet gate: it checks flat/chroma background, component count, border
clearance, gutters, exact key-color holes inside components, and broad
key/halo hue conflicts before crop rectangles are trusted. It also scores
candidate key colors against the component palette and reports
`key_color_action` plus `next_prompt_key_color` for the next source-sheet
generation pass. Use `next_prompt_key_color` when the action is
`regenerate_with_next_prompt_key_color`; when the action is
`split_preserve_or_dual_plate_alpha`, switch to explicit preserve/split policy
or dual-plate alpha instead of cycling similar chroma colors. Use
`--profile` on slow or disputed sheets to write per-stage timing and the
analysis engine (`numpy` fast path or portable `python` fallback) into
JSON/Markdown and print the slowest stage. Final-art validation requires
passing JSON intake evidence in
`expected_outputs.source_sheet_intake_audit`, and that report's `source` must
match the art job's expected source art or crop source. The audit merges small
satellite fragments near a larger component so multi-part icons can pass
without hiding truly tight full-size neighboring assets.
`tools/assets/audit_generated_ui_assets.py` is the pixel gate after slicing: it
opens runtime PNG outputs and fails clipped icon alpha bounds or visible
chroma-key fringe, including soft purple, very dark purple, and dark
maroon/magenta one-pixel edge halos, including near-black purple edge pixels
such as `#26022d`,
before the assets reach gameplay code. It reads `green_screen.key` from the
crop manifest and fails visible source-key spill, but it also treats saturated
green-screen spill as a default defect even when the manifest omitted the
source key. Intentional saturated green edges require `preserve_green_edges`.
It rejects transparent edge pixels that still store key/purple/green/source-key
RGB, because texture filtering can sample that hidden color back into visible
edges. `preserve_purple_edges` only suppresses intentional purple/magenta
checks; source-key and green-screen leaks must still fail under that policy.
Use `--profile` on slow or disputed runs to write per-asset timing into
JSON/Markdown and print the slowest asset; the default run stays quiet and
verdict-compatible. When NumPy is available, the edge color scans use
vectorized masks with the same Python fallback kept for minimal portable
installs.
`tools/assets/render_ui_asset_edge_proof.py` renders zoomed top/right/bottom/left
alpha-boundary strips on a checkerboard and marks detected bad edge pixels. It
uses the same key/purple/green/source-key edge classes as the generated UI
asset audit, including hidden bad RGB in transparent pixels near visible
edges. Add `--json-output` and `--report` to write per-asset/per-side counts
by reason so cleanup iterations can be compared without eyeballing every
pixel. Use it when 1-2 pixel fringe is reported or when a normal contact sheet
is too small to review edge quality. Use `--asset-id` and `--side` to create a
small proof for the exact reported edge.
`tools/assets/render_ui_composition_proof.py` renders runtime composition proof
from an asset manifest: slice9 base at target preview sizes, optional anchored
decor/state overlays, runtime labels, and content safe-area outlines. Use it
after slice9 assets pass pixel audits; it catches panels/buttons that are
technically cuttable but whose `content` rectangle collapses at compact sizes or
cannot fit real runtime text. The output PNG is human review evidence, and the
JSON/Markdown report should be recorded beside other generated UI reviews.
Run with `--profile` when it feels slow; the report includes timing plus
image/slice9/panel cache-hit stats so optimization work can compare measured
bottlenecks instead of guessing from the rendered PNG.
`tools/assets/audit_generated_source_derivation.py` is the anti-redraw gate for
generated UI crops: it compares each runtime PNG against the accepted source
crop after chroma cleanup and fails builders that output procedural redraws
instead of source-derived art.

### `reusable_game_infrastructure`

Reusable only when the next game keeps a compatible native/runtime stack:

- `tools/state_codegen/`
- generic `tools/devapi/` clients, capture helpers, and probes

Copy or adapt these deliberately after the engine/runtime policy is known.

### `project_specific_67_world`

Specific to the current 67 World game, its release package, balance, art packs,
or child-test evidence. These should be deleted, archived, or intentionally
adapted when starting a different game:

- `tools/project_67_world/`
- `tools/project_67_world/assets/build_67_world_*`
- `tools/project_67_world/assets/validate_67_world_pack_inputs.py`
- `tools/project_67_world/balance/simulate_67_world.py`
- `tools/project_67_world/devapi_scenarios/*.py`
- `tools/project_67_world/package_native_release.mjs`
- `tools/project_67_world/release_candidate_audit.py`

### Generated Cache

`__pycache__/` and `*.pyc` files are ignored scratch artifacts. They are safe to
delete and should never be copied into a new project.

## New Game Cleanup Rule

For a clean new game:

1. Keep `portable_ai_pipeline`.
2. Decide whether `reusable_game_infrastructure` matches the selected runtime.
3. Remove or archive `project_specific_67_world`.
4. Delete generated caches.
5. Run `node tools/pipeline_validate.mjs` after normal cleanup, or
   `node tools/pipeline_validate.mjs --full` after export/runtime template
   changes.

`--quick` is the default and is the right mode after narrow pipeline/tooling
edits. `--full` is heavy (it exports the repo into `tmp/` and re-runs every
suite inside the export) and is reserved for portable-base/export/runtime/
release gates. Each `--full` run leaves a `tmp/pipeline-validate-<stamp>/` copy;
every run now prunes those to the newest 3 by default (`--keep-exports <n>` to
change, `--no-prune` to disable).
