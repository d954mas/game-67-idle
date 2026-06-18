# Mine Cards Parameters

Status: `base draft / v0.01`.

Machine-readable source:

`data/parameters.json`

These numbers are intentionally small and provisional. They exist so the first
native slice can be implemented and tested without inventing tuning.

## Tuning Goals

| Goal | Target |
|---|---:|
| First reward | 3 seconds |
| First node decision | 15-30 seconds |
| First upgrade | 3-5 minutes |
| First session readable content | 1 skill, 2 active nodes, 1 locked preview |
| Primary session length | 5 minutes |

## Global Constants

| Parameter | Value | Reason |
|---|---:|---|
| `tick_resolution_seconds` | 0.1 | Smooth enough for progress UI |
| `reward_log_rows` | 5 | Readable without taking whole screen |
| `starting_node` | `surface_stone` | Teachable first reward |
| `starting_tool` | `worn_pickaxe` | Makes first upgrade meaningful |
| `geode_chance_per_tick` | 0.03 | Rare but visible over a few minutes |

## Resources

| ID | Label | Starts at | Cap | Notes |
|---|---|---:|---:|---|
| `stone` | Stone | 0 | none for v0.01 | Starter material |
| `copper_ore` | Copper Ore | 0 | none for v0.01 | First valuable ore |
| `coins` | Coins | 0 | none for v0.01 | Upgrade currency |
| `mining_xp` | Mining XP | 0 | level curve | Skill progress |
| `mastery_xp` | Mastery XP | per node 0 | mastery curve | Node progress |

## Mining Nodes

| ID | Label | Unlock | Interval | Reward | XP | Mastery XP |
|---|---|---|---:|---|---:|---:|
| `surface_stone` | Surface Stone | start | 3.0s | `stone +1` | 2 | 1 |
| `copper_vein` | Copper Vein | Mining Lv 2 | 5.0s | `copper_ore +1`, `coins +1` | 4 | 1 |
| `iron_deposit` | Iron Deposit | locked preview | 8.0s | deferred | deferred | deferred |

## Sell Values / Coin Injection

v0.01 should avoid a full shop. Coins can enter through small implicit sell
value or geodes.

| Source | Coins | Notes |
|---|---:|---|
| Surface Stone tick | 0 | Keeps the starter node focused on teaching item/XP gain |
| Copper Vein tick | 1 fixed | Makes the first coin path deterministic, not Geode-RNG gated |
| Geode event | 8 | Main coin burst |

Current base assumes Copper Pickaxe uses both Copper Ore and Coins. Copper Vein
therefore grants a fixed small coin reward so the first upgrade is not gated by
rare-event variance.

## XP Curve

Small v0.01 curve:

| Mining Level | Total XP Required | Unlock |
|---:|---:|---|
| 1 | 0 | Surface Stone |
| 2 | 12 | Copper Vein if not already tutorial-unlocked |
| 3 | 32 | Copper Pickaxe visibility or faster Stone mastery preview |
| 4 | 64 | Iron Deposit preview stronger |
| 5 | 110 | End of v0.01 test range |

## Mastery Tiers

Per node:

| Tier | Mastery XP | Effect |
|---:|---:|---|
| 0 | 0 | no bonus |
| 1 | 10 | -3% interval on that node |
| 2 | 30 | +1% geode chance on that node |
| 3 | 60 | +10% chance for +1 extra resource on that node |

Keep mastery visible but not dominant in the first build.

## Tools

| ID | Label | Unlock | Effect |
|---|---|---|---|
| `worn_pickaxe` | Worn Pickaxe | start | `interval_multiplier = 1.0` |
| `copper_pickaxe` | Copper Pickaxe | Copper Vein discovered | `interval_multiplier = 0.85` |

## Upgrades

| ID | Cost | Effect | Target Time |
|---|---|---|---|
| `upgrade_copper_pickaxe` | `stone 6`, `copper_ore 32`, `coins 32` | unlocks `copper_pickaxe` | about 3 minutes |

If the first test feels too slow, reduce cost before increasing rewards. Faster
first upgrade is better than inflated resource counts.

## Rare Event

| ID | Trigger | Chance | Reward | UX |
|---|---|---:|---|---|
| `geode` | completed Mining tick | 3% | `coins +8`, `mining_xp +5` | callout or auto-collect |

Geode cannot block the main progress bar.

## UI Thresholds

| UI State | Rule |
|---|---|
| Affordable upgrade | all costs met |
| Nearly affordable | at least 75% of every cost |
| Locked node | missing level/tool shown in row |
| New reward highlight | last reward row stays highlighted for 1.2s |
| Level-up callout | show once, then compact to log |
| Mastery tier callout | small node-row effect, no blocking modal |

## Validation Parameters

| Check | Pass Target |
|---|---|
| First reward | <= 4s from screen start |
| First visible upgrade target | <= 10s |
| First meaningful decision | <= 30s |
| Text readability | readable in zoom crop |
| Primary action clarity | new player can identify active node |

## Future Parameter Groups

Deferred until later:

- Smithing recipes;
- equipment stats;
- combat stats;
- food/healing;
- offline simulation;
- automation upgrades;
- card expedition tuning;
- prestige/reset economy.
