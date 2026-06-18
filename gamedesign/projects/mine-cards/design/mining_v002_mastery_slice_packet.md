# Mining v0.02 Mastery Slice Packet

Date: 2026-06-18
Task: `T0012`
Status: gated prep; do not implement until T0001 is accepted or the lead
explicitly prioritizes mechanics before visual baseline acceptance.

## Definition Of Done

The next gameplay slice is ready to implement when this packet defines one
small mastery tier-up loop that:

- uses existing Mining nodes and existing mastery parameters;
- produces one visible tier-up moment;
- changes a real Mining number;
- is testable in native runtime with a short scenario and product gate;
- does not add Smithing, equipment stats, combat, offline progress, or
  Melvor-scale breadth.

Machine-readable implementation contract:

`gamedesign/projects/mine-cards/data/mining_v002_mastery_contract.json`

Contract validation:

```powershell
py -3.12 tools/design/validate_mastery_slice_contract.py --contract gamedesign/projects/mine-cards/data/mining_v002_mastery_contract.json --parameters gamedesign/projects/mine-cards/data/parameters.json
```

## Why This Slice

T0001 proves:

```text
select node -> mine tick -> reward -> unlock/upgrade goal
```

The next gameplay step should add a reason to keep repeating a node without
opening a new screen or system. Node Mastery is already present in the v0.01
design and parameters, but T0001 treats it mostly as visible progression. v0.02
should make the first mastery tier-up feel like a game event.

## Core Loop Chain

```text
intent -> keep mining one node -> mastery XP check -> tier-up ->
visible interval bonus -> faster repeat -> next node/upgrade intent
```

| Step | Player input | Rule/check | Output state | Feedback | Next decision |
|---|---|---|---|---|---|
| Keep mining | Continue selected node | completed tick grants mastery XP | `node_mastery_xp += 1` | mastery bar fills | continue or switch |
| Tier-up check | automatic | mastery XP reaches tier threshold | `node_mastery_tier = 1` | tier-up callout/effect | compare speed |
| Apply bonus | automatic | active node has tier 1 | interval multiplier for that node | before/after interval text | repeat faster or switch |
| Re-read goal | inspect node/upgrade | node now improved | node row shows tier chip | tier badge + progress bar | aim for Copper Pickaxe/deeper node |

## First Slice Scope

Use existing parameter values:

- Tier 1 threshold: `10` mastery XP.
- Tier 1 effect: `node_interval_multiplier_delta = -0.03`.
- Applies per node only.
- No new currency.
- No new resource.
- No new tab.

Recommended proof node:

`surface_stone`

Reason:

- first reward cadence is fast;
- tier-up can be reached in a short accelerated DevAPI scenario;
- it does not disturb Copper Pickaxe economy.

Optional second proof:

`copper_vein`

Use it only if the same implementation naturally supports both nodes without
extra UI complexity.

## Runtime State Requirements

Guard:

The current root `state/game_state.schema.json` is still the reusable seed
schema. Before implementing this slice, the implementer must choose and record
whether Mine Cards mastery becomes schema-backed persistent state or remains
game-local runtime state for the narrow v0.02 proof. Do not mutate raw state
directly from UI/gameplay code; use a domain action or equivalent gameplay
operation.

Minimum per-node state:

```json
{
  "node_mastery": {
    "surface_stone": {
      "xp": 0,
      "tier": 0,
      "last_tier_up_at": null
    }
  }
}
```

Rules:

- mastery XP increments after a completed Mining tick;
- tier-up is resolved after mastery XP increment and before UI refresh;
- tier-up event is added to reward/log feedback once;
- tier effect applies to the next interval after tier-up, not the already
  completed cycle;
- existing Copper Pickaxe multiplier stacks after node mastery multiplier;
- if state is accelerated for testing, the UI must still show the same
  player-facing transition.

## UI Requirements

Node row:

- mastery tier chip: `M0`, `M1`, etc.;
- mastery progress bar or compact `8/10` text;
- interval label reflects mastery effect after tier-up.

Tier-up feedback:

- non-blocking stage or node-row callout;
- no modal;
- must not cover progress bar or next goal panel;
- reward log row can say `Surface Stone Mastery I`.

Before/after:

- show current interval and a small delta such as `3.0s -> 2.91s`;
- if the delta is too small to read, show `+3% faster` instead.

## Live-State Coverage

Add required states when implementation starts:

- `mastery_near_tier`: node has 8-9/10 mastery XP;
- `mastery_tier_up`: tick triggers tier 1 and callout/log;
- `mastery_post_tier`: same node shows tier chip and faster interval;
- `mastery_other_node_unaffected`: optional if both nodes are shown.

## Validation Plan

Native proof:

- build `game_seed`;
- run a DevAPI scenario that seeds or advances mastery to near tier;
- capture before/after screenshots;
- capture short motion/progress proof if the interval change is visible enough;
- run product gate with live-state matrix coverage;
- run UI readability zoom montage for the affected node row.

Contract validation:

- `py -3.12 tools/design/validate_mastery_slice_contract.py --contract gamedesign/projects/mine-cards/data/mining_v002_mastery_contract.json --parameters gamedesign/projects/mine-cards/data/parameters.json`

Acceptance questions:

- Can a new player tell that repeating Surface Stone produced a milestone?
- Can they see what changed numerically?
- Does the mastery feedback support the existing Copper Pickaxe goal instead
  of distracting from it?
- Does the first screen still read as Mining, not a spreadsheet?

## Rejection Axes

If the slice is rejected, record one axis:

- bonus too subtle to feel;
- UI too cluttered;
- tier-up feedback competes with geode/upgrade feedback;
- state/rules are unclear;
- it delays the more important custom character art pass.

## Deferred

- Mastery tier 2 geode bonus.
- Mastery tier 3 extra resource chance.
- Mastery persistence/offline interaction.
- Mastery achievements.
- Mastery affecting equipment or Smithing.
