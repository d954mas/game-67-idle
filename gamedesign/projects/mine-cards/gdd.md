# Mine Cards GDD

Status: `base GDD / pre-runtime`.

This document is the current design source of truth for the first iteration of
`Mine Cards`: a Melvor-like block mining idle RPG in an original voxel mine
universe.

## Definition Of Done For This Base

This base is done when a future implementation agent can build the first native
screen without inventing the core loop, first resources, first upgrade, UI
states, or validation gates.

This base is not trying to define all future mechanics, full skill synergies,
combat, offline economy, or final balance.

## Product Direction

`Mine Cards` is now a working title. The old card-crawler design is archived.
The current product is:

```text
skill-first block mining idle RPG
```

Reference anchor:

- Melvor Idle for skill-first idle progression and long-term accumulation.
- The project's old art archive for blocky mine fantasy and portrait UI taste.

Translation:

```text
Melvor starts from one skill -> Mine Cards starts from Mining.
```

## One-Sentence Hook

Run a blocky miner through an idle mining loop: choose a node, watch the miner
work, collect ore and mastery, upgrade the pickaxe, and unlock deeper mine
layers.

## Player Fantasy

The player is not just clicking a button. They are building a small mine career:

- beginning with surface stone and a worn pickaxe;
- finding copper and rare geodes;
- improving tools;
- opening deeper layers;
- eventually turning gathered resources into gear, automation, and danger-ready
  expedition power.

## Audience

Primary audience:

- Melvor Idle / RuneScape skilling fans who enjoy long-term growth;
- casual idle/incremental players who want clear rewards and upgrades;
- players attracted to blocky mining fantasy but not expecting Minecraft rules.

Design promise:

- easy first minute;
- readable progress;
- visible resource use;
- deep future system space.

## Platform And Runtime

Primary design composition:

- portrait mobile-like layout, derived from the old `1080x1920` art direction;
- native PC first for this repository;
- desktop window centers the portrait surface over a wider mine backdrop.

Implementation rule:

- game visuals must use real assets through the engine asset path;
- no debug shape renderer for shippable visuals;
- no web prototype unless the lead explicitly asks for web/mobile/browser work.

## Design Pillars

### 1. Tiny Start, Deep Promise

The first playable slice has one skill and one main action. The screen can hint
at depth, but it must not require a full skill matrix to be fun.

Violation:

- showing Mining, Smithing, Combat, Shop, Achievements, Offline, Gear, and
  Dungeons as active systems before Mining itself works.

### 2. Every Resource Has A Next Use

Ore, coins, XP, mastery, and geodes must point to visible next value.

Violation:

- a reward log full of resources that cannot be spent, sold, unlocked, or
  explained.

### 3. Idle But Alive

The game is allowed to be idle, but the screen should feel active: miner motion,
progress, reward log, node state, unlock previews, and occasional rare events.

Violation:

- a static spreadsheet where the only feedback is a number changing.

### 4. Original Blocky World

Use square/voxel readability and mining affordances, but do not copy Minecraft,
Melvor, RuneScape, or the old Steve-like proportions as final public art.

Violation:

- recognizable Minecraft silhouettes, creeper-like enemies, copied block
  textures, or a Melvor layout clone.

## Core Game Model

The long-term game is built from these entity types:

| Entity | Meaning | v0.01 status |
|---|---|---|
| Skill | A trainable activity family such as Mining | Active: Mining only |
| Node | A target inside a skill, such as Surface Stone | Active |
| Tick | One completed progress interval | Active |
| Resource | Item/currency gained from ticks or events | Active |
| Tool | Equipment that modifies a skill | Active: pickaxe only |
| Upgrade | Spend resources to improve a tool/system | Active: Copper Pickaxe |
| Mastery | Node-specific repeated-use progress | Active, simple |
| Event | Rare moment layered over the base tick | Active: Geode |
| Unlock | New node/tool/system revealed by level/resources | Active, minimal |
| Inventory | Stored resources | Active as simple counts |
| Bank | Larger item-management surface | Deferred |
| Crafting | Resource conversion | Deferred |
| Combat | Gear test and danger loop | Deferred |
| Cards | Possible later combat/exploration presentation | Deferred |

## v0.01 Scope

### In Scope

- one skill: Mining;
- one main screen: Mining Activity;
- node picker with 2 active/visible starter nodes and 1 locked preview node;
- progress timer;
- reward log;
- resources: Stone, Copper Ore, Coins;
- progression: Mining XP, Mining Level, node Mastery XP;
- one rare event: Geode/Rich Vein;
- one upgrade family: Pickaxe;
- one first upgrade: Copper Pickaxe;
- save-friendly state shape.

### Out Of Scope

- combat;
- card runs;
- smithing;
- full equipment grid;
- food;
- offline progress;
- shop as a full economy;
- achievements;
- pets;
- prestige/reset;
- multiple biomes;
- generated procedural mine content.

## Core Loop

```text
choose node -> mine tick -> receive resource/xp/mastery ->
check rare event/unlock -> spend on pickaxe -> mine faster or unlock deeper node
```

The loop must be readable at three scales:

- immediate: progress bar fills and reward log updates;
- short-term: first upgrade becomes affordable;
- medium-term: a deeper node becomes visible and desirable.

Detailed core loop spec:

`core_loop.md`

Machine-readable contract:

`data/core_loop.json`

## First 30 Seconds

Target: the player understands what is happening without a tutorial wall.

Sequence:

1. The screen opens on `Surface Stone`.
2. The miner is visibly working.
3. A large progress bar completes within about 3 seconds.
4. The reward log shows `+1 Stone`, `Mining XP +2`, and mastery progress.
5. The UI points to `Copper Vein` as the next better node.
6. The pickaxe panel shows a first upgrade and its missing cost.

Success criteria:

- player can name the active action;
- player can name the reward;
- player can see a next upgrade;
- player can see why mining again matters.

## First 5 Minutes

Target: one complete micro-progression arc.

Expected arc:

1. Mine starter stone.
2. Switch to Copper Vein after the first unlock prompt.
3. Earn Copper Ore and coins.
4. See at least one Geode/Rich Vein bonus or its chance preview.
5. Upgrade to Copper Pickaxe.
6. Notice faster mining or a newly unlocked deeper node preview.

Success criteria:

- player feels persistent growth;
- player has a reason to continue for the next node;
- no second system is required to make the first loop meaningful.

## Resources

| Resource | Type | Source | Sink | v0.01 purpose |
|---|---|---|---|---|
| Stone | item | Surface Stone ticks | first upgrade/sell conversion | Teaches item gain |
| Copper Ore | item | Copper Vein ticks | Copper Pickaxe, later Smithing | First valuable ore |
| Coins | soft currency | sell value, geode | upgrade cost | Universal early sink |
| Mining XP | progression | all Mining ticks | level curve | Unlocks nodes |
| Mastery XP | node progress | repeated node use | mastery tiers | Makes repetition valuable |
| Geode | event item/value | rare event | coins or later crafting | Active surprise moment |

Rule:

- v0.01 should avoid adding resources unless they either unlock a node, buy a
  pickaxe, or visibly explain future Smithing.

## Stats

| Stat | Scope | Changes when | Player-facing effect |
|---|---|---|---|
| Mining Level | skill | Mining XP crosses threshold | unlocks nodes/tools |
| Node Mastery | per node | Mastery XP crosses threshold | improves speed/yield/chance |
| Mining Interval | activity | tool/mastery modifiers apply | progress bar completes faster |
| Yield | activity | node/tool/mastery modifiers apply | more resources per tick |
| Geode Chance | activity | event config/mastery/tool modifies | rare bonus appears more often |

## Mining Nodes

### Surface Stone

- Goal: teach mining with a fast, safe node.
- Unlock: start.
- Base interval: 3 seconds.
- Reward: Stone, Mining XP, Mastery XP.
- Player reason: fast first reward and first unlock path.

### Copper Vein

- Goal: first meaningful ore target.
- Unlock: after starter prompt or early Mining requirement.
- Base interval: 5 seconds.
- Reward: Copper Ore, Mining XP, Mastery XP, coin chance.
- Player reason: Copper Pickaxe cost and later Smithing promise.

### Iron Deposit

- Goal: visible long-term promise.
- Unlock: not reachable in v0.01 unless explicitly approved.
- UI role: locked preview.
- Player reason: proves depth without adding another real system.

## Tools And Upgrades

### Worn Pickaxe

- Starting tool.
- No special bonus.
- Must be visible as weak but functional.

### Copper Pickaxe

- First upgrade.
- Cost: Stone, Copper Ore, and Coins in the current draft.
- Effect: Mining interval multiplier around `0.85`.
- Before/after: Copper Vein `5.0s -> 4.25s`.
- Player reason: immediately faster Mining.

Upgrade rule:

- every upgrade must show a before/after number;
- unaffordable state must show exact missing resources;
- buying the upgrade must update the progress bar timing without requiring a
  scene reload.

## Rare Event: Geode / Rich Vein

Purpose:

- break pure waiting with a small active or visible surprise;
- create a coin injection without a shop economy;
- foreshadow later gem/crafting systems without enabling them yet.

Rules:

- chance is checked on completed Mining ticks;
- event must not interrupt the base loop;
- reward is small enough that the pickaxe upgrade still comes from Mining;
- if the player ignores it, it can auto-collect or expire depending on the final
  UX decision.

v0.01 default:

- 3% chance per tick;
- reward: coins plus Mining XP;
- no separate gem economy yet.

## UI Flow

Primary screen:

`Mining Activity`.

Required UI zones:

- top HUD: Mining Level, coins, Stone, Copper Ore;
- world/hero area: miner and active node;
- activity panel: selected node, progress bar, timer, current state;
- reward log: last 3-5 rewards;
- node picker: current node, available node, locked preview;
- upgrade panel: current pickaxe, next pickaxe, cost, before/after;
- event callout: geode/rich vein;
- minimal navigation: Mining, Upgrades, Inventory only if needed visually.

Deferred UI:

- Shop;
- Achievements;
- full Skills screen;
- Combat tab;
- Equipment tab;
- Card tab.

## UX Rules

- First reward must happen quickly enough to prove the loop.
- The primary action area must be visually larger than secondary navigation.
- Locked nodes must explain requirements.
- Costs must be legible and must not sit on busy art.
- The progress bar must remain visible during events.
- The first screen must make `what is running` and `what grows` obvious within
  about 10 seconds.

## Visual Direction

Accepted direction draft:

`visual/fake_shots/mining_v001_fake_shot_2026-06-17.png`

Use it as mood and composition direction, not as pixel target.

Target qualities:

- chunky voxel-inspired miner;
- warm mine/workshop environment;
- readable UI plates;
- large progress bar and reward log;
- saturated ore/coin/resource icons;
- not a dark unreadable cave;
- not a generic fantasy card game.

Known fake-shot mismatches:

- resource counts are too high for a fresh start;
- bottom nav shows later surfaces;
- generated text must be replaced by real UI;
- timing values may differ from `data/parameters.json`.

### Product Differentiator: Living 3D Voxel Hero

Melvor Idle's current presentation is mostly static app/panel UI. Mine Cards can
differentiate by making the miner and equipment visibly alive.

Direction:

- use a real 3D voxel/low-poly miner in the Mining screen;
- use modular equipment meshes for pickaxe, helmet, armor, and later weapons;
- animate the first character through simple transform curves, not skeletal
  animation;
- make gear upgrades visible on the character when they correspond to real
  progression.

Current technical base:

- the engine has mesh rendering and GLB import examples;
- skeletal animation/skinning is not currently a safe v0.01 dependency;
- modular mesh parts and procedural transform animation are the safe fallback;
- a skeletal animation spike is strategically valuable because it unlocks
  ready humanoid clips, better elbows/knees, and animated gear/clothing.

Detailed direction:

`visual/3d_character_direction.md`

Animation technical options:

`visual/animation_runtime_options.md`

## Audio Direction

Audio is not v0.01 required, but the design should leave room for:

- pickaxe hit tick;
- ore collect;
- XP/mastery tick;
- geode sparkle;
- upgrade purchase.

If implemented early, audio must reinforce feedback rather than add noise.

## Save State

v0.01 state should be simple:

- selected mining node;
- progress elapsed/current tick start if needed;
- resources;
- Mining XP/level;
- node mastery;
- owned pickaxe;
- seen tutorial hints;
- last reward log entries if UI wants persistence.

Offline progress is deferred. The state should not block later offline support,
but v0.01 does not need to simulate time away from game.

## Future Mechanics And Synergy Axes

These are extension axes, not v0.01 commitments. The durable expansion rules
live in:

`systems_foundation.md`

Machine-readable system registry:

`data/systems_registry.json`

| Axis | Future purpose | Depends on |
|---|---|---|
| Smithing | consumes ore into bars/tools/gear | Mining has enough resources |
| Equipment | gives crafted items a reason | Smithing exists |
| Combat | tests equipment and food | Gear/economy exists |
| Cards/Expedition | possible compact combat presentation | Combat needs a visual mode |
| Offline Progress | idle expectation | save model and active loop stable |
| Bank | manages many item types | more than a few resources |
| Automation | reduces repeated manual choices | first loop proven |
| Mastery Expansion | node-specific long-term goals | first mastery readable |

Rule for future expansion:

```text
new mechanic must consume, improve, unlock, risk, visualize, or create a return
reason for an existing mechanic
```

No standalone minigames until the Mining base is proven.

Current v0.01 dependency chain:

```text
Mining -> Copper Ore/Coins -> Copper Pickaxe -> faster Mining/deeper preview
```

Future mechanics must declare a synergy type before implementation, such as
`source_to_sink`, `upgrade_to_activity`, `activity_to_gear`, `gear_to_risk`,
`risk_to_resource`, `event_to_bonus`, `resource_to_unlock`,
`mastery_to_efficiency`, or `visual_to_motivation`.

## Reference Grounding

Current reference docs:

- `references/melvor_idle_reference_intake_2026-06-17.md`
- `references/melvor_history_deconstruction_2026-06-17.md`

Core reference lesson:

- Melvor's current size is a long-term result.
- Its early start was one skill, clear progress, tools, random events, and
  immediate UI fixes.
- Our base should start with Mining only, then deepen and connect systems later.

## Validation Gates

Before implementation starts:

- lead accepts the base GDD;
- first-slice parameters are acceptable as draft;
- fake shot direction is accepted or redirected.

Before a native slice is called done:

- native build runs;
- native screenshot exists;
- screenshot is compared qualitatively against the fake shot;
- UI readability zoom gate passes;
- product/game-loop gate confirms a new player can understand action, reward,
  upgrade, and reason to continue.

## Current Open Decisions

1. Should the first visible selected node be `Surface Stone` or `Copper Vein`?
   Current base assumes `Surface Stone` for teachability.
2. Should Copper Pickaxe cost be simplified after playtest?
   Current reviewed draft uses `stone 6`, `copper_ore 32`, and `coins 32` to
   give the starter Stone a first sink while keeping Copper Ore as the gating
   resource.
3. Should Geode be auto-collected or actively tapped?
   Current base allows either, but it must not interrupt Mining.
4. How far from Minecraft-like proportions should the first original miner move?
   Current base treats the fake-shot miner as direction, not final IP-safe art.

## Source Order

For future agents, read in this order:

1. `README.md`
2. `gdd.md`
3. `core_loop.md`
4. `parameters.md`
5. `systems_foundation.md`
6. `data/parameters.json`
7. `data/systems_registry.json`
8. `data/core_loop.json`
9. `data/balance.json`
10. `data/ui_flow.json`
11. `visual/3d_character_direction.md`
12. `visual/fake_shots/mining_v001_fake_shot_2026-06-17_review.md`
13. `game_implementation_plan.md`
