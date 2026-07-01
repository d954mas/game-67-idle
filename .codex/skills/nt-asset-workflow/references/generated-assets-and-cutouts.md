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

Generated or artist source art needs durable provenance, but AI Studio no
longer owns a shared generated-art scaffold. Keep game-specific contracts,
prompts, source decisions, accepted outputs, and rejected-candidate notes with
the game that asked for them, usually under `games/<game-id>/design/` or the
game's asset metadata.

Record enough to re-run or judge the source: provider or generator,
model/workflow when known, prompt, negative prompt, accepted source image,
source-first decision, seed when exposed, or an explicit no-seed reason. Do not
invent pseudo-seeds.

## Cutout Rules

- Chroma cleanup must be border-connected so intentional interior colors are not
  deleted.
- Remove key fringe and hidden RGB under transparent edges before storage or
  game use.
- Tight gutters, baked key spill, unsafe shadows, or missing pivots are source
  problems, not prepared asset details.
- Slice9 assets need content safe area, minimum target size, and game
  composition proof at the smallest supported layout.
- Reusable UI-kit work stays split by owner: the game owns source-family
  contracts and prompt/provenance notes; `ai_studio/assets/tools/` owns
  source-sheet audits, crop plans, cutouts, and review atlases;
  `ai_studio/quality/` owns visual acceptance rules.
