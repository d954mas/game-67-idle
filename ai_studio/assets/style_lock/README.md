# Style lock

Style lock is the game-owned operational contract shared by asset generation,
Canvas review, technical art checks, and later promotion gates. A game stores
its accepted copy at `games/<game-id>/design/style_lock.json` and validates it
with:

```powershell
node ai_studio/assets/style_lock/validate.mjs games/<game-id>/design/style_lock.json
```

Start from `style_lock.example.json`. `style_lock.schema.json` is the portable
structural JSON Schema. Cross-field ownership rules (matching private game id,
one Canvas project, unique exemplar ids) are intentionally enforced by the
dependency-free fail-closed `validate.mjs`, because JSON Schema cannot express
those equalities. The CLI additionally binds the lock to the current Studio
workspace at `games/<id>/design/style_lock.json` or
`games/private/<id>/design/style_lock.json`, resolves links physically, and
requires matching public/private Canvas refs plus a regular `art_contract_ref`
file inside that game's physical design directory.

## Division of responsibility

`design/art/art_contract.json` remains the broad taste and screenshot-review
brief: audience, fantasy, materials, camera, UI hierarchy, and yes/no
references. `design/style_lock.json` is its compact operational twin. It links
back through `art_contract_ref` and owns only repeatable prompt inputs, accepted
owned exemplars, background preparation, asset dimensions, and calibrated
technical-gate thresholds. Do not copy the whole art contract into the lock.

`draft` is exploration state. `accepted` means the lead has chosen one shared
direction for world/sprites and GUI. Recipe, pack, animation, and AI dual-plate
generation on a game-owned Canvas project is production mode: it requires the
accepted game lock before any paid or slow work and freezes its `id` into
`element.meta.origin`. `--no-lock` explicitly switches that run to explore mode
and records a visible `no-lock` taint; unowned Canvas projects remain untainted
exploration workspaces.

## Canvas convention

Use one game-owned Canvas project and create a top-level plain group named
`style`. Its public ref becomes `canvas_ref` (private game refs remain
store-qualified). Inside it:

- create one child style card named `passport` through `style-create`; its
  `style.prompt` mirrors `prompt_preamble`;
- place 2-3 owned exemplar images in the passport card, covering both
  `world` and `gui`; one may be the card's generation `style.ref`, while every
  accepted exemplar is listed explicitly in `exemplar_refs`;
- add a plain `palette` group with labelled swatches;
- add a plain `references` group for sourced moodboard material, kept separate
  from owned exemplars;
- add a plain `do-dont` group with positive and negative notes/examples.

Create and mutate these groups through Canvas CLI/ops, never by editing
`project.json`. The machine lock points to stable Canvas ids, not group names.
Palette feeds prompts and review context; it is not a palette-distance gate.

## v1 invariants

- One lock id and game id, both lowercase slugs.
- A confined `design/.../art_contract.json` link.
- A non-empty prompt preamble and negative prompt.
- 2-12 unique colors and an explicit transparent or magenta/green chroma rule.
- 2-3 owned Canvas exemplars with both world and GUI represented.
- Fixed generation size and complete deterministic gate thresholds. Chroma
  locks require spill/halo ratios; native-transparent locks use `null` for
  those two color-key metrics.
- `model_checkpoint` stays `null`; per-game LoRA/checkpoints remain parked for
  long projects and require a future schema version.

T0317 owns the measurement formulas and calibration evidence for deterministic
background, alpha, crop, and aspect checks. The formulas and exact mapping from
`technical_gate` plus `asset_size` are defined in the
[post-cutout quality gate](../tools/image/quality_gate/README.md). Accept a
game's values only after its real broken/clean corpus separates; the committed
example is synthetic contract evidence, not universal tuning.

Vision/style acceptance remains advisory against exemplars plus Do/Don't, with
the lead as backstop. No CLIP, embedding, or palette-delta gate is part of v1.
