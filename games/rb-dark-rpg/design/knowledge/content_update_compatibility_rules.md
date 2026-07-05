---
type: Game Design Policy
title: RB Dark RPG Content Update Compatibility Rules
description: Save-compatible rules for changing authored quests, items, rewards, flags, and unlocks after player saves exist.
tags: [game-knowledge, content, save-compatibility, migrations]
game_id: rb-dark-rpg
status: draft
---

# Content Update Compatibility Rules

Status: draft

## Why This Exists

Runtime saves store progress and owned objects by stable ids. Authored text and
layout can change freely, but ids that have entered a save become a compatibility
contract.

Current evidence:

- `state/game_state.schema.json` stores quest ids, quest step ids, claimed reward
  ids, choice ids, flag ids, unlock ids, item definition ids, gear instance ids,
  stack instance ids, and equipped gear instance ids.
- `design/data/quests.json` links quest steps to item ids such as `old_sword`,
  `padded_jacket`, `leather_greaves`, and reward/unlock ids such as
  `seeker_token`, `map_gate_unlocked`, and `old_mill_unlocked`.
- `src/game_actions.c` writes quest ids, step ids, reward ids, choice ids, flag
  ids, gear instance ids, and gear `def_id` values into `GameState`.

## Stable Id Contract

After a playtest or release build can write persistent saves, these ids must be
treated as stable:

- quest ids;
- quest step ids;
- dialogue choice ids once recorded in `quests.choice_ids`;
- reward ids once recorded in `quests.claimed_reward_ids`;
- flag ids;
- unlock ids;
- item definition ids stored as stack or gear `def_id`;
- gear and stack instance ids;
- equipment slot instance ids;
- content ids used by requirements, blockers, and effects.

Never reuse an old id for a different meaning. Reusing an id is worse than
deleting it because old saves will silently point to the wrong content.

## Safe And Unsafe Changes

Generally safe after saves exist:

- changing display names, descriptions, dialogue text, journal text, UI hints,
  and icons;
- changing numeric balance values when the global rebalance is intentional;
- hiding content from new players while keeping the old id valid for old saves;
- adding new ids.

Destructive and always migration-required:

- renaming any stable id;
- deleting any stable id;
- changing an existing item from gear to stackable, or from one equipment slot
  to another;
- changing quest flow so an old `current_step_id` no longer exists;
- changing reward ids, because that can duplicate rewards or block claims;
- removing flags or unlocks that gates still read.

A destructive content change is blocked unless it has a migration plan. A
legacy stub is not a free deletion path; it is one possible part of a migration
strategy.

## Quests

Do not delete or rename a quest id after it may be present in
`quests.quest_states` or `quests.tracked_quest_id`.

If a quest must be removed from the current game:

1. Keep a legacy quest stub with the same `id`.
2. Keep any old step ids that may appear as `current_step_id`.
3. Mark the quest as deprecated/archived in authored data.
4. Migrate active saves to one of these outcomes:
   - set the quest status to `content_missing` and clear/replace
     `tracked_quest_id`;
   - convert the quest to a replacement quest via explicit `replacement_id`;
   - complete/fail the quest with a compensation reward if that is the design
     decision.
5. Preserve completed step ids, choice ids, claimed reward ids, and relevant
   flags. They are historical facts in the save.

If a quest step changes design, prefer keeping the old step id and changing the
text/objective. If the new step is truly different, add a new step id and
migrate old `current_step_id` values intentionally.

## Items And Gear

Do not delete or rename an item definition id after it may be present in
inventory stack instances, gear instances, quest requirements, rewards, blockers,
or equipment.

Example: if `old_sword` is too strong, the preferred fix is to keep
`id: old_sword` and lower its stats. The player's gear instance still points to
the same definition, and the item continues to render, equip, unequip, and pass
or fail requirements predictably.

If an item is replaced by a new design:

1. Keep the old item definition as a legacy item.
2. Keep enough data for rendering and safe inventory behavior: kind, slot,
   stackability, icon, display name, and fallback stats.
3. Add a `replacement_item_id` only as explicit metadata; do not silently reuse
   the old id for the new item.
4. Use a migration or domain action if old player-owned instances should become
   the replacement item.
5. If equipped gear can no longer be equipped, unequip it safely or swap it to
   the replacement instance during migration.

For gear, changing the slot or kind of an existing id is migration-required.
Changing only stats is normally a balance patch.

For stackable items, the definition must remain valid while any save can contain
a stack instance with that `def_id`.

## Rewards, Flags, Unlocks

Reward ids recorded in `quests.claimed_reward_ids` are compatibility ids. Do not
rename them during content edits. Changing a reward id can let old players claim
the same reward twice or make a valid claim look unclaimed.

Immediate rewards must have one runtime owner. If a dialogue choice grants an
item through `dialogues.json` effects, the linked quest step must not duplicate
the same item in `quests.json/on_complete`. The quest can still preview or
describe the reward, but only one runtime action should grant it.

Flag and unlock ids are also compatibility ids. If a flag is no longer used by
new content, keep it readable for old saves and mark it deprecated. If a new gate
replaces it, migrate the old flag to the new one explicitly.

## Text And Balance

Authored text is not duplicated into save-state, so text can be edited without a
save migration.

Balance changes are allowed, but they have different effects:

- definition-level stat edits affect every instance that reads current item
  definitions;
- player-specific compensation, clamping, or swaps require a migration;
- changing reward amounts before a reward is claimed is a live balance change;
- changing already claimed rewards requires a compensation or rollback policy,
  not only a data edit.

## Required Future Tooling

Before external playtests, keep content compatibility validation enabled:

- maintain `design/data/content_compatibility.json` as the registry for ids that
  have shipped or entered saves;
- fail validation if a stable id disappears without a migration rule or legacy
  stub;
- fail validation if `quests.status_enum` and `GameState.QuestStatus` diverge;
- validate all quest/dialogue/effect/blocker references against current content;
- validate migrations on sample old saves.

## Current Implementation Gaps

Current runtime code does not yet fully protect this contract:

- `tools/validate_content_compatibility.py` protects authored stable ids during
  builds, but it is an authoring/build gate, not a runtime save migration layer.
- Runtime currently has a generated dialogue lookup, but no equivalent generated
  quest or item definition lookup API.
- `game_actions_grant_gear` writes gear `def_id` values without validating that
  the item definition exists.
- There is no save migration layer yet for missing quest/item ids.

Until those gaps are closed, designers should treat deletion or renaming of
quest, item, reward, flag, and unlock ids as blocked after a save can contain
them.
