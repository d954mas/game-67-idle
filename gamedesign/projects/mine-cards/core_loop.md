# Mine Cards Core Loop

Status: `base spec / v0.01`.

This file defines the first playable loop in implementable terms. It avoids
future mechanics except where they explain why the base exists.

## Loop Summary

```text
intent -> choose node -> mine tick -> reward -> progress/unlock check ->
upgrade decision -> stronger mining -> next node intent
```

## Primary Action

Action:

`select_or_continue_mining_node`

Player intent:

Start or continue the best available Mining node.

System behavior:

- if no node is selected, select the default starter node;
- if a node is selected, its progress timer advances;
- when progress reaches interval, resolve one Mining tick;
- after tick resolution, progress restarts automatically on the same node unless
  the player switches nodes.

Feedback:

- selected node highlight;
- miner animation;
- progress bar;
- timer label;
- reward log row;
- resource counter change.

Blocked states:

- locked node: show requirement;
- unaffordable upgrade: show missing resource;
- invalid node switch: keep current node and show why.

## Loop Step Contract

| Step | Player input | Rule/check | Output state | Feedback | Next decision |
|---|---|---|---|---|---|
| Choose node | Tap node or accept default | Node unlocked? | `selected_node_id` | Node highlight, yield preview | Wait, switch, inspect upgrade |
| Mine tick | Wait while active | `elapsed >= interval` | Resource/XP/mastery delta | Bar completes, reward log | Continue, switch, upgrade |
| Rare event | Optional tap or auto | Chance on tick | Bonus coins/XP | Geode callout | Continue mining |
| Unlock check | Automatic | Level/tool/resource requirement | Node/tool becomes available | Unlock callout | Try new node/upgrade |
| Upgrade | Tap upgrade | Cost affordable? | Tool owned, modifiers update | Before/after speed, spend animation | Mine faster |

## Timing Targets

| Moment | Target |
|---|---:|
| First visible progress | immediate |
| First reward | 3 seconds |
| First new decision | 15-30 seconds |
| First rare-event chance seen | within first 2 minutes as a visible chance, not guaranteed |
| First upgrade target reached | 3-5 minutes |
| First deeper-node preview understood | first 60 seconds |

## First Session Walkthrough

### Beat 1: Start Mining

- UI starts on `Surface Stone`.
- Player sees miner, node, progress bar, and reward preview.
- Progress completes quickly.
- Reward log shows Stone and XP.

Pass:

- a new player can say "I am mining stone".

### Beat 2: Better Node

- `Copper Vein` is visible as a near-available node.
- Player switches to it after reaching Mining Lv2.
- Copper Ore appears in reward log and HUD.

Pass:

- player understands Copper Ore is more valuable than Stone.

### Beat 3: First Upgrade

- Copper Pickaxe panel shows required Copper Ore and Coins.
- As resources accumulate, the upgrade becomes affordable.
- Purchase reduces Mining interval.

Pass:

- player sees a before/after change and progress feels faster.

## Activity Template

## Activity: Mining

- Unlock condition: start.
- Input: choose a mining node.
- Duration/cooldown: node interval modified by pickaxe and mastery.
- Cost: none in v0.01.
- Consequence/reward: resources, Mining XP, node Mastery XP, rare event chance.
- Stat effects: Mining Level, node Mastery, Mining Interval.
- Failure/blocked state: locked node or unaffordable upgrade; no death/failure.
- Visual feedback: miner animation, progress bar, reward log, node highlight.
- Progression affected by: pickaxe tier, mastery tier, Mining level.
- UI location: Mining Activity screen.

## Progression: Mining Level

- Unlock condition: earn Mining XP.
- Cost/risk/requirement: none.
- Effect: unlocks or previews nodes/tools.
- Why player wants it: deeper nodes and better resources.
- Visible before/after: level label, unlock callout.
- Max level: v0.01 needs only levels 1-5 even if long-term cap is higher.
- Affected activity: Mining.

## Progression: Node Mastery

- Unlock condition: mine the same node repeatedly.
- Cost/risk/requirement: time spent on that node.
- Effect: small speed/yield/geode-chance benefits by tier.
- Why player wants it: repeated Mining feels productive even before new nodes.
- Visible before/after: mastery bar/tier chip on node row.
- Max level: v0.01 uses 3 simple tiers.
- Affected activity: Mining nodes.

## Upgrade: Copper Pickaxe

- Unlock condition: Copper Vein discovered or Copper Ore gained.
- Cost/risk/requirement: Copper Ore and Coins.
- Effect: faster Mining interval.
- Why player wants it: all future Mining ticks complete faster.
- Visible before/after: interval number and progress speed.
- Max level or scaling: one upgrade in v0.01.
- Affected activity: all Mining nodes.

## Reward Resolution Order

On completed Mining tick:

1. Grant node resource.
2. Grant Mining XP.
3. Grant node Mastery XP.
4. Check level-up.
5. Check mastery tier-up.
6. Check rare event.
7. Check unlocks.
8. Append reward log row(s).
9. Refresh affordability states.

Reason:

- resource counters and unlocks should never lag one tick behind the visible
  reward log.

## First-Session Timing Draft

Current v0.01 lock:

- start on `Surface Stone`;
- `Surface Stone` grants `mining_xp +2` every 3 seconds;
- `Copper Vein` unlocks at Mining Lv2 / 12 total XP, after about 18 seconds;
- `Copper Vein` grants `copper_ore +1`, `coins +1`, and `mining_xp +4` every 5
  seconds;
- `Copper Pickaxe` costs `stone 6`, `copper_ore 32`, and `coins 32`.

Expected no-Geode path:

```text
18s to Copper Vein + 32 Copper ticks * 5s = about 178s to first pickaxe
```

Geodes can shorten the coin side, but Copper Ore remains the deterministic
gating resource. The `stone 6` cost uses the starter Stone naturally earned on
the path to Mining Lv2.

## Re-Entry

When player returns to the screen in v0.01:

- selected node is restored;
- resource/XP/mastery state is restored;
- progress can either resume from saved elapsed time or restart at 0, depending
  on implementation simplicity;
- no offline simulation is required.

If progress restarts, the UI should not claim offline earnings.

## Confusion Guards

- Never show more than one primary CTA in v0.01.
- Locked node labels must include the requirement.
- Upgrade panel must show missing cost.
- Reward log must use icons plus readable text.
- Geode event must not cover the progress bar.
- Tabs for deferred systems should be hidden or visibly disabled.

## Done Criteria For The Loop

The first loop is working when:

- the player receives the first reward in about 3 seconds;
- the player can switch or preview Copper Vein;
- the player can earn and spend toward Copper Pickaxe;
- the upgrade changes a visible number and actual timing;
- the player understands one next goal without reading a long tutorial.
