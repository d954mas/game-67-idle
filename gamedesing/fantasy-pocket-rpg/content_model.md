# Content Model

## Goal

Make it clear how to produce a lot of future content without redesigning the game every time.

The content unit is not a long quest chain. The repeatable unit is:

```text
node -> encounter -> challenge/choice -> reward/state change -> camp/world response -> next hook
```

## Content Types

### Region

Purpose: a self-contained adventure area.

Template:

- id:
- name:
- fantasy:
- danger tier:
- visual motifs:
- factions:
- main mystery:
- node list:
- climax:
- unlocks next:

Target size:

- P0: 1 micro-region.
- P1: 1 small region with 3-5 nodes.
- Full region: 8-12 nodes.

### Map Node

Purpose: a destination with a clear reason to visit.

Template:

- id:
- name:
- type: road, ruin, cave, shrine, settlement, camp, ford, tower.
- unlock condition:
- visible hook:
- danger:
- primary encounter:
- reward:
- next unlock:

Good node hook examples:

- smoke behind the trees;
- broken bridge with claw marks;
- sealed shrine responding to dragon omen;
- hunter camp with missing scout clue;
- old tower visible from the road.

### Encounter

Purpose: a short playable decision.

Template:

- id:
- scene:
- setup text:
- player choices:
- challenge:
- success result:
- failure result:
- reward:
- camp follow-up:

Encounter types:

- search;
- fight;
- skill check;
- talk;
- hazard;
- relic interaction;
- companion scene;
- faction choice.

### Enemy

Purpose: a danger unit that teaches or tests a player decision.

Template:

- id:
- role:
- health:
- damage/action:
- special rule:
- reward:
- best counter:
- tutorial lesson:

Enemy roles:

- fast weak attacker;
- armored blocker;
- resolve-draining horror;
- ranged caster;
- elite guardian;
- boss/climax.

### Reward

Purpose: make the player feel changed after the node.

Reward categories:

- resource: gold, herbs, supplies;
- gear: weapon, armor, charm;
- route unlock;
- camp recipe;
- companion trust;
- dragon omen;
- faction reputation;
- lore clue.

Every node should give at least one visible reward or visible state change.

### Camp Beat

Purpose: convert adventure outcome into preparation and story.

Template:

- trigger:
- speaker:
- practical effect:
- story clue:
- next route hint:
- repeat behavior:

Camp beat examples:

- scout notices shard heat and marks a ruin on map;
- companion treats wound and unlocks rest tutorial;
- old map reveals safer path through ford;
- relic dream adds dragon omen milestone.

## Content Scaling Rules

- Add content by cloning templates, not inventing new systems.
- Each region must introduce at most one new mechanic.
- Every new mechanic must appear in UI, balance JSON, and validation.
- Every region needs a content budget before writing prose.
- Lore text is not content unless it creates a decision, reward, unlock, or relationship change.

## First Region: Moss Road

Purpose: teach expedition rhythm.

Nodes:

1. `old_road`: route tutorial.
2. `moss_ruins`: search + first combat.
3. `safe_fire`: camp tutorial.
4. `hunters_ford`: next route unlock.
5. `sealed_shrine`: first dragon omen gate.

Enemies:

- `ruin_wolf`;
- `bandit_scout`;
- `ash_wisp` later for Resolve pressure.

Rewards:

- herbs;
- rusty blade;
- dragon-marked shard;
- Trail Herbalist I;
- Hunter's Ford unlock.

## Content Done Criteria

A new content pack is ready when it has:

- region summary;
- node table;
- encounters;
- enemy/reward definitions;
- camp follow-up;
- balance data;
- UI route/state;
- validation path.
