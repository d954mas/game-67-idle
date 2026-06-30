# Gameplay Systems

Load this when defining gameplay, player verbs, rules, feedback, risks, goals,
stats, activities, UI flow, structured core-loop data, or the first playable
slice.

## Goal

Make the player action model concrete before writing broad lore, content lists,
or implementation phases.

## Core Loop

Write the loop as:

```text
intent -> action -> rule/check -> consequence -> visible change -> choice/goal -> next intent
```

For each step, define:

- player input;
- timing, cost, risk, or constraint;
- output state;
- UI feedback;
- failure, blocked, or recovery state;
- next available decision.

## First Slice Minimum

Define only what the first slice needs:

- 1-3 player verbs;
- the rule/check that resolves each verb;
- one visible consequence or feedback event;
- one blocked, failure, or recovery state;
- one short-term goal or next decision;
- one reason to repeat or continue.

Avoid extra economies, crafting, inventory, zones, or meta loops until the first
loop is playable and the lead chooses that direction.

## UI Minimum

The first playable UI needs:

- main screen with avatar/world/status;
- top bar with currencies/stats;
- primary action area;
- upgrade/activity panel;
- next goal or unlock hint;
- feedback toast/result;
- blocked state with requirement.

Do not split the first slice into many screens unless navigation is the thing
being tested.

## Challenge Minimum

For RPG, adventure, survival, combat, or other danger loops, create
`data/combat.json` or an equivalent structured file with:

- player baseline stats used by the encounter;
- enemy or obstacle stats;
- player actions and effects;
- enemy or check actions and effects;
- win, loss, retreat, and recovery outcomes;
- expected first encounter path;
- tuning knobs likely to change.

Do not leave challenge as button labels only. UI vocabulary is not enough.

## Failure Modes

- Currencies exist but no source/sink exists.
- Jobs exist but do not affect stats or progression.
- Stats exist but do not change UI, unlocks, or rewards.
- Upgrades exist but the player cannot see why they matter.
- Core loop is a genre description, not actions and state changes.
- UI map has screens but no primary action path.
