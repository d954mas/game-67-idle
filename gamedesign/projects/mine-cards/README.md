# Mine Cards

Project wiki for reviving the old `Mine Cards` archive as a new blocky idle RPG.

## Current Direction

Status: `direction pivoted`.

The old card-crawler concept is archived. The active direction is:

```text
Melvor-like block mining idle RPG, starting with one Mining activity.
```

## Definition Of Done For This Design Pass

- Preserve the old GDD/art import as historical source material.
- Record the Melvor-like pivot and the removal of cards from v0.01.
- Capture a source-backed early Melvor history/deconstruction.
- Define a tiny first slice: Mining-only, pickaxe upgrade, rare mining event.
- Keep implementation status honest: ready for fake-shot iteration, not runtime
  implementation.

Out of scope for this pass: runtime code, copying PSDs into the repo, final
asset packing, web prototype work, full economy balancing, and claiming the
game is implementation-ready.

## Source Order

1. `sources/old_gdd_import_2026-06-17.md`
2. `references/melvor_idle_reference_intake_2026-06-17.md`
3. `references/melvor_history_deconstruction_2026-06-17.md`
4. `decisions/2026-06-17-remove-cards-melvor-like-idle.md`
5. `concept.md`
6. `gdd.md`
7. `core_loop.md`
8. `parameters.md`
9. `systems_foundation.md`
10. `data/parameters.json`
11. `data/systems_registry.json`
12. `data/core_loop.json`
13. `data/balance.json`
14. `data/ui_flow.json`
15. `visual/fake_shot_brief_melvor_blocky.md`
16. `visual/3d_character_direction.md`
17. `visual/animation_runtime_options.md`
18. `visual/art_inventory.md`
19. `visual/runtime_asset_plan_v001.md`
20. `visual/skeletal_spike/skeletal_model_animation_research_2026-06-17.md`
21. `data/asset_candidates.json`
22. `reviews/base_gdd_review_2026-06-17.md`
23. `game_implementation_plan.md`

## Current Gate

Status: `base GDD reviewed, task plan ready, not implementation-ready`.

Implementation remains blocked until the T0002 balance draft is accepted or
redirected and the first native screen plan is confirmed. The first 3D miner
source path is now procedural/blockout GLB mesh parts.
