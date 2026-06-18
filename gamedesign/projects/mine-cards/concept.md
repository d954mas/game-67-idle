# Mine Cards Concept

## One-Line Hook

A blocky mining idle RPG inspired by Melvor Idle's skill-first progression:
train one activity, gather resources, upgrade tools, unlock deeper layers, and
eventually use crafted gear in combat.

## Current Direction

The old `card descent` idea is archived. It may return later as a dungeon or
expedition mode, but it is not the first playable loop and not the product
spine.

Working genre:

```text
Melvor-like block mining idle RPG in an original voxel mine universe.
```

## Fantasy

You manage a small blocky miner-adventurer who grows from breaking surface
stone into running a deep mine, discovering rare veins, crafting better tools,
and preparing for the dangerous lower layers.

## Audience

- Players who like Melvor Idle / RuneScape-style progression but want a more
  visual, cozy block-mining fantasy.
- Casual players who understand voxel/block affordances without needing
  Minecraft-specific characters, enemies, or item silhouettes.
- Idle/incremental players who enjoy long-term growth from simple actions.

## Platform And Screen

- Primary layout: portrait, derived from the old `1080x1920` art direction.
- Native PC remains the implementation harness for this repository.
- Desktop framing can center the portrait game view over a wide mine backdrop.

## Core Verbs

- Select one active skill activity.
- Wait for progress ticks.
- Collect resource, XP, mastery, and occasional rare finds.
- Spend resources/coins on tool upgrades.
- Unlock better nodes and deeper mine layers.
- Later: convert resources into gear and test that gear in combat.

## Pillars

1. **Tiny start, deep promise.** First slice is one skill done well, not a
   complete Melvor clone.
2. **Every resource has a visible next use.** Ore, coins, mastery, and rare
   finds point to a concrete unlock or upgrade.
3. **Blocky world, original IP.** Use voxel/mining readability without copying
   Minecraft or Melvor UI/assets.
4. **Idle but not dead.** Progress can run passively, but the screen should show
   a living miner, changing node, reward log, and clear next decision.

## No-Go List

- No cards in v0.01.
- No combat in v0.01.
- No full skill matrix on the first screen.
- No web prototype unless explicitly approved for a separate visual document.
- No copied Minecraft/Melvor characters, blocks, icons, UI layout, or naming
  set.
- No implementation-readiness claim without a current reference digest and a
  small core-loop contract.

## First Slice

`Mine Cards v0.01` should be a Mining-only idle slice:

1. Player selects `Mine Stone` or `Mine Copper`.
2. A progress bar fills and grants ore, XP, and a small coin chance.
3. Mining XP increases level; mining mastery increases node efficiency.
4. A rare `Geode` event can appear as a short optional bonus.
5. Coins/ore buy the first pickaxe upgrade.
6. The better pickaxe makes the next node faster or unlocks it.

The slice is successful only if a new player understands within about 10
seconds: what is running, what was gained, what can be upgraded, and why the
next mining node matters.
