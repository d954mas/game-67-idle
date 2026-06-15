---
type: Reference Study
title: Fishing Reference Study
status: reference-ready-for-first-native-prototype-after-fake-shot-review
timestamp: 2026-06-15T00:00:00Z
---

# Fishing Reference Study

## Reference Lock

- Mode: central deconstruction for Roblox-like fishing loop/art direction;
  quick supporting checks for asset options and broader fishing depth.
- Reference question: what first-screen loop, UI hierarchy, reward feedback,
  progression fantasy, and visual grammar should drive a casual 3D fishing
  prototype?
- Durable doc path:
  `gamedesign/projects/roblox-fishing/references/fishing_reference_study.md`.
- Required source packet: official/store pages, screenshot/frame evidence,
  gameplay walkthrough with screenshot sequence, supporting economy/progression
  sources, and current native capture.
- Current native capture path or capture plan:
  `tmp/roblox_fishing/current_native_before_fishing.png`, captured on
  2026-06-15. Source inspection and screenshot show the current runtime is
  still a `Rune Marches` RPG/map/combat screen, not a fishing game.
- No-coding/no-final-art boundary: do not implement fishing gameplay, economy,
  primary UI, or final reference-driven art until this study is reviewed or the
  lead approves a narrow exception. Lead has approved moving to a first fake
  shot on 2026-06-15; this does not unlock final runtime art or code.
- Expected proof screenshot/scenario: native first-slice screenshot showing
  avatar + water + rod + bobber/catch feedback + primary action + fish reward.
- Unlock condition: lead reviews the fake shot direction and the next native
  implementation pass names the first screenshot proof.

## Definition Of Ready Checklist

- [x] mode matches implementation risk
- [x] source matrix is filled
- [x] evidence board has 6 cited player-facing frames/screenshots or approved gap
- [x] gameplay footage/walkthrough or long screenshot sequence exists, or gap is approved
- [x] current native capture exists or capture plan is explicit
- [x] observation ledger has at least 5 visible beats before conclusions
- [x] borrow / avoid / copy-risk are explicit
- [x] current-build mismatch is written
- [x] next native proof is named

Status: reference study is ready enough for the first native prototype after
fake-shot direction review. It is not a license to copy layouts, fish names, UI
frames, or Roblox/Fisch/Fish It assets.

## Sources Checked

| Source | Quality | Checked | Proves | Does not prove |
|---|---|---:|---|---|
| https://www.roblox.com/games/16732694052/Fisch | official/store | 2026-06-15 | Fisch premise, creator, fishing instructions, 400,000+ fish variations, cast/shake/reel-progress loop | detailed timing, exact first-session pacing |
| https://www.roblox.com/games/2866967438/Fishing-Simulator | official/store | 2026-06-15 | 10+ islands, boats, sea monsters, 1000+ fish, click/green-zone catch loop, coin/gem reward framing | exact UI timing and economy values |
| https://www.pcgamer.com/games/roblox/fisch-codes/ | secondary/current guide | 2026-06-15 | Fisch remains current/popular in June 2026; rewards include skins, submarine parts, bobbers | raw gameplay or unbiased design proof |
| https://gamertweak.com/how-to-fish-in-roblox-fisch/ | gameplay walkthrough with screenshots | 2026-06-15 | Fisch first fishing sequence: equip rod, pier, hold cast, bite `!`, shake prompts, hold/release reel bar, success with fish name/weight | economy pacing, original source quality |
| https://www.destructoid.com/fisch-money-farming-guide-best-locations/ | guide with screenshots | 2026-06-15 | Fisch money farming depends on locations, rods, baits, gear value, and currency farming | first-session tutorial timing |
| https://deltiasgaming.com/how-to-get-free-megalofriend-rod-skin-in-fish-it-roblox/ | guide with screenshots | 2026-06-15 | Fish It hub/island travel, dock NPC reward exchange, cosmetic rod skin status fantasy | first catch minigame |
| https://fandomwire.com/roblox-fish-it-rod-tier-list-december-2025/ | guide with shop/progression screenshots | 2026-06-15 | Fish It rod progression uses visible rod stats such as luck, speed, weight/capacity, prices, store locations | verified balancing accuracy for our game |
| https://store.steampowered.com/app/3146520/WEBFISHING/ | official/store | 2026-06-15 | cozy multiplayer fishing, nearly 100 creatures, gear upgrades, cosmetics, social toys, tags: casual/cute/3D/colorful/cozy | Roblox-style economy or first-minute UI |
| https://store.steampowered.com/app/766570/Russian_Fishing_4/ | official/store | 2026-06-15 | long-term depth: many species, rods/reels, reservoirs, shore/boat fishing, weather/time effects | casual Roblox tone; first prototype scope |
| https://kenney.nl/assets/nature-kit | asset source | 2026-06-15 | CC0 3D nature kit, 330 files | fish/rod gameplay assets |
| https://kenney.nl/assets/watercraft-kit | asset source | 2026-06-15 | CC0 3D boats/watercraft, 45 files | avatar/fish/rod assets |
| https://kenney.nl/assets/blocky-characters | asset source | 2026-06-15 | CC0 animated 3D blocky characters, 20 files | fishing-specific animation quality |
| https://kenney.nl/assets/fish-pack | asset source | 2026-06-15 | CC0 2D fish pack, 120 files for icons/cards/UI | 3D fish models |

## Source Ladder

- User-provided material: broad direction only: casual Roblox-like 3D fishing,
  bright/juicy visuals, generated UI/art skill test, research before questions.
  Updated 2026-06-15: casual audience; progression/grind good; complex gameplay
  bad; feel and fake shot important; clarity of goal/progression important;
  realism forbidden.
- Official/store/trailer visuals: Roblox pages for Fisch and Fishing
  Simulator; Steam pages for WEBFISHING and Russian Fishing 4.
- Raw gameplay evidence: Gamertweak walkthrough with multiple Fisch screenshots
  for equip/cast/bite/shake/minigame/catch sequence; supporting image-search
  frames for Fishing Simulator, Fish It hub/shop, and Fisch backpack/quest
  states.
- Supporting interpretation: PC Gamer current Fisch guide, Destructoid
  farming guide, Deltia Fish It cosmetic/island guide, FandomWire Fish It rod
  tier guide, Kenney asset pages.
- Source gaps / exceptions: no direct local video capture. For first prototype,
  the screenshot walkthrough plus current native mismatch is sufficient because
  we are adapting a simple one-input casual loop, not cloning exact timing.

## Observation Ledger

| Beat | Source/frame | Visible screen state | Player action | Visible response | Reward/UI feedback | Inferred meaning |
|---|---|---|---|---|---|---|
| 1 | Gamertweak Fisch guide, first rod/pier step | Player has a rod in hotbar and stands at pier/water | Equip rod and hold left mouse | Bobber is cast into water | Bite wait begins | First input should be obvious cast from dock |
| 2 | Gamertweak Fisch guide, bite step | Bobber is in water; bite is signaled by an exclamation mark | Wait for bite | Bite state begins | `!` prompt tells player when to act | Use a big bite pulse instead of subtle realism |
| 3 | Gamertweak Fisch guide, shake step | Shake circles appear on screen | Click shake prompts | Fish attraction accelerates; missed click can reset | Active waiting feedback | Prototype can simplify this to one juicy bite pulse |
| 4 | Gamertweak Fisch guide, minigame step | Bottom bar has movable white bar and fish marker/line | Hold/release mouse | White bar moves right while held, left when released | Progress fills toward catch | One-input hold/release reel meter is enough |
| 5 | Gamertweak Fisch guide, success step | Catch result appears after progress completes | Finish minigame | Fish is caught | Success message with fish name and weight | Catch reveal must show fish identity and weight/value |
| 6 | Fishing Simulator official description | Player casts into air/water | Click when bite appears | Green-zone keep-click phase starts | Progress bar full catches fish | Similar accessible reel grammar |
| 7 | Fishing Simulator official description | World has islands, boats, sea monsters, fish collection | Fish with friends and explore | Unlock new areas/targets | 1000+ species, gems/coins | Progress fantasy is travel + collection |
| 8 | WEBFISHING Steam page | Cozy 3D social fishing camp | Fish, sell/complete journal, upgrade/customize | Cash and rewards unlock gear/cosmetics | Journal, money, special rewards | Social/cozy loop can soften grind |

## Evidence Board

| Item | Source/timestamp/frame | What it proves | What it cannot prove |
|---|---|---|---|
| first screen | Gamertweak Fisch screenshot sequence, rod/pier frame | Avatar at pier/water with fishing rod and bottom hotbar | exact source game timing |
| first input | Gamertweak Fisch guide, cast step | Player holds mouse to cast bobber from pier | final UI/control layout for our game |
| visible response | Gamertweak Fisch guide, bite/shake/minigame frames | Bite `!`, shake circles, bottom reel bar with fish marker | exact values and timing |
| reward feedback | Gamertweak Fisch guide, success/catch frame | Catch completion shows fish name and weight | our fish naming or rarity design |
| upgrade/progression UI | FandomWire Fish It rod shop screenshots and stats | Rod shop cards communicate luck/speed/weight/capacity and coin prices | our exact upgrade values |
| hub/island status | Deltia Fish It Fisherman Island screenshot/article | Island hub, docks, merchant/NPC reward exchange, rod skin prestige | first-session navigation |
| economy grind | Destructoid Fisch guide screenshots/tables | Money farming depends on locations, rods, baits, and gear | exact rates for our economy |
| current build mismatch | `tmp/roblox_fishing/current_native_before_fishing.png` | Current native screen is Rune Marches RPG map/combat UI, not fishing | future prototype quality |

Evidence board status: sufficient for first native prototype translation.

## Systems Extraction

## Screen Grammar

- Camera/framing: third-person, slightly elevated, avatar visible beside water;
  for prototype use a fixed or gentle orbit camera.
- Primary play space: water edge with a highlighted cast zone and visible
  bobber/fishing line.
- Player click target: one dominant cast/reel action; minigame can use a large
  horizontal or vertical catch meter.
- Reward location: catch reveal near the center/bottom over the water, with
  coin burst and fish card/index update.
- Primary UI: coins, level/XP, backpack/fish count, current objective, primary
  cast/reel button.
- Secondary UI: fish index, shop/upgrades, quests, map/islands, settings.

## Mechanics / Balance Notes

- Core loop: cast -> bite -> reel skill check -> catch reveal -> sell/collect
  -> upgrade -> unlock spot.
- Currencies: coins as first currency; gems or pearls can stay out of the
  prototype unless needed for quest reward.
- Sources: fish sale value, quest completion, first-catch bonuses.
- Sinks: rod strength/control, bait luck, backpack capacity, boat/island unlock.
- First prototype targets: 5 fish, 3 rarities, 1 upgrade, 1 collection index,
  1 blocked upgrade state.

## Visual Composition

- Bright water should dominate the screenshot.
- Use blocky avatar proportions and chunky props, but keep our own shapes,
  colors, and names.
- Fish need exaggerated silhouettes and rarity color accents.
- UI should look like game objects: glossy panels, large icons, simple labels,
  no dense debug text.

## Borrow / Avoid / Copy-Risk

- Borrow:
  - hold/click reel meter with progress feedback;
  - fish rarity/weight/value reveal;
  - collection index and gear upgrades;
  - islands/boats as visible long-term fantasy;
  - cozy optional social/hangout tone.
- Avoid:
  - realistic tackle simulation in the first slice;
  - deep menu stacks before the first catch;
  - grind-only loop with no visual surprise;
  - adult MMO UI density;
  - subtle or realistic naturalistic visuals;
  - monetization/redeem-code pressure in prototype.
- Copy-risk:
  - Fisch/Fishing Simulator names, UI layout, fish mutations, exact rods,
    screenshots, and Roblox assets;
  - WEBFISHING characters/social identity;
  - using Roblox branding instead of "Roblox-like" blocky original style.

## Mismatch Audit Against Current Build

- Current screenshot: `tmp/roblox_fishing/current_native_before_fishing.png`
  captured 2026-06-15.
- Source inspection mismatch: `src/main.c` and screenshot are currently a
  `Rune Marches` native RPG screen with map/actions/combat telemetry, not a 3D
  fishing scene.
- Visual mismatch: dark fantasy UI, map landmarks, HP/mana/silver, combat
  enemy card, strike/spark/guard/rest buttons. It does not show avatar on dock,
  water, rod, bobber, fish, catch reward, coins/index/backpack, or upgrade goal.
- Required change: replace the current gameplay surface with a native fishing
  first slice after concept/GDD gate.

## Reference Digest

- Mode: central deconstruction for first native prototype.
- Sources checked: Roblox official pages for Fisch and Fishing Simulator,
  Gamertweak Fisch screenshot walkthrough, Destructoid Fisch farming guide,
  Deltia Fish It island/rod-skin guide, FandomWire Fish It rod progression
  guide, Steam pages for WEBFISHING and Russian Fishing 4, PC Gamer current
  Fisch guide, Kenney asset pages.
- Observed facts:
  - Fisch official copy describes hold-to-cast, bite wait/shake, hold/click
    reeling, and progress-bar catch completion.
  - Fishing Simulator official copy describes casting, bite click, repeated
    clicking to keep the line in a green zone, and progress-bar catch
    completion.
  - Fishing Simulator foregrounds 10+ islands, boats, sea monsters, and 1000+
    fish species.
  - WEBFISHING foregrounds cozy social fishing, journal completion, gear
    upgrades, cosmetics, and nearly 100 creatures.
  - Russian Fishing 4 shows the depth direction to avoid for prototype scope:
    many species, tackle types, reservoirs, weather/time behavior.
- Lead direction accepted: casual audience, simple gameplay, strong
  progression/grind, juicy bright non-realistic visuals, feel and fake shot as
  first proof.
- Current-build mismatch: current native capture is Rune Marches RPG, not 3D
  fishing. It lacks every required first-slice fishing proof element.
- Borrow: accessible reel minigame, catch reveal, collection/index, upgrades,
  islands/boats as status fantasy.
- Avoid: exact copied UI/assets/names, realistic sim depth, monetization
  pressure, dense menus.
- Copy-risk: high if we imitate Fisch/Fishing Simulator names, exact meter
  layout, fish mutations, or Roblox assets.
- Next native proof: first screenshot must show avatar, water, rod/bobber,
  dominant cast/reel action, catch reward, currency/index/backpack, and one
  upgrade affordance. It should visibly translate the accepted fake shot without
  copying exact reference UI/assets.
