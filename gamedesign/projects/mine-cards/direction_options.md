# Mine Cards Direction Options

## Decision

Current recommendation: build a `Melvor-like block mining idle RPG`.

The old card mechanics are removed from the first direction. They can survive
only as archived history or a much later optional expedition/combat mode.

## Option A - Block Mine Idle

Pitch: `Melvor Idle's tiny-start skilling loop, translated into an original
voxel mine fantasy`.

The player trains Mining, gathers ore, improves tools, unlocks deeper nodes, and
later grows into smithing/equipment/combat.

### First Screen

An idle activity screen:

- top: Mining level, coins, ore, current pickaxe;
- center: original blocky miner working a mine node;
- main panel: active node such as `Copper Vein`;
- progress bar: time to next ore;
- reward log: `+1 Copper Ore`, `Mining XP +4`;
- upgrade panel: `Copper Pickaxe`;
- small node picker: Surface Stone, Copper Vein, next locked node.

### Why It Fits Existing Art

- Old location/equipment/skills/chest art gives taste and future UI surfaces.
- The blocky mine theme is already present.
- Old card battle art is no longer first-slice scope, but can inform future
  danger/combat presentation.

### First Slice

Goal: upgrade from `Worn Pickaxe` to `Copper Pickaxe`.

Primary action: choose/continue a Mining node.

Three-beat FTUE:

1. Mine Stone/Copper.
2. See ore, XP, and mastery enter the UI.
3. Upgrade pickaxe and unlock/preview a deeper node.

### Risks

- Can become a dry spreadsheet if miner animation, reward feedback, and node
  art are weak.
- Needs strong IP hygiene: blocky mine fantasy, not Minecraft clone.
- Needs discipline: do not add 20 skills just because Melvor eventually has
  them.

## Option B - Card Expedition

Pitch: `Forward-like card descent in voxel mines`.

This is closest to the old PSDs and old combat notes.

### Why It Is Not Current Direction

The user explicitly moved away from cards and toward a Melvor-like clone in this
universe. This option is deferred. If revived, it should be a later combat
activity fed by the skilling economy, not the home loop.

## Option C - Full Melvor Clone Up Front

Pitch: launch with Mining, Woodcutting, Fishing, Cooking, Smithing, Combat,
Gear, Bank, Shop, Offline Progress, and many tabs.

### Why It Is Rejected

Melvor did not start that way. Its v0.01 shipped one skill, then deepened and
expanded over time. Starting with a large matrix would violate the repository's
first-screen scope discipline and likely stall before the core moment works.

## Recommendation

Choose Option A.

For this project, "Melvor-like" means:

```text
one skill that works -> deepen it -> add a resource consumer -> add combat
only when gear/resources need a purpose
```

For v0.01, that means:

```text
Mining-only idle loop + pickaxe upgrade + one rare mining event.
```
