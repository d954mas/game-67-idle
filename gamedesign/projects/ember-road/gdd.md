# Ember Road GDD

## One-Line Concept

A beautiful fantasy hero RPG with map travel, town hubs, quests, items, grinding, leveling, and automated tactical battles.

## Audience

Players who want classic fantasy RPG progression, visible hero growth, quests,
items, and grinding without manual tactical combat. Progression should be
clear; controls and moment-to-moment play should stay simple.

## Pillars

1. Hero growth is always visible: level, gear, quest progress, and map access
   must change on screen.
2. The world feels explorable: towns and map nodes are real destinations, not a
   menu list.
3. Combat is automatic but preparation matters: equipment, level, supplies, and
   quest choice decide whether the hero wins, struggles, retreats, or needs to
   grind.

## No-Go List

- Do not copy `Legend: Legacy of the Dragons` IP, names, exact layouts,
  character designs, screenshots, monetization, or protected assets.
- Do not make the first slice a passive idle game or away-time reward loop.
- Do not expand to many towns, classes, crafting, pets, PvP, guilds, or full
  economy before one quest/combat/loot loop is playable and readable.
- Do not accept placeholder programmer UI as product visual proof.

## Core Loop

1. Read the town/map state and choose the next goal: quest, travel, grind,
   equip, heal, or claim reward.
2. Execute the chosen action: accept quest, move to a node, start encounter,
   equip loot, or return to NPC.
3. Resolve rules: map lock, quest requirement, inventory/equipment check, and
   automated combat.
4. See visible change: HP/XP/gold/items, quest step, map node status,
   equipment comparison, level progress, battle result animation.
5. Choose the next intent: push deeper, return to town, farm one more fight,
   upgrade gear, or follow the next quest marker.

## First Playable Slice: Town Gate Hunt

The first slice proves one complete RPG loop, not the whole RPG.

- Start location: `Old Gate` town square.
- First screen: hero portrait, town background, quest NPC, mini-map strip, hero
  stats, gold, XP bar, inventory button, primary quest action.
- Quest: `Wolves at the North Road`.
- First map node: `North Road`.
- First enemy: `Road Wolf`.
- First reward: XP, gold, one item drop, quest progress.
- First upgrade decision: equip `Rusty Iron Ring` for +1 attack or keep current
  gear.
- First return: claim town reward after one victory.
- First lock: `Old Mine` visible but locked until hero level 2 or quest
  complete.

## Player Verbs

- `accept_quest`: choose a quest from the town NPC.
- `travel`: move between town and one unlocked map node.
- `start_auto_battle`: commit the hero to the encounter at the selected node.
- `equip_item`: compare and equip one reward item.
- `claim_reward`: return to town and finish the quest step.

## Automated Combat

First combat is deterministic enough for validation but expressive enough to
feel like an RPG battle.

- Hero baseline: 30 HP, 5 attack, 1 defense, 0 speed bonus.
- Road Wolf: 18 HP, 4 attack, 0 defense.
- Each round: hero attacks, enemy counterattacks if alive.
- Damage formula for first slice: `max(1, attacker_attack - defender_defense)`.
- Expected normal win: 4 hero attacks, hero ends near 18 HP.
- Low-health state: if hero HP would fall below 8, show warning and recommend
  return/heal after the fight.
- Loss state for later tuning: hero retreats to town at 1 HP, keeps quest
  progress only if enemy was defeated.

## First Playable Slice

- One native PC flow with town, one map node, one automated battle, and one
  reward return.
- One clear primary path: accept quest -> travel -> auto battle -> loot -> claim.
- One feedback moment per step: quest marker, travel movement, combat animation,
  XP/gold/item flyout, level/quest progress.
- One visual proof screenshot for product-read review.
- One filled `reviews/first_slice_visual_gate.md` before broad runtime work.
- One filled `data/core_loop.json` with player verbs, rules, feedback, risk,
  goals, replay reason, and reference grounding. Do not assume hands-off
  progression, away-time rewards, or reset-meta loops unless the lead
  explicitly chooses that direction.
- One project-specific `visual/live_state_acceptance_matrix.json` that names
  required UI/player-read states before broad visual acceptance.
- One visual-first session contract: goal, non-goal, proof, stop condition,
  likely files.
- One screenshot-vs-target mismatch list before runtime visual code and after
  meaningful render changes.
- If the slice depends on beauty, casual readability, generated UI, or a fake
  shot match, one strict visual product gate using `--visual-strict`.
- Optional critic packet from `tools/product_gate/visual_critique_packet.mjs`
  before the strict gate verdict.

## Art Direction Stub

Beautiful fantasy UI with ornate panels, readable serif-like title treatment,
gold/emerald/ruby accents, parchment/map surfaces, strong item rarity colors,
and animated magical feedback. Backgrounds should show actual fantasy places:
town gate, road, forest edge, later city interiors. The first screen must look
like a game immediately: hero, location, quest, map path, stats, and reward
promise visible in the first viewport.

## Visual Proof Requirement

Before runtime visual implementation, create or select a target fake shot that
shows:

- town square with hero/status panel and quest NPC;
- map node path to `North Road`;
- ornate fantasy quest panel and reward preview;
- auto-battle result or battle overlay state;
- inventory/equipment comparison with one loot item.

Strict product gate must use `reviews/first_slice_visual_gate.md` and
`visual/live_state_acceptance_matrix.json`.

## Open Questions For Lead

- Should the hero be a named single protagonist, a created avatar, or a class
  archetype?
- Should the first fantasy tone be noble/high-fantasy, darker gothic, or
  colorful fairy-tale?
- Should autobattle be fully automatic or allow pre-battle skill/loadout choices
  before the fight starts?
