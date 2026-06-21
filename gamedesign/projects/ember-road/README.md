# Ember Road

Project wiki for the active prototype `ember-road`.

## Concept

Beautiful fantasy hero RPG. The player travels across an overworld map and
towns, takes quests, farms enemies/resources, collects items, upgrades gear,
levels up, and resolves combat through automated battles. Player agency sits in
where to go, which quest to pursue, what to equip, when to return to town, and
which grind target advances the hero next.

Closest named taste reference from the lead: `Legend: Legacy of the Dragons`
for fantasy RPG mood, dense quest/map progression, and ornate fantasy UI. This
is a taste anchor only until `references/legend_legacy_dragons_digest.md` is
completed with observed sources. Do not copy names, characters, screenshots,
layout shapes, monetization, or protected assets.

## Stage 0 Startup Gate

- Native-first implementation only until an explicit web/mobile exception is approved.
- First playable slice must name a fake shot, product-read gate, and native screenshot proof before broad runtime work.
- Visual-first session contract is required before runtime visual work: goal,
  non-goal, proof, stop condition, likely files.
- Before visual/runtime coding, compare current native screenshot or capture
  plan against the accepted fake shot/target and write a mismatch list.
- Beautiful/casual/generated-UI/fake-shot slices use the strict visual product
  gate rubric: six visual scores and blocker/major issue reporting.
- Keep reusable process learnings in `gamedesign/knowledge/`; keep project-specific facts here.

## First Slice

- Define the smallest playable loop in `gdd.md`.
- First playable loop: town square -> accept one quest -> travel to one forest
  node -> automated fight -> loot/XP -> return to town and claim reward.
- Fill `reviews/first_slice_visual_gate.md` before broad runtime work.
- Fill `visual/live_state_acceptance_matrix.md` before any broad UI/visual pass.
- For visually important slices, create the critic packet named in that gate
  before writing the strict product gate verdict.
- Capture visual/product proof in `reviews/` before expanding content.
- Product-read gates must use `visual/live_state_acceptance_matrix.json`
  with explicit covered or not-covered states.
- Update screenshot-vs-target mismatches after meaningful render changes.
