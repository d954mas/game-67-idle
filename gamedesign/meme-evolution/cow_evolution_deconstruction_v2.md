# Cow Evolution Deconstruction V2

Status: corrective research pass after the current native screenshot was judged
visually and mechanically unclear.

Research date: 2026-06-12.

## Sources Checked

- Google Play listing, current public store page:
  https://play.google.com/store/apps/details?id=br.com.tapps.cowevolution&hl=en_US
- BlueStacks app page and screenshots:
  https://www.bluestacks.com/apps/simulation/cow-evolution-on-pc.html
- Dailymotion gameplay video page, "cow evolution recreate universe":
  https://www.dailymotion.com/video/x6kze3d
- Softonic iPhone page and screenshots:
  https://cow-evolution-merge-animals.en.softonic.com/iphone
- Behance art/project page:
  https://www.behance.net/gallery/25457769/Cow-Evolution
- Blog review with gameplay screenshots:
  https://umsofaalareira.blogspot.com/2015/06/games-cow-evolution-vacas-loucas_22.html

## Reference Lock

- mode: central deconstruction for field-first screen grammar; not deep enough
  for exact long-session balance.
- reference question: what must 67 World borrow from Cow Evolution so the first
  screen reads as a playable merge/evolution field instead of stacked UI?
- durable doc path:
  `gamedesign/meme-evolution/cow_evolution_deconstruction_v2.md`.
- required source packet: official/store text and screenshots, gameplay
  screenshot sequence or video frames, one supporting mechanics/pacing source,
  and current native 67 World capture.
- current native capture:
  `build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`
  checked 2026-06-13.
- no-coding/no-final-art boundary: future Cow Evolution-driven gameplay/UI/art
  changes must not start from memory or this summary alone; use the ready audit
  below to decide whether the change is unlocked.
- expected native proof: a native PC screenshot/scenario showing a compact HUD,
  field-owned interaction, in-world crate/spawn, mergeable characters, reward
  feedback near the field, and a supporting collection drawer.
- unlock condition: implementation may use this doc for field-first screen
  grammar and copy-risk decisions. Deep timing, one-hour economy, retention, or
  monetization conclusions require a separate deep deconstruction with a real
  video/player transcript.

## Definition Of Ready Checklist

- [x] mode matches implementation risk for field-first screen grammar.
- [x] source matrix is filled.
- [x] gameplay screenshot/frame sequence exists for screen grammar.
- [x] current native capture exists.
- [x] observation ledger has 5 visible beats before conclusions.
- [x] borrow / avoid / copy-risk are explicit.
- [x] current-build mismatch is written.
- [x] next native proof is named.
- [ ] deep timing/balance source is available.

Ready state: ready for screen grammar, first-screen composition, reward
placement, and copy-risk translation. Not ready for implementation decisions
that depend on exact first-minute timing, one-hour balance, retention pressure,
or monetization pacing.

## Source Matrix

| Source | Quality | Checked | Proves | Does not prove / uncertainty |
|---|---|---:|---|---|
| Google Play listing: `play.google.com/...br.com.tapps.cowevolution` | official/store | 2026-06-13 | Developer, current public positioning, screenshots/trailer presence, merge/cow/discovery/equipment/customization/world-stage claims, ads/IAP, PEGI 3, current update date. | Does not prove exact first input, object timings, merge animation cadence, or one-hour balance. |
| MWM App Store mirror: `mwm.ai/apps/cow-evolution-merge-to-evolve/901864337` | store screenshot mirror/analysis | 2026-06-13 | Five screenshot descriptions: initial farm layout, ready-to-merge cows, magnet/currency attraction, discovery notification, rapid `+500` coins, alien world, right-side world icons. | Not official; screenshot captions are enough for screen grammar but not for exact player timing. |
| Apponic Android page: `cow-evolution.apponic.com/android/` | secondary mechanics summary | 2026-06-13 | A basic cow arrives on a timer; two same-size cows combine into larger/mutant tiers; higher tiers earn more currency; upgrades/tractor automate earnings. | Old version page; useful for loop shape, not current store balance. |
| Dailymotion video page: `dailymotion.com/video/x6kze3d` | gameplay video page/frame | 2026-06-13 | Video title and frame show late-game farm: top compact HUD, fenced field, in-world crates, multiple cows, `FULL!` capacity indicator. | The page/frame was not enough to produce a timestamped first-minute transcript. |
| Softonic iPhone page: `cow-evolution-merge-animals.en.softonic.com/iphone` | secondary app page/screenshots | 2026-06-13 | Confirms idle clicker framing, stages/worlds, customization, quests, coins/upgrades, and a 5-image screenshot set. | Secondary summary; not a substitute for observed gameplay. |
| Current 67 World native capture: `build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png` | current build evidence | 2026-06-13 | Shows current implemented field, compact HUD, in-world crate, tutorial plaque, collection drawer, and current visual density. | Does not prove child comprehension without manual test. |

## Observation Ledger

| Beat | Source timestamp/frame | Visible screen state | Player action | Visible response | Reward/UI feedback | Inferred meaning |
|---|---|---|---|---|---|---|
| 1 | Apponic screenshot / image result | Bright green fenced pasture, small barn edge, wooden crate in field, several cows inside the world, compact top HUD. | Player is expected to interact with field objects, not a separate board UI. | Crate and cows are co-located in the pasture. | Top HUD shows coins/rate; field remains dominant. | First screen should be a living field; crate belongs inside the world. |
| 2 | Dailymotion video frame / screenshot | Multiple wooden crates are stacked in the fenced field beside cows; top bar shows coins, income rate, gems, and menu; bottom shows `FULL!`. | Player likely taps/opens crates and manages capacity. | The field can fill with crates/cows; capacity state appears without replacing the field. | `FULL!` is a compact state signal, not a modal takeover. | 67 World should show full/stuck state in-place and keep the field visible. |
| 3 | MWM screenshot description: initial farm layout | Multiple cows ready for merging; magnet attracts generated currency. | Player watches/collects currency and combines matching creatures. | Currency moves toward collection; cows remain readable as separate objects. | Coin attraction is visible near gameplay objects. | Reward feedback should happen near 67s/field, not only in top text. |
| 4 | MWM screenshot description: discovery notification | A newly evolved creature is spotlighted with an `ALPACOW DISCOVERED!`-style discovery moment. | Player has completed a merge/unlock. | Discovery uses a large celebration overlay. | The unlock moment is explicit and celebratory. | 67 World should make new variants obvious with a field/confetti/catalog glow. |
| 5 | MWM/Apponic alien-world screenshots | Red cratered world/alien scene still uses the same field grammar with mutant creatures and resource counters. | Player continues the same loop in a new world. | Background/world theme changes, but core merge/collect grammar persists. | Top counters and world icons support progression. | Future 67 worlds should reskin the field while preserving the core loop. |
| 6 | Current 67 World native capture | 67 World has compact top HUD, fenced tile field, an in-world crate, tutorial plaque, and bottom collection drawer. | Player can tap crate / merge matching 67s by scenario automation. | Field and UI are visible together. | Tutorial/collection drawer are readable but still large. | Current build moved in the right direction, but child comprehension still needs manual test and reward-density review. |

## Player Transcript From Observed Sources

- first screen: a fenced or themed field owns most of the screen; creatures and
  crates sit inside that field; the top HUD is compact currency/progress/status;
  secondary navigation sits at edges or bottom.
- first input: the evidence supports tapping/opening crates, collecting
  resources, and combining matching creatures. Exact tutorial order is not
  proven by the current source packet.
- visible response: crates/cows populate the field, higher-tier creatures
  replace lower ones after merges, coins/gems or collection effects appear near
  gameplay objects, and discovery overlays celebrate new species.
- first 10 seconds: likely crate/tap/collect/merge setup, but not ready for
  exact timing claims without a watched first-session video.
- first 60 seconds: likely repeated crate opening and first merge, but exact
  pacing is not source-backed enough for balance decisions.
- 1-5 minute loop: generate/spawn base creatures, merge pairs, collect coins,
  buy upgrades/equipment, reveal the next creature/world; exact timers and
  price curves remain inferred.

## What Cow Evolution Actually Feels Like

Cow Evolution is not primarily a grid UI game. It is a field-first merge idle
game:

- The main screen is a fenced field/world. Creatures are the focus.
- Wooden crates appear inside the world, not as a giant bottom UI button.
- The player gets basic cows, drags or combines identical creatures, and the
  next mutation appears in the field.
- Coin rewards float above creatures and merges. The field stays alive even
  when the player is not reading text.
- The top HUD is compact: currency, income rate, premium currency, menu/world
  buttons.
- Tutorial/marketing notes use large taped paper/speech-bubble overlays, but
  they do not replace the field as the main interaction.
- Progression expands by stages/worlds and catalog discoveries: farm, monster,
  alpaca, alien/planet style spaces.
- Customization is a secondary layer: hats, clothes, accessories, dressing
  room. It is not the core first-screen loop.

## Current 67 World Mismatch

The current native screenshot does not yet match that gameplay grammar:

- It looks like UI assets laid on top of each other, not a coherent game world.
- The board reads as a fixed beige grid, not a living play field.
- The spawn source reads as a bottom button with a crate image, not as crates
  appearing in the world.
- The collection cards dominate the lower half too early.
- HUD text and icons compete for attention instead of supporting the field.
- The generated UI crops are not release-ready: some pieces feel mis-cut,
  flipped, cramped, or visually unrelated.
- The core player verb is unclear. A child should understand: "open crates,
  merge matching 67s, collect coins, discover weirder 67s."

## Correct 67 World First Screen

The next native redesign should be:

```text
top compact HUD
fenced 67 World field
  - small crates spawn inside field
  - 67 characters occupy field positions
  - matching pair pulses
  - drag/click one 67 then matching 67 merges them
  - coins and stars pop above the merge
bottom compact drawer
  - next goal + small catalog strip, not giant cards
```

The field owns the game. UI supports it.

## Gameplay Translation

Use this first-minute loop:

1. A crate appears in the field.
2. Player taps crate -> Tiny 67 pops out.
3. Another crate appears quickly.
4. Player taps crate -> second Tiny 67.
5. Matching Tiny 67s pulse.
6. Player drags/taps Tiny onto Tiny -> Berry 67 appears.
7. Coins pop above the new Berry.
8. Bottom catalog briefly highlights Berry.
9. Goal changes to Banana; gameplay continues.

No separate "Auto Merge" gameplay should be visible to the child. DevAPI can
keep semantic merge actions, but player-facing UI should be field interaction.

## Visual Direction Fix

Do not patch the current bad composition by adding more labels. Replace the
screen composition:

- Generate/compose a coherent field background: sky/grass/fence/soft doodle
  world.
- Use separate reusable objects: crate closed/open, coin, sparkle, field
  shadow, compact catalog tile, small HUD capsules.
- Keep button/panel slice9 assets, but regenerate or re-crop the bad UI sheet
  before relying on it.
- Characters need transparent sprites with clean feet/bottom pivots and no
  remaining chroma-key fringe.
- A screenshot is rejected if a major UI element looks cut from a different
  scale/style or overlaps the field incoherently.

## Implementation Target

The next native implementation pass should not add more variants first. It
should redesign the first playable screen around field-first gameplay:

- hidden or subtle logical slots, visually placed as field positions;
- crate objects inside the field;
- compact top HUD;
- compact catalog drawer with the next 5-7 variants;
- direct field click/drag merge behavior;
- visual coin/reward flyouts;
- native screenshot evidence at 960x540.

Only after this feels like a game should batch-2 content and one-hour balance
expand.
