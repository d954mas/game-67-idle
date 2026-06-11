# Fantasy Pocket RPG Implementation Plan

## Goal

Build the first playable slice: a player starts on a province map, travels to old ruins, resolves one search/fight path, receives loot and a dragon omen, unlocks a safe camp, rests/crafts/talks, then returns to the map with `Hunter's Ford` unlocked.

## Source Files To Read First

1. `AGENTS.md`
2. `gamedesing/fantasy-pocket-rpg/handoff_status.md`
3. `gamedesing/fantasy-pocket-rpg/gdd.md`
4. `gamedesing/fantasy-pocket-rpg/data/balance.json`
5. `gamedesing/fantasy-pocket-rpg/data/ui_flow.json`
6. `gamedesing/fantasy-pocket-rpg/combat_spec.md`
7. `gamedesing/fantasy-pocket-rpg/data/combat.json`
8. `gamedesing/fantasy-pocket-rpg/data/asset_manifest.json`

## First Playable Slice Packet

- Player starts at: `province_map`.
- Screen shown first: province map with `Old Road` available and `Hunter's Ford` locked.
- Available actions: travel, search, fight/skill, take loot, enter camp, rest, craft, talk, leave camp.
- Currencies/stats: Health 30/30, Resolve 10/10, Supplies 3, Gold 12, Herbs 0, Dragon Omen 0/10.
- First activity: `travel_old_road`.
- First reward: Herbs +2 and Dragon Omen +1 from `search_standing_stones`.
- First enemy/obstacle: `Ruin Wolf`, 12 HP, 4 damage bite.
- Player combat actions: `Attack` deals 5 damage, `Defend` halves next enemy damage, `Use Draught` heals 10/15, `Calm The Beast` costs Resolve 2 with 60% success.
- Combat outcomes: victory grants `Rusty Blade`, 4 Gold, and location progress; forced retreat returns to safe camp at 5 Health.
- Recovery path: camp rest costs Supplies 1 and heals 12 Health.
- First upgrade: `trail_herbalist_1`, potion heal 10 -> 15.
- First visual/status change: camp button changes from locked/unsafe to available; `Hunter's Ford` unlocks after camp beat.
- Save/load expectation: for P0, reset/new game is enough; persistence can be added after reducer/UI proof.
- Out of scope: character creator, full inventory, shops, factions, dragon pet, base building, procedural world, monetization.

## Suggested Phases

1. Data/state schema: resources, stats, route unlocks, current screen, camp safe flag, inventory list, upgrade flags.
2. Core actions: travel, search, fight placeholder, take loot, enter camp, rest, craft upgrade, talk, leave camp.
3. UI screens: province map, encounter view, combat panel placeholder, loot result, safe camp, inventory/upgrade panel.
4. Visual wiring: use GDD backgrounds as temporary game backgrounds; implement readable overlay UI.
5. Automation: click through first path and capture desktop screenshot; add mobile/web capture if web target exists.

## Acceptance Gates

- Player can perform the full path without external docs.
- At least one resource/stat visibly changes after action.
- Camp availability changes from locked to available.
- Upgrade changes a visible number.
- Next map node unlocks.
- Screenshot proves nonblank readable UI.
- Emulated input proves the main action path.

## Commands

- Status: command discovery still required.
- Discover build/run conventions first: inspect repo scripts and `external/neotolis-engine` integration without editing the submodule.
- Default validation target: native desktop/PC harness.
- Web/mobile validation only when implementation target is web or UI is browser-based.

## Next-Chat Prompt

Use the project rules in `AGENTS.md`. Implement the first playable slice from:

1. `gamedesing/fantasy-pocket-rpg/handoff_status.md`
2. `gamedesing/fantasy-pocket-rpg/gdd.md`
3. `gamedesing/fantasy-pocket-rpg/data/balance.json`
4. `gamedesing/fantasy-pocket-rpg/data/ui_flow.json`
5. `gamedesing/fantasy-pocket-rpg/data/asset_manifest.json`

Goal: build the first playable compact fantasy RPG loop: province map -> ruins encounter -> reward/fight placeholder -> safe camp -> rest/craft/talk -> unlock next route.

Must implement: visible resources/stats, main click path, first Ruin Wolf combat contract from `data/combat.json`, blocked/safe camp state, first upgrade, next-node unlock, screenshot/input validation.

Out of scope: full RPG systems, final art pack, base building, engine submodule edits.

First action in that chat: discover and record exact build/run/test commands before coding.
