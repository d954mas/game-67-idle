# Voxelheim GDD - Frost Keep Rebuilder

Status: **rescue direction** (2026-06-17). The previous generic idle GDD is now
legacy evidence, not the product target. See:

- prototype teardown: `reviews/prototype_deconstruction_2026-06-17.md`
- competitor packet: `references/competitor_deconstruction_2026-06-17.md`
- UI/UX rescue spec: `visual/ui_ux_rescue_spec.md`
- loop contract: `data/rescue_loop.json`

## Hook

**Your blocky hero raids snowy monsters to recover magic blocks, then you
rebuild the ruined Frost Keep room by room; every expedition makes the hero
stronger and every repair visibly changes the toy-block diorama.**

## Product Thesis

The old hook, "auto-battle for gold and upgrades," is too generic. Voxelheim's
own differentiator is the Frost Keep. The keep must become the player's visible
project, not background art.

## Audience / Platform

- Audience: casual RPG/incremental players who like visible collection,
  cozy-toy fantasy, and short decision loops.
- Platform: native PC first. UI is designed mobile-readable but no web/mobile
  build is in scope unless explicitly requested.
- Session: 30-90 second check-ins, 5-15 minute active sessions, offline return
  rewards later.

## Pillars

1. **One obvious next click.** At any moment the player can tell whether to
   claim loot, choose a card, repair a room, train the hero, or fight the boss.
2. **Progress you can see.** Repairing rooms, lighting the forge, adding a
   campfire, changing gear, and adding helpers are more important than raw DPS
   text.
3. **Idle with choices.** Auto-combat runs by itself, but short packets create
   1-of-3 loot/rune decisions so the player shapes the run.
4. **Cozy blocky identity.** The game should feel like a frosted toy diorama,
   not a black RPG HUD over a generic fantasy scene.

## Core Loop

```text
watch auto-combat
  -> earn Gold + Frost Blocks
  -> choose 1 of 3 loot/rune cards
  -> spend Gold on hero training OR Frost Blocks on keep rooms
  -> visible hero/keep improvement
  -> beat a stronger boss
  -> unlock rooms, cards, companions, and eventual Avalanche Reset
```

## First 30 Seconds

1. The hero auto-fights one large icy enemy beside a broken keep gate.
2. The enemy dies and drops Gold plus Frost Blocks; rewards fly to the top bar.
3. `Repair Gate` becomes the single dominant button.
4. Player repairs the Gate; the gate visibly completes and unlocks the first
   3-card loot choice.

## First 5 Minutes

- Repair Gate, then Forge, then Campfire.
- Choose at least 3 loot/rune cards.
- Beat the first boss packet.
- See one companion/helper appear after Campfire.
- Learn the loop without more than 3 tutorial beats.

## Currencies

- **Gold**: run soft currency. Source: kills and boss chests. Sink: hero
  training. Resets on Avalanche Reset.
- **Frost Blocks**: visible rebuild currency. Source: monster drops, loot cards,
  boss blueprints. Sink: repair keep rooms. First-slice progression currency.
- **Frost Shards**: later prestige/meta currency. Source: Avalanche Reset based
  on Keep Rank/highest boss. Sink: permanent blueprint bonuses. Hidden from the
  first 30 seconds unless already earned.

## Systems

### Auto-Combat

- One large active enemy at a time.
- Short combat packets target about 20 seconds for the first slice.
- Rewards appear near the enemy first, then fly to counters.
- Boss packets create a visible timer/check, but failing retries without loss.

### Loot / Rune Choices

- After a combat packet, show 3 cards and pick 1.
- First card pool:
  - Sharp Edges: more damage this run.
  - Block Cache: gain Frost Blocks now.
  - Quick Hands: faster attacks this run.
- Cards must have large icon, short label, short effect, and visible rarity/state.

### Keep Repair

First-slice rooms:

1. Gate - costs 5 Frost Blocks; unlocks loot choices.
2. Forge - costs 15 Frost Blocks; unlocks stronger weapon cards and hero
   training.
3. Campfire - costs 30 Frost Blocks; unlocks first helper/companion.

Each room must visibly change in the scene or keep panel when repaired.

### Hero Training

Keep the first slice to three readable run stats:

- Power: kill faster.
- Speed: attack faster.
- Loot: more Gold/Blocks.

Do not show seven upgrades on the first screen. Depth unlocks through rooms and
cards, not by exposing every stat at once.

### Prestige

Prestige becomes **Avalanche Reset**:

- unlocks after repairing Gate, Forge, and Campfire to reach Keep Rank 3;
- grants Frost Shards from Keep Rank plus a smaller highest-stage bonus;
- resets run Gold, Frost Blocks, repaired rooms, temporary cards, and stage
  progress;
- preserves Frost Shards, shard upgrades, and highest-stage history;
- next run starts with the rebuilt Keep buried again and persistent meta
  currency visible.

### Frost Blueprints

After the first Avalanche Reset, Frost Shards become spendable in a compact
**Frost Blueprints** panel:

- Sharper Steel: permanent damage +10% per level.
- Rich Veins: permanent Gold +10% per level.
- Head Start: start future expeditions one stage later per level.
- Camp Supplies: improves offline return by +5% per level.

The panel must answer, on-screen, why the next expedition is stronger. It is not
a hidden debug/meta list.

### Offline Return

Offline return unlocks after the first Avalanche Reset, once the player has seen
the run reset and Frost Shards. While away, the camp recovers:

- Gold, for hero training after Forge.
- Frost Blocks, so the player can immediately repair the Keep again.

The return popup must be a clear reward moment: time away, Gold, Frost Blocks,
Camp Supplies multiplier, and one `Collect` action.

## UI/UX Direction

Source of truth: `visual/ui_ux_rescue_spec.md`.

First-slice hierarchy:

1. primary next action;
2. combat result;
3. visible keep repair;
4. Gold / Frost Blocks / Keep Rank;
5. compact secondary training;
6. locked previews with one unlock reason.

Remove the minimap from the first slice. Replace black plates with frosted
readable panels. Put every label on a calm plate and validate with the zoom
readability tool before calling UI done.

## Visual Direction

Keep the bright blocky snow identity, but re-aim it:

- old target: "Bright Roblox Adventure";
- new target: **cozy frosted toy diorama with readable game UI**.

The keep, repaired rooms, loot cards, helper, and reward effects are now core
visual assets. The existing hero/enemy/keep can be reused as placeholders only
if they pass the new screenshot gate.

## First Playable Slice

One native screen:

- central auto-combat against one large enemy;
- top resource bar: Gold, Frost Blocks, Keep Rank;
- visible keep repair panel with Gate, Forge, Campfire;
- one dominant next action;
- 3-card loot/rune choice after the first combat packet;
- compact hero training after Forge;
- no minimap, no old four-equal-button bottom shop, no exposed late-game shards
  before they matter.

## No-Go

- No generic "just add more upgrades" patch.
- No web/mobile prototype detour.
- No monetization, ads, passes, clans, or server competition in the first slice.
- No unreadable icon-only controls.
- No more than 3 tutorial beats.
- No final art generation from a named competitor until the reference packet is
  upgraded with raw gameplay frames if the art/layout depends on that competitor.

## Validation

- Native repair-chain screenshot: `build/captures/rescue_campfire_helper.png`.
- Native card-choice UI rescue screenshot:
  `build/captures/ui_rescue_card_choice.png`.
- Native offline return screenshot: `build/captures/offline_return_popup.png`.
- Latest offline return UI rescue screenshot:
  `build/captures/ui_rescue_offline_popup.png`.
- Latest Frost Blueprints layout screenshot:
  `build/captures/ui_rescue_blueprints_layout.png`.
- Card-choice readability zoom:
  `py -3.12 tools/devapi/ui_readability.py build/captures/ui_rescue_card_choice.png --region "hud=0.00,0.00,1.00,1.00"`
- Frost Blueprints readability zoom:
  `py -3.12 tools/devapi/ui_readability.py build/captures/ui_rescue_blueprints_layout.png --region "right_meta=0.68,0.00,1.00,0.86"`
- Readability zoom:
  `py -3.12 tools/devapi/ui_readability.py build/captures/offline_return_popup.png --region "offline_popup=0.20,0.28,0.80,0.74"`
- Product read:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_card_choice_ui_rescue.md`.
- Product read:
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_blueprints_layout.md`.
- Product read: a new player can answer, from a still screenshot, what is
  fighting, what was earned, what to press next, and what will visibly change.

## Implementation Note

`src/voxelheim_main.c`, `state/game_state.schema.json`, `data/balance.json`, and
`data/rescue_loop.json` now implement the first repair chain, first Avalanche
Reset, first Frost Blueprint spend, and first offline return popup. Remaining
implementation work should focus on stronger feedback/audio, polished room art,
and a broader fun/retention critic pass before broadening content.
