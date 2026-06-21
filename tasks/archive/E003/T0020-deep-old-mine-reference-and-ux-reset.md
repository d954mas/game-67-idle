---
id: T0020
title: Deep Old Mine reference and UX reset
status: dropped
epic: E003
priority: P1
tags: [ember-road, references, visual-direction, ux, old-mine, lead-rejection, y-up]
created: 2026-06-20
updated: 2026-06-21
---

## What

Stop feature expansion after the lead rejected the current visual/UX direction
again. Refresh the Old Mine reference packet with closer fantasy browser RPG
and quest-location references, audit the current Depth 1 screenshot against
that packet, and define the next runtime proof before any repeatable dungeon,
economy, or gear expansion.

### Iteration Goal

Make the next Old Mine pass reference-driven instead of decorator-driven. The
player-facing target must answer: where am I, what is the main world action,
what changed after input, what reward/progress matters, and why this is closer
to the desired fantasy RPG.

### Scope

- Strengthen the Old Mine reference digest with more similar references and
  role-specific use/avoid notes.
- Compare the current native capture
  `build/captures/ember-road/state_old_mine_depth_encounter.png` against the
  reference grammar.
- Record a smaller honest next proof: a revised Old Mine runtime/fake-shot
  target where the mine scene, route/depth, threat, and reward are integrated.
- Re-state the Y-up invariant as a hard rule for every future game/world/UI
  layout note.

### Out Of Scope

- No new mine floors, repeatable delve loop, procedural dungeon, inventory,
  crafting, shop, economy, or combat variety.
- No final sliced UI kit or direct copied layouts/assets from references.
- No Y-down layout semantics. Y is up everywhere in game/world/UI logic;
  renderer, input, screenshot, and DevAPI may convert only at named boundary
  adapters.

### Proof

- Updated reference digest:
  `gamedesign/projects/ember-road/references/old_mine_entry_reference_refresh.md`.
- Updated rejection digest:
  `gamedesign/projects/ember-road/references/visual_ux_rejection_reference_digest.md`.
- Taskboard validation.

## Done when

- [x] Similar reference set is expanded with source roles, not just names.
- [x] Current T0019 screenshot mismatch is written in concrete visual/UX terms.
- [x] Next Old Mine proof is named before more runtime feature work.
- [x] Y-up hard rule is repeated in the task and reference digest.
- [x] `node tools/taskboard/cli.mjs validate` passes.

## Open questions

- Does the lead want the next proof to be a revised fake shot first, or a
  narrow runtime rewrite of the Old Mine screen using the current assets?

## Log

- 2026-06-21: Lead closed the Ember Road prototype. This task is dropped and
  archived as historical evidence only; do not continue it unless explicitly
  reopened.
- 2026-06-21: Started after lead said the current visual/UX still does not
  match the desired game and re-stated that Y must always be up.
- 2026-06-21: Updated Old Mine reference refresh and visual rejection digest
  with the T0019 depth-result mismatch, stronger adjacent ref roles, and next
  proof candidate
  `build/captures/ember-road/state_old_mine_next_delve_choice.png`. Validation:
  `node tools/taskboard/cli.mjs validate` passed.
- 2026-06-20: Reference/UX reset complete: updated Old Mine refresh and rejection digest with T0019 mismatch, stronger adjacent ref roles, Y-up hard rule, and next proof candidate state_old_mine_next_delve_choice.png; taskboard validate passed.
