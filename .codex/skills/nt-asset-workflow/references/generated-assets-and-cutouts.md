# Generated Assets And Cutouts

Load this when generated art, source sheets, chroma/alpha cleanup, crop plans,
slice9, or final-art claims are involved.

## Boundary

- Keep raw source art separate from prepared/generated outputs.
- Keep crop rectangles, pivots, trim rules, slice9 margins, and source family
  ids in manifests, not chat notes.
- Prepared outputs must be reproducible from source assets plus preparation
  commands.
- Procedural/debug art is allowed only as an explicit temporary shortcut. It is
  not a final asset claim.

## Provenance

For generated multi-asset work, keep an art job and generation record:

```powershell
node ai_studio/assets/workflow/art_jobs/new_art_job.mjs ...
node ai_studio/assets/workflow/art_jobs/new_generation_record.mjs ...
```

Generated or artist source art needs a real workflow path, non-empty workflow
JSON, or explicit no-seed reason. Do not invent pseudo-seeds.

## Cutout Rules

- Chroma cleanup must be border-connected so intentional interior colors are not
  deleted.
- Remove key fringe and hidden RGB under transparent edges before storage or
  game use.
- Tight gutters, baked key spill, unsafe shadows, or missing pivots are source
  problems, not prepared asset details.
- Slice9 assets need content safe area, minimum target size, and game
  composition proof at the smallest supported layout.
- Reusable UI-kit work stays in this workflow: art jobs own provenance and
  source-family contracts; `ai_studio/assets/prep/` owns source-sheet audits,
  crop plans, cutouts, and review atlases; `ai_studio/quality/` owns visual
  acceptance rules.
