# Generated UI Workflow And Gate Reference

Load this reference when doing real generated runtime UI work. The skill body is
only the routing layer.

## Work Unit

Use one art job as the unit:

```text
accepted target -> art bible -> art job -> source family prompt
-> selected source sheet -> crop plan -> runtime assets
-> composition proof -> source-derivation audit -> runtime integration
-> screenshot/product gate
```

Before visual-first UI work, write the 5-line session contract: goal, non-goal,
proof, stop condition, likely files. The proof must name source/runtime
manifests, composition proof, screenshot/product gate, and runtime integration
evidence.

## Source Families

Generate source families, not one composed gameplay screenshot:

- `blank UI kit sheet`
- `isolated icon sheet`
- `ui decor overlay sheet`
- state overlay sheet
- bar/progress system sheet
- map/world layer sheet
- sprite/FX sheet when needed

Use full mockups only as visual targets. Do not claim final generated UI from a
mixed source sheet. Reject baked text, fake letters, fused icons/buttons, tight
gutters, weak silhouettes, watermarks, unsafe chroma backgrounds, and sheets
where component isolation requires overlapping crops.

## Art Job And Provenance

Create/update an art job before generation:

```powershell
node tools/assets/job/new_art_job.mjs --id <job-id> --family <family> --project-dir <project-dir>
```

The job should track source art, crop/runtime manifests, expected outputs,
runtime composition, `target_preview_sizes`, and final evidence.

After selecting source art, create a generation record:

```powershell
node tools/assets/job/new_generation_record.mjs --id <source-id> --project-dir <project-dir> --source-family "<family>" --source-family-role "<role>" --accepted-source <path> --provider <provider> --model <model-or-workflow> --workflow-path <workflow.json> --prompt-packet <prompt.json> --seed <seed>
```

If the provider has no stable seed, use `--no-seed-reason`; do not invent
unknown seed data. Generated/artist records need real workflow provenance or a
non-empty workflow JSON. Procedural scaffolds must be recorded as debug
exceptions and cannot pass `--final-art`.

Plan prompt packets from the art job, not ad hoc chat text:

```powershell
node tools/assets/job/plan_source_sheet_prompt.mjs --job <job> --source-family "<family>" --output <prompt.md> --json-output <prompt.json>
```

Prompt packets must include source family, source family role, prompt, negative
prompt, acceptance checklist, machine-readable `source_sheet_layout`, key color
source, and intake recommendation when relevant.

## Intake And Crop

Normalize non-flat chroma backgrounds first:

```powershell
py -3.12 tools/assets/intake/normalize_source_sheet_chroma.py --source <raw-sheet> --output <clean-sheet>
```

Audit source-sheet intake:

```powershell
py -3.12 tools/assets/intake/audit_source_sheet_intake.py --source <source-sheet> --json-output <audit.json> --report <audit.md>
```

This catches non-flat chroma, unsafe key-color holes, hue conflicts, merged
components, clipped components, and gutter failures. If the source contains
border-connected key color, shadow/key contamination, or semitransparent fringe,
fix the source, split layers, use true alpha, or switch to dual-plate alpha
rather than widening tolerance until art is damaged.

For multi-component icon/decor/sprite sheets, create a crop plan from intake:

```powershell
py -3.12 tools/assets/crop/plan_runtime_crops_from_intake.py --intake-audit <audit.json> --ids-file <ids.txt> --kind <icon|decor|sprite> --source-id <source-id> --source-role <role> --output-dir <runtime-dir> --json-output <crop-plan.json> --report <crop-plan.md>
```

Runtime crop entries need named ids, rects, outputs, kind, semantic role,
trim padding, and component isolation policy. Slice9 entries also need slice9
margins, content safe areas, target preview sizes, stretch policy, and usage
policy. Do not integrate from an empty crop manifest or empty runtime manifest.

## Runtime Assets

Build runtime PNGs from the crop plan:

```powershell
py -3.12 tools/assets/assemble/build_runtime_assets_from_crop_plan.py --crop-plan <crop-plan.json> --crop-manifest <crop-manifest.json> --asset-manifest <asset-manifest.json> --art-job <job.json> --contact-sheet <contact.png>
```

The builder must cut from accepted source art, not redraw panels. Keep
`tools/assets/chroma_key_alpha.py` as the shared cleanup path for chroma/alpha
logic. It should remove border-connected key color, trim to alpha bounds with
padding, scrub transparent RGB, and preserve source-derived pixels.

If a crop is soft/glow/glass, route to dual-plate alpha instead of forcing
`key_matte`; use `--strict-route` to turn route warnings into hard failures
when needed. Runtime builders must not redraw panels with procedural shapes and
present them as generated outputs.

## Gate Tiers

Run only the tier the iteration needs.

DRAFT, every normal iteration:

- source-sheet intake, with normalize step only when needed;
- runtime PNG/contact sheet build;
- optional draft contract: `node tools/assets/job/validate_art_job.mjs --job <job>`.

INTEGRATE, when wiring assets into runtime:

- `node tools/assets/job/validate_art_job.mjs --job <job> --strict`
- `py -3.12 tools/assets/audit/render_ui_composition_proof.py --asset-manifest <runtime-manifest> --output <proof.png> --json-output <proof.json> --report <proof.md>`
- `py -3.12 tools/assets/audit/audit_generated_source_derivation.py --crop-manifest <crop-manifest> --json-output <audit.json> --report <audit.md>`
- runtime screenshot/product gate with `node tools/product_gate/review.mjs` or
  `node tools/ai.mjs gate`.

FINAL-ART, only when shipping a reusable kit or claiming completion:

- source-sheet intake evidence recorded in the art job;
- strict contract;
- slice9 design-policy audit;
- composition proof;
- atlas metadata audit;
- labeled review atlas build and audit;
- source family coverage audit;
- generated-source derivation audit proving source-derived PNGs;
- runtime usage audit;
- final generated/artist art gate:
  `node tools/assets/job/validate_art_job.mjs --job <job> --final-art`;
- native/runtime screenshots and product gates;
- `node tools/product_gate/responsive_layout_audit.mjs` when `ui.tree` bounds are available.

Use the tier planner to print exact command order without running image tools:

```powershell
node tools/assets/job/run_ui_asset_tier.mjs --tier draft|integrate|final --plan --job <job> --crop-manifest <crop-manifest> --runtime-manifest <runtime-manifest> --source-sheet <source-sheet>
```

## Responsive Runtime Proof

Desktop and portrait are separate compositions using the same reusable assets;
do not squeeze desktop HUDs into phone layout. Portrait should have fewer
simultaneous values, one full-width primary action, secondary actions below, and
short objective/journal text.

A screenshot pass is not enough when clickable geometry is wrong. Use
`responsive_layout_audit.mjs` for action bounds when runtime `ui.tree` exists.

## Failure Response

If the lead reports cropped icons, key-color outlines, halo, ugly UI, unclear
first action, or mobile density:

1. Stop feature/content expansion.
2. Reopen source sheet, crop manifest, contact sheet, composition proof,
   source-derivation audit, latest screenshots, and product gate.
3. Fix the earliest failed stage.
4. Do not compensate in runtime code for a bad source sheet or missing manifest
   rule.
5. If the fix swaps in procedural/programmer panels, mark it as a temporary
   debug scaffold and reopen source generation.
6. Add a validator, audit, or skill rule for repeated failures before moving on.

## Report Shape

Report source art, art bible, crop/runtime manifests, preview sheets,
composition proof, source-derivation audit, responsive layout audit,
screenshots, product gates, validations run, and the next visual gap.
