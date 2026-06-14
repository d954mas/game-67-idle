# Tools Layout

`tools/` intentionally contains several layers. Do not treat every file here as
equally portable.

The machine-readable map is `tools/tool_layers.json`.

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
- `tools/assets/audit_runtime_ui_asset_usage.mjs`
- `tools/assets/audit_source_family_coverage.mjs`
- `tools/assets/chroma_key_alpha.py`
- `tools/assets/dual_plate_alpha.py`
- `tools/assets/normalize_source_sheet_chroma.py`
- `tools/assets/audit_source_sheet_intake.py`
- `tools/assets/audit_generated_ui_assets.py`
- `tools/assets/render_ui_asset_edge_proof.py`
- `tools/assets/render_ui_composition_proof.py`
- `tools/assets/audit_generated_source_derivation.py`

`tools/product_gate/review.mjs` creates the durable screenshot/player-read
gate for visual, FTUE, and audience-test slices. Use it through
`node tools/ai.mjs gate` before expanding game content when the task depends on
whether the first screen reads as a product. `tools/product_gate/close_slice.mjs`
is exposed as `node tools/ai.mjs close-slice`; use it before handoff/review so
the task log records the product gate, validation evidence, and next action.
`tools/product_gate/responsive_layout_audit.mjs` checks UI-tree geometry for
responsive layouts: required action nodes exist, touch targets are large
enough, selected buttons do not overlap, and portrait primary actions use a
full-width row above secondary actions.

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
Generation records can then reference the packet with `prompt_packet`; strict
art-job validation parses JSON prompt packets and checks schema, required
fields, job identity, acceptance checklist, and source-family alignment.
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
visible-pixel and cleanup stats:

```powershell
py -3.12 tools/assets/dual_plate_alpha.py `
  --light path/to/light-plate.png `
  --dark path/to/dark-plate.png `
  --output path/to/output.png `
  --json-output path/to/report.json `
  --report path/to/report.md `
  --blob-min-area 12
```

This mode requires a stronger generation contract than chroma: both plates must
have the same dimensions and the same subject placement. If the generator
changes the ornament shape, lighting, or pose between plates, reject the pair
before extraction.

`tools/assets/validate_art_job.mjs` validates the generated-art job contract:
source families, generation records, reusable kinds, crop/runtime manifests,
slice9 metadata, icon policies, listed edge-proof evidence, slice9 design
policy evidence, runtime composition proof evidence, and generated UI
audit evidence. Use `--strict` after slicing to prove the runtime asset
contract; strict mode requires every crop output to appear in the runtime
manifest with matching `id`, `kind`, and `path`. It also requires
`expected_outputs.asset_audit` and reads JSON reports for `verdict: pass`;
those reports must reference the same crop manifest as the art job and cover
every crop id. Use `--final-art` only when claiming final generated or artist
source art; it rejects procedural debug scaffolds, partial/unknown generation
provenance, missing generated-source derivation, source-sheet intake audit,
slice9 design policy, runtime composition proof, or source-family coverage
evidence, audit evidence that points at a different crop manifest/source/runtime
manifest, and source-derivation reports that do not cover every source-derived
`slice9`, `border`, `tile`, or `sprite` crop id.
`tools/assets/audit_slice9_design_policy.mjs` is the manifest-level gate for
slice9 art design. It requires each slice9 crop and runtime asset to declare
stretch-zone material policy, fixed-corner/non-stretch ornament policy,
allowed size class, minimum runtime size, and disallowed uses. This catches the
failure where a generated panel is technically cuttable but has medallions,
gems, or heavy caps baked into areas that runtime resizing will stretch; those
decorations must be corner-only or separate overlay sprites.
`tools/assets/audit_atlas_metadata.mjs` checks runtime asset manifests before
packing/reuse. Every asset must declare `pack_group`, `source_crop`,
`original_size`, `trim_rect`, and `atlas_policy` fields for trim mode, alpha
bleed, premultiplied-alpha-safe processing, extrusion, shape/border padding,
scale variant, and rotation policy. Slice9 entries must set
`allow_rotation: false` and `trim_preserves_slice9: true`; aliases must point
to existing asset ids. This is the gate for atlas space savings without
reintroducing 1-2 pixel halos or broken slice margins.
`tools/assets/build_ui_atlas_pack.py` builds atlas PNGs from a runtime asset
manifest. It groups assets by `pack_group`, preserves slice9/content metadata
in a `game.ui_atlas_pack` manifest, and writes extruded padded rects around
each sprite so filtering cannot sample transparent or neighboring pixels.
Record the JSON manifest in `expected_outputs.atlas_pack`; final-art validation
requires it for generated UI.
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
candidate key colors against the component palette and reports a
`suggested_key_color` for the next source-sheet generation pass. Final-art
validation requires passing JSON intake evidence in
`expected_outputs.source_sheet_intake_audit`, and that report's `source` must
match the art job's expected source art or crop source. The audit merges small
satellite fragments near a larger component so multi-part icons can pass
without hiding truly tight full-size neighboring assets.
`tools/assets/audit_generated_ui_assets.py` is the pixel gate after slicing: it
opens runtime PNG outputs and fails clipped icon alpha bounds or visible
chroma-key fringe, including soft purple, very dark purple, and dark
maroon/magenta one-pixel edge halos, including near-black purple edge pixels
such as `#26022d`,
before the assets reach gameplay code. It also reads `green_screen.key` from
the crop manifest and fails visible source-key spill such as green-screen
contamination on the alpha contour. It rejects transparent edge pixels that
still store key/purple/source-key RGB, because texture filtering can sample
that hidden color back into visible edges.
`tools/assets/render_ui_asset_edge_proof.py` renders zoomed top/right/bottom/left
alpha-boundary strips on a checkerboard and marks detected bad edge pixels. Use
it when 1-2 pixel fringe is reported or when a normal contact sheet is too small
to review edge quality. Use `--asset-id` and `--side` to create a small proof
for the exact reported edge.
`tools/assets/render_ui_composition_proof.py` renders runtime composition proof
from an asset manifest: slice9 base at target preview sizes, optional anchored
decor/state overlays, runtime labels, and content safe-area outlines. Use it
after slice9 assets pass pixel audits; it catches panels/buttons that are
technically cuttable but whose `content` rectangle collapses at compact sizes or
cannot fit real runtime text. The output PNG is human review evidence, and the
JSON/Markdown report should be recorded beside other generated UI reviews.
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
5. Run `node tools/pipeline_validate.mjs` after cleanup/export.
