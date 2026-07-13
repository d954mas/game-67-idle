---
type: Game Design Document
title: Runway Awakening Poki MVP GDD
description: Implementation contract for the bright three-Essence 500-player MVP.
tags: [gdd, mvp, poki, runway-awakening]
game_id: web-dressup
status: accepted
date: 2026-07-11
---

# Runway Awakening - Poki MVP

## Definition of Done

The MVP is ready for the 500-player test when:

- first equip is available without instruction in <=5 seconds;
- first awakening is reachable in <=60 seconds;
- one round lasts 55-90 seconds;
- six discoveries plus two remixes provide 8-12 minutes of content;
- all six awakenings are identifiable by silhouette without their labels;
- portrait and landscape are fully playable with touch and mouse;
- initial network payload is <=6,500,000 bytes;
- web audio, Poki lifecycle and approved Game Events work in release;
- release contains no DevAPI/debug surface, unlicensed asset, or legacy art.

## Player verbs

1. Select a category and equip an item.
2. Choose a main outfit and a magic accent.
3. Press `AWAKEN` and watch the runway reveal.
4. Save the result card or restyle for another recipe.

## Deterministic recipe contract

- `main_focus` (dress or top) sets the primary Essence.
- `accent_focus` sets the secondary Essence.
- The recipe key is unordered: Moon+Bloom equals Bloom+Moon.
- Hair, bottom and shoes affect palette, trail, framing and mood only.
- A dress suppresses an incompatible bottom visually and logically.
- Missing focus pieces keep `AWAKEN` blocked with a visual slot hint.
- There is no score, RNG rank or failure result.

## MVP recipes

| Pair | Working title | Signature silhouette and hero moment |
|---|---|---|
| Moon + Moon | Lunar Oracle | Asymmetric crescent halo and descending moonbeam |
| Bloom + Bloom | Garden Empress | Six-petal floor rosette opening into a cape |
| Flame + Flame | Solar Guardian | Sharp sun crown and vertical flame train |
| Moon + Bloom | Dreamgarden Fae | Crescent-butterfly wings opening from a lunar ring |
| Moon + Flame | Eclipse Guardian | Eclipse disk and two diagonal comet blades |
| Bloom + Flame | Phoenix Rose | Phoenix feather fan ignited by a rose-vine spiral |

## First session

### First 30 seconds

1. Boot directly into the Dress Room.
2. The doll occupies the majority of the stage; no modal appears.
3. The first tap equips a garment and produces a short Essence hint.
4. Category and selected-state changes are obvious on touch and mouse.
5. `AWAKEN` becomes dominant after both focus slots are filled.

### Guided discovery without text walls

- Round 1 guarantees a new recipe and a strong reward.
- The result card reveals which two focus pieces produced the awakening.
- Round 2 offers a newly unlocked focus piece that creates a different pair.
- Round 3 leaves the next experiment to the player.
- The recipe album uses silhouettes and discovered cards instead of prose.

### Ten-minute content path

- Six first-discovery rounds.
- Two remix rounds using an already discovered pair with different support pieces.
- Unlocks after rounds 1, 3 and 6.
- Lookbook stores the best player-authored card for each recipe.
- No timer, ad gate, grind or artificial waiting counts as content.

## Content budget

Thirty shipping wearables:

- 6 hair choices;
- 6 main focus pieces: one dress and one top per Essence;
- 6 bottoms: two per Essence;
- 6 shoes: two per Essence;
- 6 magic accents: two per Essence.

All focus recipes are available from the first session. Cosmetic support items
may unlock during the discovery path.

## Runway sequence

`RUNWAY_INTRO -> CHARGE -> FLASH -> REVEAL -> VICTORY -> RECIPE_CARD`

- Two short NPC silhouettes/cards establish visible competition.
- The player's already equipped outfit charges in place.
- A silhouette flash hides the overlay swap.
- Static awakening layers use scale, rotation, alpha, camera, particles and sound.
- The player exits last, receives the strongest audience response and wins.

## Visual direction

- Electric magical editorial anime.
- Dark navy/plum runway used as contrast, with electric cyan, rose and gold light.
- Clean ink contour, cel shading and one controlled highlight.
- Large readable silhouette; no soft pastel boudoir or generic AI-fantasy noise.
- UI uses compact jewel/glass shapes and one saturated primary CTA.

## Analytics contract

Exact release schema:

| Category | What | Action |
|---|---|---|
| `ftue` | `first-equip` | `start`, then `complete` |
| `button` | `awaken-first` | `visible`, then `interact` |
| `round` | `1`, `2`, `3`, `6`, `8` | `start`, then `complete` |
| `awakening` | `moon-moon`, `bloom-bloom`, `flame-flame`, `moon-bloom`, `moon-flame`, `bloom-flame` | `discovered` |
| `recipes` | `2`, `3`, `6` | `reached` |
| `lookbook` | `main` | `visible`, `interact` |
| `lookbook` | `save-look` | `interact` |
| `session` | `60s`, `180s`, `300s`, `600s` | `reached` |
| `cohort` | `touch-portrait`, `touch-landscape`, `mouse-landscape` | `entered` |
| `player` | `fresh`, `returning` | `entered` |

The Poki adapter must route the approved low-cardinality milestones through
`PokiSDK.measure`; local game events remain the DevAPI/test evidence. No PII or
random user identifiers are emitted.

Expected first-session trace begins with cohort/player entry; first styling
input starts Poki gameplay, `ftue/first-equip/start` and `round/1/start`; first
equip completes FTUE; AWAKEN becomes visible/interacted; runway stops gameplay;
one awakening discovery and `round/1/complete` occur while stopped; Recipe Card
remains stopped; Restyle returns to Dress Room, resumes gameplay and emits
`round/2/start`. Automated tests assert this ordering and once-only milestones.

Pressing `AWAKEN` calls gameplay stop before the noninteractive runway. Active
time milestones pause through Intro/Charge/Flash/Reveal/Victory and the Recipe
Card result/menu. Gameplay and active time resume only after Restyle returns to
the interactive Dress Room.

## Discovery navigation contract

- After first use, focus-item cards permanently show their discovered Essence.
- The recipe album is a 3x3 pair matrix with six canonical cells.
- Unknown cells show distinct silhouettes; tapping one filters/highlights an
  owned main-focus and accent-focus combination capable of reaching that cell.
- The exact awakening art/title remains hidden until reveal.
- Repeating a known pair records a remix instead of blocking progress, then
  returns to the nearest reachable unknown cell.
- All six focus pairs are reachable from the wardrobe at first launch; rewards
  add cosmetic freedom rather than access gates.

## Go / iterate / kill

Official Poki Player Fit gate:

- average playtime >3 minutes;
- at least 25% of 500 players (>=125) play longer than 3 minutes.

Internal healthy targets:

- first equip >=85% of gameplay starts;
- first round complete >=70%;
- second round start >=55%;
- third round complete >=30%;
- active 5 minutes >=20%;
- six recipes reached >=12%;
- saved look >=15%;
- critical runtime failure <1%.

Decision table:

- `go`: official gate passes, average >=4 minutes, >=35% play >3 minutes and
  round-2 start >=55%;
- `iterate once`: official pass but below internal go, or round-2 start 40-55%;
- `kill/redesign`: official gate fails after one focused iteration, round-2
  start remains <40%, causality remains unclear, or players call the results
  the same fairy in different colors.

Critical failure means fatal/uncaught error, softlock, missing reveal asset,
save corruption or broken ad/lifecycle recovery. The denominator is gameplay
starts; <=4 affected sessions out of 500 may satisfy `<1%` only when no root
cause repeats. Any reproducible softlock blocks submission.

## Runtime and accessibility gates

- median 60 FPS and p95 frame <=33 ms during the representative mobile reveal;
- no input/reveal hitch >100 ms; mobile memory high-water <=180 MB;
- first visible loader response <=500 ms, desktop interactive <=4 seconds and
  Fast 4G interactive <=10 seconds;
- touch targets >=44x44 CSS px; normal text contrast >=4.5:1 and large/UI
  contrast >=3:1;
- color is not the sole selected-state signal; no flash rate above 3 Hz;
- mute and reduced-effects mode are available without breaking causality.

## Scope exclusions

No story chapters, full gallery/social sharing, multiplayer, generated video,
character rig, additional Essence, currency, shop, ads inside the first session,
or generalized reusable engine work belongs in this MVP.
