---
type: Game Knowledge Index
title: RB Dark RPG Knowledge Index
description: Index of durable game-specific knowledge for RB Dark RPG.
tags: [game-knowledge, index]
game_id: rb-dark-rpg
status: draft
---

# RB Dark RPG Knowledge Index

Use this as the first stop for durable game-specific knowledge.

## Accepted Design Facts

- `rb-dark-rpg` is the active game id and `RB Dark RPG` is the working title.
- The first slice targets dark fantasy RPG exploration/combat, not idle or
  offline progression.
- A user-provided v0 concept/GDD draft is captured in
  [sources/v0_concept_gdd_draft.md](sources/v0_concept_gdd_draft.md).
- The distilled v0 concept direction is tracked in
  [v0_concept_idea.md](v0_concept_idea.md): `Дракон не вернулся`, a
  2D illustrated FITGAME-like dark browser RPG about ordinary searchers in a
  world after the Great Dragon disappears.

## Reference Lessons

- Story/world/lore reference lessons are captured in
  [story_world_lore_digest.md](story_world_lore_digest.md), backed by
  [sources/story_world_reference_packet_2026-07-04.md](sources/story_world_reference_packet_2026-07-04.md).
- First visual direction quick pass for the `Последний Пост` hub background is
  captured in
  [visual_refpack_last_post_2026-07-04.md](visual_refpack_last_post_2026-07-04.md).
- First-autobattle stat reference notes are captured in
  [sources/combat_stats_reference_packet_2026-07-04.md](sources/combat_stats_reference_packet_2026-07-04.md);
  the full combat mechanics spec lives in
  [../combat_mechanics.md](../combat_mechanics.md), and the current structured
  combat contract lives in
  [../data/combat.json](../data/combat.json).
- Dialogue modal reference notes are captured in
  [sources/legacy_of_dragons_dialogue_modal_ref_2026-07-04.md](sources/legacy_of_dragons_dialogue_modal_ref_2026-07-04.md):
  first-slice dialogue should be a centered modal over a dimmed scene, not a
  small side panel.
- Quest structure and state decisions are captured in
  [../quest_system.md](../quest_system.md), with authored quest data in
  [../data/quests.json](../data/quests.json).
- The data-driven content model is captured in
  [../content_model.md](../content_model.md), the local web editor direction is
  captured in [../content_editor_spec.md](../content_editor_spec.md), and the
  loader/editor manifest lives in [../data/content_manifest.json](../data/content_manifest.json).
- Save-compatible content update rules are captured in
  [content_update_compatibility_rules.md](content_update_compatibility_rules.md):
  after saves exist, quest/item/reward/flag/unlock ids become compatibility
  contracts and should be deprecated or migrated instead of deleted.

## Playtest Or Build Findings

- Starter project created from `templates/template` on 2026-07-04.

## Open Questions

- Decide whether `rb-dark-rpg` remains only the repo id while the public title
  becomes `Дракон не вернулся`.
- Choose the first implementation slice: hub screen, map screen, autobattle,
  quest journal, or clue journal.
- Define the first screen layout for `Последний Пост`.
- Decide the Dragon's true state after Act I: bound, hidden, wounded, used as a
  seal, or another option.
- Decide Black Sun's exact ideology beyond "prepared for a world without the
  Dragon".
