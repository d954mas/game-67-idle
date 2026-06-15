---
type: Game Design Document
title: Splash Rods GDD
status: direction-accepted-for-first-fake-shot
timestamp: 2026-06-15T00:00:00Z
---

# Splash Rods GDD

## One-Line Pitch

A bright Roblox-like 3D fishing game where players catch chunky weird fish,
fill a colorful index, upgrade rods and bait, and unlock sunny islands one
short satisfying cast at a time.

## Audience

- Casual players who understand Roblox-like third-person spaces and simple
  click/hold minigames.
- Players who want collection, upgrades, surprise catches, and cozy status
  fantasy rather than serious fishing simulation.
- Lead direction: progression/grind is good; complicated gameplay and realism
  are bad.
- Prototype target: native PC first, with controls and UI that can later map to
  touch.

## Core Fantasy

The player is a cheerful island angler building a tiny fishing reputation:
stand on a dock, cast into sparkling water, fight a fish with a simple reel
meter, reveal a goofy trophy, earn coins, and make the next catch easier or
more valuable.

## Design Pillars

1. **Readable in 5 seconds.** The screen always shows where to fish, what button
   to press, what reward changed, and the next goal.
2. **Juicy catch reveals.** Fish are oversized, colorful, named, weighted, and
   ranked by rarity with splash/coin/shine feedback.
3. **Toy-world progression.** Rods, bait, boats, islands, index pages, and shop
   props are visible world/status objects, not only menu rows.

## Product No-Gos

- No realistic simulation-first tackle depth in the first slice or visual
  direction.
- No dark, muddy, survival, horror, or adult MMO tone.
- No exact Fisch/Fishing Simulator/Roblox names, UI layouts, assets, fish
  mutations, or screenshots.
- No monetization pressure, codes, premium currency, daily rewards, or ads in
  the prototype.
- No web prototype unless the lead explicitly approves web work.

## Core Verbs

- Move/look at dock area.
- Cast.
- Wait/attract with a simple juicy prompt.
- Reel.
- Catch.
- Sell/keep.
- Upgrade.
- Open fish index.
- Follow next goal.

## Core Loop

```text
intent: catch a fish
-> action: cast bobber into highlighted water
-> timer/check: bite timer, then hold/click reel meter
-> reward: fish card, coins, XP, index progress
-> visible change: coin burst, fish model/card, index count, goal update
-> unlock/choice: buy rod/bait/backpack upgrade or fish again
-> next intent: catch a rarer/heavier fish or afford the next area
```

## First 30 Seconds

1. Player spawns on a small dock facing bright water.
2. Top HUD shows `Coins 0`, `Level 1`, `Fish 0/5`, `Backpack 0/3`.
3. Objective chip says `Catch your first fish`.
4. Primary button says `Cast`.
5. Click cast: bobber arcs into a glowing fishing circle.
6. After about 2 seconds, bite pulse appears and the primary button changes to
   `Reel`.
7. Reel minigame appears: keep the fish marker inside a target zone until the
   catch bar fills.
8. Catch reveal appears: fish name, rarity, weight, value, and `Index +1`.
9. Coins increase; objective changes to `Catch 2 more or buy Better Line`.

## First 5 Minutes

- Catch 3-5 common/uncommon fish.
- Fill backpack once; sell all fish at the dock sign.
- Buy `Better Line I` after 30 coins.
- Upgrade increases reel control and unlocks `Blue Ripple` rare chance.
- Catch first rare fish or see a locked rare hint.
- See the next island/boat sign locked behind `80 coins + 3 index entries`.

## First Playable Slice

- Scene: one tropical dock, one water area, one shop/sell sign, one locked boat
  gate as aspiration.
- Fish: 5 species, 3 rarities.
- Economy: coins, XP/level, fish index count, backpack capacity.
- Actions: cast, reel, sell, buy one upgrade, open/close index.
- Upgrade: `Better Line I`.
- Blocked state: trying to buy `Better Line I` without coins; trying to fish
  with full backpack.
- Save/load: coins, XP, index caught flags, upgrade level, backpack contents.

## Full-Version Structure

## World

- **Starter Cove:** tutorial dock, common fish, first shop, sell stand.
- **Coral Pier:** brighter reef fish, bait tutorial, cosmetic pier props.
- **Mango Marsh:** slow/heavy fish, line-control upgrade pressure.
- **Moonlit Jetty:** night fish, glow effects, rare timing windows.
- **Storm Toy Atoll:** playful sea monster event, late-game boat status.

## Progression

- Rod line: control, tension forgiveness, cast distance.
- Bait line: bite speed, rarity chance, special fish lure chance.
- Backpack line: capacity, auto-sell threshold later.
- Boat/island line: unlocks new fishing spots and fish pools.
- Collection line: index milestones unlock cosmetics and island props.

## Fish Content Model

Each fish has:

- id, display name, silhouette family, rarity, habitat, base value;
- min/max weight;
- reel difficulty;
- first-catch index reward;
- visual effect tags such as splash, shine, wobble, glow.

Prototype fish:

| ID | Name | Rarity | Value | Weight | Difficulty | Visual role |
|---|---|---:|---:|---:|---:|---|
| bubble_guppy | Bubble Guppy | common | 8 | 0.2-0.8 kg | 1 | tiny readable starter |
| mango_minnow | Mango Minnow | common | 10 | 0.3-1.0 kg | 1 | warm color contrast |
| stripe_snapper | Stripe Snapper | uncommon | 18 | 1.0-2.5 kg | 2 | clear side stripes |
| jellybean_koi | Jellybean Koi | uncommon | 24 | 0.8-2.2 kg | 2 | playful rounded rare-ish |
| crown_catfish | Crown Catfish | rare | 55 | 3.0-6.5 kg | 3 | first trophy silhouette |

## Economy

- `coins`: main soft currency, earned by selling fish and first-catch bonuses.
- `xp`: earned per catch; levels are status and unlock pacing, not power in
  the prototype.
- `index_count`: unique fish species caught; unlocks island/boat gates.
- `backpack_slots`: friction gate that teaches selling.

First-slice balance target:

- First fish in under 15 seconds.
- First sell in under 90 seconds.
- First upgrade after about 3-4 catches.
- First blocked state visible by 2-3 minutes if the player does not sell or
  lacks coins.

## Reel Minigame

- Player holds/clicks `Reel`.
- Catch bar fills while the moving fish marker overlaps the target zone.
- Catch bar drains slowly when outside the zone.
- Rod control widens target zone.
- Fish difficulty controls marker speed and drain pressure.
- Prototype has no fail loss; if tension drops to zero, fish escapes and
  player can immediately cast again.
- Complexity ceiling: one meter and one input. No lure physics, no line knots,
  no realistic fish fighting, no separate rod/reel/bait simulation in the first
  prototype.

## UI Flow

1. Main HUD: coins, level, backpack, index.
2. Context HUD: cast/reel button, bite prompt, reel meter.
3. Result modal: fish card, rarity, weight, value, keep/sell action.
4. Shop panel: one upgrade card, locked next upgrade teaser.
5. Index panel: 5 fish slots with caught/unknown states.
6. Blocked toast: missing coins or backpack full with clear next action.

## Visual Direction

- 3D low-poly/blocky toy world, not realistic.
- Camera: third-person, slightly elevated, dock and water visible together.
- Water: saturated cyan with sparkle/ripple FX and readable cast circle.
- UI: seafoam/white panels, gold coin icon, rarity colors, rounded but not
  mushy; runtime text composed over blank generated UI bases.
- Fish: chunky, exaggerated silhouettes, no tiny realistic species.
- Visual intensity: bright, juicy, pleasant, noticeable. Avoid subtle realism,
  muted nature palettes, and simulator-like tackle presentation.

## Runtime Asset Strategy

- Use external CC0 assets where efficient: Kenney Nature Kit, Watercraft Kit,
  Blocky Characters, Fish Pack for UI cards/icons.
- Generate first fake shot for visual target.
- Generate reusable source families later: blank UI kit sheet, isolated icon
  sheet, decor overlay sheet, fish/catch card sheet, sprite/FX sheet.
- Keep generated text out of source assets.
- Treat all fake shots as visual targets, not runtime assets.

## Risks

| Risk | Type | Why it matters | Smallest test |
|---|---|---|---|
| Fishing feel is too passive | fun | Waiting without agency gets boring fast | First 30-second native loop with bite/reel timing |
| Visuals look like programmer art | visual | User explicitly requested juicy polished visuals | First fake shot + native screenshot product gate |
| UI kit becomes a flat mockup | production | Generated UI must be reusable | Art job with separate source families and validation |
| Reference copy risk | legal/brand | Roblox/Fisch-like can become too literal | Borrow/avoid/copy-risk review before final art |
| Scope creep into full sim | production | Realistic fishing systems would delay prototype | Keep first slice to 5 fish, 1 upgrade, 1 spot |

## Open Decisions

- Final title.
- Whether social/multiplayer is only tone or a future feature.
- Whether first prototype includes visible avatar animation or simplified posed
  avatar.
- Whether fish are true 3D models immediately or 2D catch-card art plus simple
  3D water silhouettes for the first native proof.

## Acceptance For Prototype

- Player can complete cast -> bite -> reel -> catch -> reward -> sell/upgrade.
- Screen communicates action and reward without external explanation.
- Visual proof looks like a bright game, not a debug UI.
- Data contracts define fish, economy, UI flow, and assets.
- Native screenshot/input proof exists before claiming playable completion.
