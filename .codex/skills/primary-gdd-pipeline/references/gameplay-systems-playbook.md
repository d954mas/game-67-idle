# Gameplay Systems Playbook

Load this file only when defining gameplay, player verbs, rules, feedback,
risks, goals, stats, activities, UI flow, structured core-loop data, or the
first playable slice.

## Goal

Make the player action model concrete before writing broad lore, content lists,
or implementation phases. A GDD is not ready if the player cannot tell what to
click, what changes, and why the next action matters.

## Core Loop Frame

Write the loop as a chain:

```text
intent -> action -> rule/check -> consequence -> visible change -> choice/goal -> next intent
```

For each step, define:

- player input;
- timing, cost, risk, or constraint;
- output state;
- UI feedback;
- failure/blocked state;
- next available decision.

## System Map

For the first slice, fill this:

- Primary action:
- Secondary action:
- Activity/job:
- Progression or mastery hook:
- Stat affected:
- Resource/source, if any:
- Spend/sink, if any:
- Unlock:
- Visual/status change:
- Reason to repeat or continue:

Every system must connect to at least one player verb and one visible UI state.

## Core-Loop Data Minimum

Define only what the first slice needs:

- 1-3 player verbs;
- the rule/check that resolves each verb;
- one visible consequence or feedback event;
- one failure, blocked, or recovery state;
- one short-term goal or next decision;
- one replay/continue reason.

Add extra economies, crafting, inventory, multiple zones, or reset-meta loops
only after the first loop is playable and the lead explicitly chooses that
direction.

## Activity/Job Template

```markdown
## Activity: [name]
- Unlock condition:
- Input:
- Duration/cooldown:
- Cost:
- Consequence/reward:
- Stat effects:
- Failure/blocked state:
- Visual feedback:
- Progression affected by:
- UI location:
```

## Progression Template

```markdown
## Progression/Unlock: [name]
- Unlock condition:
- Cost/risk/requirement:
- Effect:
- Why player wants it:
- Visible before/after:
- Max level or scaling:
- Affected activity:
```

## UI Flow Minimum

The first playable UI needs:

- main screen with avatar/world/status;
- top bar with currencies/stats;
- primary action area;
- upgrade/activity panel;
- next goal/unlock hint;
- feedback toast/result;
- blocked state with requirement.

Do not split the first slice into many screens unless navigation itself is the
thing being tested.

## Core-Loop Sanity

Before writing `data/core_loop.json`, answer:

- How many clicks or seconds to first reward?
- How many actions to first new decision?
- What changes after the first meaningful success?
- How long to first visual/status milestone?
- What happens when the player stops, fails, retries, or re-enters?
- What prevents the player from being confused or stuck?

For danger/combat/challenge loops, also answer:

- What is the first enemy or obstacle?
- What are player HP/resource values before and after the encounter?
- What are enemy HP, damage, and turn/check rules?
- How many turns/clicks should a normal win take?
- What happens on low health, loss, retreat, or no supplies?
- What recovery action proves the camp/rest/heal loop matters?

## Machine-Readable Contract Notes

Prefer stable ids and explicit links:

- activity id references reward ids;
- upgrade id references affected activity/stat;
- unlock id references requirement and revealed content;
- UI action id references activity or upgrade;
- asset id references visual state or UI component.

Avoid numbers that exist only in prose.

## Challenge Contract Minimum

For RPG/adventure/survival first slices, create `data/combat.json` or an
equivalent structured file with:

- player baseline stats used by the encounter;
- enemy/obstacle stats;
- player actions and effects;
- enemy actions and effects;
- win/loss/retreat outcomes;
- expected first encounter path;
- tuning knobs likely to change.

Do not leave combat as "Attack/Defend/Use Item" only. That is UI vocabulary,
not implementable system behavior.

## Failure Modes

- The GDD names currencies but not sources/sinks.
- Jobs exist but do not affect stats or progression.
- Stats exist but do not change UI, unlocks, or rewards.
- Upgrades exist but the player cannot see why they matter.
- Core loop is described as a genre, not as actions and state changes.
- UI map has screens but no primary action path.
