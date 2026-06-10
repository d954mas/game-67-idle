# Gameplay Systems Playbook

Load this file only when defining gameplay, economy, stats, activities, jobs,
upgrades, UI flow, balance JSON, or the first playable slice.

## Goal

Make the player action model concrete before writing broad lore, content lists,
or implementation phases. A GDD is not ready if the player cannot tell what to
click, what changes, and why the next action matters.

## Core Loop Frame

Write the loop as a chain:

```text
intent -> action -> timer/check -> reward -> visible change -> unlock/choice -> next intent
```

For each step, define:

- player input;
- wait or cost;
- output number;
- UI feedback;
- failure/blocked state;
- next available decision.

## System Map

For the first slice, fill this:

- Primary action:
- Passive/idle action:
- Activity/job:
- Upgrade:
- Stat affected:
- Currency source:
- Currency sink:
- Unlock:
- Visual/status change:
- Reason to return:

Every system must connect to at least one player verb and one visible UI state.

## Economy Minimum

Define only what the first slice needs:

- 1 soft currency;
- 1-3 stats;
- 1 activity source;
- 1 upgrade sink;
- 1 unlock gate;
- 1 visual/status milestone.

Add premium currency, prestige, crafting, inventory, or multiple zones only
after the first loop is playable.

## Activity/Job Template

```markdown
## Activity: [name]
- Unlock condition:
- Input:
- Duration/cooldown:
- Cost:
- Reward:
- Stat effects:
- Failure/blocked state:
- Visual feedback:
- Upgrade affected by:
- UI location:
```

## Upgrade Template

```markdown
## Upgrade: [name]
- Unlock condition:
- Cost:
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

## Balance Sanity

Before writing JSON, answer:

- How many clicks or seconds to first reward?
- How many rewards to first upgrade?
- What changes after first upgrade?
- How long to first visual/status milestone?
- What happens while the player is idle?
- What prevents the player from being confused or stuck?

## Machine-Readable Contract Notes

Prefer stable ids and explicit links:

- activity id references reward ids;
- upgrade id references affected activity/stat;
- unlock id references requirement and revealed content;
- UI action id references activity or upgrade;
- asset id references visual state or UI component.

Avoid numbers that exist only in prose.

## Failure Modes

- The GDD names currencies but not sources/sinks.
- Jobs exist but do not affect stats or progression.
- Stats exist but do not change UI, unlocks, or rewards.
- Upgrades exist but the player cannot see why they matter.
- Core loop is described as a genre, not as actions and state changes.
- UI map has screens but no primary action path.
