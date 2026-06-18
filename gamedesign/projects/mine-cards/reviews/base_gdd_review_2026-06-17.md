# Mine Cards Base GDD Review - 2026-06-17

Status: `reviewed / ready to plan tasks, not ready for runtime implementation`.

Scope reviewed:

- `gdd.md`
- `core_loop.md`
- `parameters.md`
- `systems_foundation.md`
- `game_implementation_plan.md`
- `data/parameters.json`
- `data/core_loop.json`
- `data/balance.json`
- `data/ui_flow.json`
- Melvor reference intake and history deconstruction
- Mining v0.01 fake-shot review
- 3D character and animation direction notes

## Verdict

The base direction is coherent and worth continuing:

```text
Mining-only v0.01 -> deepen Mining -> add resource consumer -> add combat later
```

The documents are strong enough to create focused production tasks. They are not
yet strong enough to start the native runtime screen without one balance pass and
one asset/animation decision pass.

## What Works

- The old card-first design is cleanly removed from v0.01 instead of lingering
  as a hidden requirement.
- The Melvor deconstruction gives the right lesson: start with one skill and a
  clear reward/tool loop, not the current full Melvor feature matrix.
- The first 30 seconds and first 5 minutes are described in player-visible
  terms.
- The core loop has real verbs and states: choose node, complete tick, gain
  reward, check unlock, buy pickaxe, mine faster.
- `systems_foundation.md` gives future mechanics a dependency model, so Smithing,
  gear, combat, offline progress, and cards have a sane order.
- The fake shot points at a game screen, not a poster: avatar, progress, rewards,
  node list, upgrade, and event callout are all represented.
- The 3D avatar differentiator is product-relevant: "Melvor-like depth with a
  visible animated geared hero" is a stronger identity than a static panel clone.

## Findings

### P1 - First-upgrade economy is under-specified and can miss the target

`data/parameters.json` gives `upgrade_copper_pickaxe` a cost of `copper_ore 8`
and `coins 12`, but the early coin path is not locked:

- Surface Stone coins are `none_or_optional`.
- Copper Vein coins are `fixed_or_chance`.
- Geode coins are reliable only in expectation, not in a 5-minute first-session
  experience.

If Copper Vein gives a fixed `coins +1` per tick, the upgrade may arrive closer
to 60-90 seconds than the intended 3-5 minutes. If coins come mostly from Geodes,
the target can average out but becomes too RNG-dependent for a first upgrade.

Required action:

- Lock one deterministic coin model for v0.01, or make the first pickaxe
  ore-only and move coins to the second upgrade.
- Recompute the first 5-minute path after the decision.

Follow-up:

- T0002 now contains a locked draft for review: start on `Surface Stone`,
  unlock `Copper Vein` at Mining Lv2, grant fixed `coins +1` from Copper Vein,
  and price Copper Pickaxe at `stone 6` + `copper_ore 32` + `coins 32`.

### P1 - T0001 is too broad if animation R&D remains inside it

The first native screen already includes:

- Mining loop;
- UI layout;
- real asset path;
- node selection;
- reward log;
- pickaxe upgrade;
- fake-shot comparison;
- readability gate;
- 3D miner;
- animation proof.

Adding a skeletal/Mixamo-style pipeline to the same task would make the first
playable slice depend on engine/asset-pipeline R&D. The GDD correctly treats
skeletal animation as strategic, but task planning needs to enforce that split.

Required action:

- Keep v0.01 proof on modular mesh-part animation unless a separate spike proves
  skeletal animation first.
- Track the skeletal path as its own sidecar task with pass/fail evidence.

### P1 - Public-safe 3D miner source path is a blocking decision

The project wants a blocky miner but must avoid Minecraft/Steve-like public art.
The GDD names the options but does not choose the first source path:

- procedural/blockout GLB parts;
- licensed ready model;
- custom modelled/generated asset.

Required action:

- Choose the first source path before native implementation.
- Record provenance and style constraints before integrating runtime assets.

### P2 - Reference work is sufficient for direction, not for final UI/balance

The Melvor history packet is enough to justify the Mining-only start. It is not
enough to copy Melvor UI density, exact pacing, economy numbers, offline
expectations, or combat shape.

Required action:

- Treat the current reference docs as `direction-ready`.
- Add deeper reference deconstruction only when implementing UI density,
  offline progress, combat, or long-term skill interlocks.

### P2 - Fake shot is useful but has known runtime mismatches

The accepted fake shot has high resource counts, later navigation surfaces,
generated text, and timing values that do not match the data files.

Required action:

- Use it as mood/composition direction only.
- Build the native screen from `data/parameters.json` after T0002 locks balance.

## Recommended Task Split

1. Lock v0.01 parameters and first-session economy.
2. Choose the first public-safe 3D miner asset path.
3. Build the native Mining screen with modular 3D mesh-part animation.
4. Run a separate skeletal/Mixamo-style sidecar spike if the lead wants that
   pipeline before or alongside v0.01.

## Current Gate

Status: `partial`.

The base GDD is good enough for planning and task decomposition. It is not yet
implementation-ready because balance and 3D asset/animation source decisions are
still open.
