---
type: Game Design Specification
title: RB Dark RPG Quest System
description: Quest authoring, runtime state, and first quest structure for RB Dark RPG.
tags: [gdd, quests, state, narrative]
game_id: rb-dark-rpg
status: draft
---

# Quest System

Status: draft

## Design Intent

Quests are the spine of the first slice. They drive hub interactions, equipment
tutorials, map unlocks, encounters, rewards, and clue journal updates.

Quest text and structure are data. Runtime code should only know how to evaluate
objective/event types such as `talk_to_npc`, `equip_item`, `win_encounter`, and
`visit_location`.

## Storage Split

Static authored content lives in:

```text
games/rb-dark-rpg/design/data/quests.json
```

Quest definitions are part of the broader modular content pack described in
`content_model.md` and loaded through `data/content_manifest.json`.

Future runtime save state should store only player progress:

```text
save.quest_state
```

Do not store per-save quest state inside the authored quest definition. Do not
hard-code quest text or step order in runtime code.

## Quest Definition

Each quest definition owns:

- stable `id`;
- player-facing title and short journal text;
- quest type: `main`, `contract`, `clue`, `tutorial`, or `side`;
- start conditions;
- ordered steps;
- objective descriptors;
- UI hints and blocked reasons;
- rewards;
- unlocks and flags.

Definition fields:

| Field | Meaning |
|---|---|
| `id` | Stable quest id used by state and references. |
| `title` | Player-facing title. |
| `type` | Main quest, contract, clue quest, tutorial, side quest. |
| `act` | Narrative act or content bucket. |
| `giver` | NPC/object that starts or advances the quest. |
| `start_location` | Where the quest first appears. |
| `prerequisites` | Required flags, completed quests, or unlocked locations. |
| `steps` | Ordered quest steps. |
| `rewards` | XP, gold, items, flags, map unlocks, quest unlocks. |
| `journal` | Short text shown in quest journal. |

## Step Definition

Each step should answer one concrete player action.

| Field | Meaning |
|---|---|
| `id` | Stable step id inside the quest. |
| `title` | Short objective title. |
| `description` | Journal detail for the step. |
| `location_id` | Where the step happens. |
| `objective` | Objective type and target. |
| `ui_hint` | One line hint when the player is blocked or lost. |
| `on_complete` | Flags, rewards, unlocks, or next step behavior. |

Keep first-slice quests linear. Branching can be represented later through
optional `choice_id`, but no branching is needed for Act I onboarding.

## Objective Types

V1 objective types:

| Type | Example | Completes When |
|---|---|---|
| `talk_to_npc` | Gate guard, blacksmith | Player opens/finishes required dialogue. |
| `receive_items` | Old sword, padded jacket | Items are granted or collected. |
| `equip_item` | Old sword | Required item is equipped. |
| `win_encounter` | Gate scavenger | Combat result is win. |
| `visit_location` | Old mill | Location becomes current or visited. |
| `collect_item` | Grain sacks | Inventory or encounter reward count reaches requirement. |
| `inspect_object` | Black Sun sign | Player inspects object and flag is set. |
| `return_to_npc` | Gate guard, elder | Player reports after prior step. |

Quest code should advance steps from events:

```text
npc_talked
item_received
item_equipped
encounter_won
location_visited
item_collected
object_inspected
quest_reported
```

## Quest Status

Use this status enum:

| Status | Meaning |
|---|---|
| `hidden` | Not shown and not available yet. |
| `available` | Can be accepted or started. |
| `active` | In progress. |
| `ready_to_turn_in` | Objectives complete, report/reward pending. |
| `completed` | Final rewards/unlocks applied. |
| `failed` | Reserved for future; not used in first slice. |

Blocked states are not separate quest statuses. They are UI explanations derived
from unmet requirements, such as missing gear, low HP, locked map node, or
unfinished objective.

## Runtime State Shape

Save state should be compact and id-driven:

```json
{
  "quest_state": {
    "tracked_quest_id": "q001_gate_pass",
    "quests": {
      "q001_gate_pass": {
        "status": "active",
        "current_step_id": "equip_old_sword",
        "completed_step_ids": ["talk_gate_guard"],
        "objective_progress": {
          "equip_old_sword": 0
        },
        "flags": ["gate_guard_intro_seen", "starter_gear_received"],
        "started_at_step": 1,
        "completed_at_step": null,
        "last_update_reason": "npc_talked"
      }
    },
    "global_flags": {
      "map_gate_unlocked": false,
      "seeker_token_owned": false
    }
  }
}
```

Runtime state should not duplicate quest text. It stores status, current step,
completed step ids, objective progress, choices, and flags.

## First Quest: Допуск за ворота

Purpose:

- teach the player that the hub is interactive;
- establish that leaving town requires permission and gear;
- give starter sword, armour, and greaves directly inside the first dialogue;
- prove first autobattle;
- unlock the map and the first contract.

Flow:

1. Talk to the gate guard at `Последний Пост`.
2. Receive `Старый меч`, `Стеганая куртка`, and `Кожаные поножи` from the guard.
3. Equip `Старый меч`.
4. Equip `Стеганая куртка`.
5. Equip `Кожаные поножи`.
6. Defeat `Падальщик у ворот`.
7. Return to the guard and receive `Жетон искателя`.
8. Unlock map access and `Хлеб для Поста`.

The first dialogue is mandatory. It may offer lore/clarification choices, but it
does not offer a refusal or "return later" branch.

The first dialogue reward block is shown in this order:

1. dialogue text;
2. separator;
3. `Задание`: one short objective line;
4. separator;
5. `Текущая награда`: reward cells received immediately;
6. separator;
7. `Награда за квест`: reward cells for quest completion;
8. mandatory action choices.

The quest should take under two minutes on a clean first playthrough.

## UI Contract

Quest journal shows:

- current tracked quest;
- current step title;
- one instruction line;
- linked location/NPC;
- rewards;
- blocked reason if action is unavailable;
- next action button when obvious.

Hub markers show:

- `!` for available quest;
- `?` for ready-to-turn-in;
- small tracked marker for current objective NPC/object.

Map nodes show:

- locked reason;
- linked quest id;
- threat label if the next step is combat.

## First-Slice Limits

Do not add in v1:

- branching quest outcomes;
- timed quests;
- repeatable daily quests;
- random objective selection;
- reputation gates;
- failure penalties;
- quest chains stored in code;
- MMO-style task spam.

Add these only after the first hub -> quest -> gear -> combat -> reward -> map
unlock loop is playable.
