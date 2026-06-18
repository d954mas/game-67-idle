# Mine Cards Systems Foundation

Status: `base architecture / expansion framework`.

Purpose:

Define how Mine Cards grows from the Mining v0.01 base into a Melvor-like
systems game without turning the first slice into a huge feature matrix.

Machine-readable registry:

`data/systems_registry.json`

## Expansion Principle

Every new mechanic must do at least one of these:

- consume an existing resource;
- improve an existing activity;
- unlock a new activity;
- create a meaningful risk or constraint for an existing reward;
- make an existing upgrade visually/status-legibly stronger;
- create a reason to return to an earlier activity.

Mechanics that only add another tab, another currency, or another number are
not allowed until they connect to the current loop.

## Current System Graph

```text
Mining Node
  -> produces Stone/Copper Ore
  -> produces Mining XP
  -> produces Node Mastery XP
  -> rolls Geode Event

Copper Ore + Coins
  -> buy Copper Pickaxe

Copper Pickaxe
  -> improves Mining interval
  -> previews deeper node

Mining Level + Mastery
  -> unlock/boost nodes
  -> future: unlock Smithing inputs
```

## Future System Order

The order is intentionally dependency-driven, not content-driven.

| Layer | System | Adds | Depends on | Why it comes here |
|---:|---|---|---|---|
| 0 | Mining | resources, XP, mastery, pickaxe | none | proves the base idle loop |
| 1 | Mining Depth | more nodes, better ores, richer mastery | Mining | deepens the first skill before adding breadth |
| 2 | Smithing | ore -> bars/tools/gear | Mining resources | creates a resource consumer |
| 3 | Gear Slots | visible equipment stats and avatar changes | Smithing outputs | makes crafted items matter visually |
| 4 | Basic Combat | automated enemy test | gear + food/recovery plan | gives gear a risk/reward purpose |
| 5 | Food/Cooking | healing/sustain | combat damage loop | supports longer combat |
| 6 | Expedition/Card Mode | compact active danger presentation | combat loop | optional use of old card art grammar |
| 7 | Offline Progress | time-away rewards | stable save model and balance | expected by idle players, risky before loop is tuned |
| 8 | Automation | auto-switch, auto-sell, auto-upgrade helpers | repeated actions | reduces friction only after repetition is known |
| 9 | Meta/Prestige | long-term reset or depth prestige | mature economy | late retention, not early proof |

## System Categories

### Activity Systems

Activities are trainable loops with progress timers and rewards.

Examples:

- Mining;
- Smithing;
- Cooking;
- Combat;
- Expedition.

Required fields:

- player verb;
- duration/interval;
- input cost or requirement;
- output reward;
- affected stats;
- blocked state;
- upgrade hooks;
- UI feedback.

### Resource Systems

Resources are stored values the player earns and spends.

Types:

- item resource: Stone, Copper Ore, Iron Ore;
- soft currency: Coins;
- progression resource: XP/Mastery;
- sustain resource: Food later;
- rare resource: Geode/Gems later.

Rule:

Do not add a resource unless it has at least one visible source and one visible
sink in the current or next planned layer.

### Upgrade Systems

Upgrades make an existing loop better.

Valid upgrade effects:

- shorter interval;
- higher yield;
- higher rare-event chance;
- unlock node;
- unlock recipe;
- visual avatar/gear change;
- new automation behavior.

Every upgrade must show before/after.

### Unlock Systems

Unlocks reveal new choices without overwhelming the first screen.

Valid unlock requirements:

- skill level;
- mastery tier;
- owned tool;
- resource count;
- completed tutorial beat;
- combat/expedition result later.

Unlock UI rule:

Locked content may be visible only if the requirement is clear and the player
can understand why it matters.

### Risk Systems

Risk systems introduce failure, recovery, or resource pressure.

Examples:

- combat HP loss;
- food consumption;
- expedition retreat;
- tool durability only if deliberately chosen;
- dangerous deeper nodes if later added.

Rule:

Risk cannot enter v0.01. Add it only when it improves a proven reward loop.

### Event Systems

Event systems create a small spike of attention without necessarily adding
danger.

Examples:

- Geode;
- rich vein;
- double-yield moment;
- later: wandering merchant or rare node.

Rule:

Events can enter v0.01 only if they do not block the main progress loop and do
not introduce a separate economy.

## Synergy Types

Mine Cards should grow by explicit synergy types.

| Synergy | Shape | Example |
|---|---|---|
| Source -> Sink | one activity produces input for another | Mining Ore -> Smithing Bars |
| Upgrade -> Activity | upgrade improves a loop | Copper Pickaxe -> faster Mining |
| Activity -> Gear | activity creates equipment | Smithing -> Helmet/Sword/Pickaxe |
| Gear -> Risk | equipment improves dangerous action | Armor -> safer Combat |
| Risk -> Resource | dangerous action yields rare reward | Combat -> monster drops |
| Event -> Bonus | rare non-danger event injects reward/attention | Geode -> Coins/XP |
| Resource -> Unlock | stored value opens new content | Coins/Ore -> deeper node/tool |
| Mastery -> Efficiency | repetition improves a specific loop | Copper Mastery -> more Copper |
| Visual -> Motivation | progression changes avatar/screen | Copper Pickaxe appears in hand |

Every future mechanic should declare its synergy type before implementation.

## Skill Families

### Gathering

Purpose:

Creates raw resources and long-term mastery goals.

Candidate skills:

- Mining;
- Woodcutting if the world needs wood/logs;
- Harvesting/Farming later if food/crafting needs it.

Expansion rule:

Do not add a gathering skill unless its resource feeds a near-term sink.

### Processing

Purpose:

Consumes gathered resources and creates intermediate goods.

Candidate skills:

- Smelting;
- Smithing;
- Cooking.

Expansion rule:

Processing must convert resource abundance into a meaningful decision, not just
another progress bar.

### Equipment

Purpose:

Turns resources into visible power.

Candidate systems:

- tools;
- armor;
- weapons;
- accessories;
- avatar clothing/appearance.

Expansion rule:

Gear must affect an activity and, where possible, appear on the 3D avatar.

### Danger

Purpose:

Creates risk, sustain needs, and gear tests.

Candidate systems:

- automated combat;
- expeditions;
- bosses;
- old card-mode descent.

Expansion rule:

Do not add danger until there is gear or sustain for it to test.

### Convenience

Purpose:

Reduces friction after the player understands repetition.

Candidate systems:

- offline progress;
- auto-switch node;
- auto-sell;
- auto-equip;
- loadouts.

Expansion rule:

Convenience is earned or unlocked after manual meaning exists.

## Mechanic Spec Template

Use this template before adding any new mechanic.

```markdown
## Mechanic: [name]

Status:

Layer:

Player verb:

Primary synergy type:

Depends on:

Produces:

Consumes:

Improves:

Unlocks:

Risk/blocked state:

Visible feedback:

Avatar/equipment effect:

First proof:

Out of scope:
```

## Design Gates For New Systems

Before a system moves from idea to implementation:

- it has a dependency in the system graph;
- it declares source/sink or upgrade/activity linkage;
- it has a UI place;
- it has a blocked state;
- it has one visible proof scenario;
- it does not require more than one new major system to make sense.

Before a system is called done:

- native screenshot/proof exists if visual/playable;
- player can explain what changed;
- parameters exist in JSON or data file;
- task log records validation;
- any reference-driven behavior has a durable deconstruction.

## Future Mechanics Backlog

This is not a task list. It is a design parking lot.

### Strong Candidates

- Smithing as the first resource consumer.
- Visible pickaxe/helmet/armor swaps on the 3D avatar.
- Rigid gear attachments before skinned wearables.
- Skeletal animation spike if Mixamo-style clips are selected.
- Offline progress after Mining timing is tuned.

### Conditional Candidates

- Cooking/Food only after combat needs sustain.
- Card expeditions only after combat needs an active presentation.
- Bank only after resource count exceeds the first screen's simple inventory.
- Automation only after the player has repeated the manual loop enough.

### Not Now

- prestige/reset;
- premium currencies;
- pets;
- broad achievements;
- multi-biome world map;
- procedural dungeon cards;
- full equipment rarity economy.

## Base Audit

The current base is allowed to mention future systems, but must not require them
for v0.01.

Current v0.01 dependency chain:

```text
Mining -> Copper Ore/Coins -> Copper Pickaxe -> faster Mining/deeper preview
```

If a future edit makes v0.01 depend on Smithing, Combat, Offline Progress, or
Cards, it must be explicitly approved as a scope expansion.
