---
type: Reference Deconstruction
title: Voxelheim Competitor Deconstruction 2026-06-17
description: Current competitor and reference scan for rescuing Voxelheim from a generic idle RPG prototype.
tags: [voxelheim, references, competitors, idle-rpg, ui, market]
checked: 2026-06-17
---

# Voxelheim Competitor Deconstruction

Mode: **central product deconstruction for GDD/UI direction**.

Status: **not ready for final art or exact economy implementation**. This packet
uses official/store pages, visible screenshot pages, guides, reviews, and local
market research. It is strong enough to redirect Voxelheim's product shape and
GDD. It is not enough to clone timing, exact screen layouts, monetization, or
late-game balance because raw gameplay video frame capture was not completed.

## Reference Lock

- Reference question: what current idle/AFK/RPG competitors prove about hook,
  UI hierarchy, first-session decisions, progression fantasy, and why the
  current Voxelheim prototype feels too simple.
- Durable doc path:
  `gamedesign/projects/voxelheim/references/competitor_deconstruction_2026-06-17.md`
- Current build capture:
  `build/captures/after_final.png`
- No-coding/no-final-art boundary: do not implement final UI/art from these
  references until the next pass captures fresh native proof and, if needed,
  raw gameplay frames for the chosen closest reference.
- Expected proof: native screenshot showing combat + visible keep rebuild +
  one primary action + 3-card choice/loot beat.

## Source Matrix

| Source | Link | Quality | Checked | Proves | Does not prove |
|---|---|---:|---|---|---|
| Legend of Mushroom, Google Play | https://play.google.com/store/apps/details?id=com.mxdzzus.google | marketplace/store | 2026-06-17 | current rating/downloads, official feature claims: lamp gear, classes, alliances, garden, idle RPG tags | exact first-minute timing |
| Legend of Mushroom Beginner Guide, Theria | https://theriagames.com/guide/legend-of-mushroom-beginner-guide/ | guide with screenshots | 2026-06-17 | system density: rush events, lamps, gear, stats, pals/skills, server-day unlocks, passes | official intent or revenue |
| Capybara Go!, Google Play | https://play.google.com/store/apps/details?id=com.habby.capybara | marketplace/store | 2026-06-17 | current rating/downloads, text roguelike RPG positioning, companions, random events, choices/luck | detailed economy math |
| Capybara Go! AppGrowing analysis | https://appgrowing.net/blog/en/capybara/ | market/product analysis | 2026-06-17 | one-button MUD loop, Next Day, 1-of-3 skills, equipment/talents/pets/chests, Habby formula | exact live balance; market estimates are directional |
| AFK Journey, Google Play | https://play.google.com/store/apps/details?id=com.farlightgames.igame.gp | marketplace/store | 2026-06-17 | current rating/downloads, positioning: formation strategy, shared levels, auto-battle/AFK, 100 heroes, seasons | exact source of retention |
| AFK Journey FAQ/screens | https://playafkjourney.com/faq/ | guide/screens | 2026-06-17 | auto-chess framing, dailies, endgame modes, strategic team-building | official design intent |
| Tap Titans 2, Google Play | https://play.google.com/store/apps/details?id=com.gamehivecorp.taptitans2 | marketplace/store | 2026-06-17 | proven clicker/idle RPG formula: tap, heroes, pets, skills, prestige, artifacts, clans/raids/events | exact formulas |
| Legend of Slime, Google Play | https://play.google.com/store/apps/details?id=com.loadcomplete.slimeidle | marketplace/store | 2026-06-17 | current idle RPG competitor, auto-battle/offline, equipment, companions, review pain around ads/UI overload | exact first-session timing |
| VOODOO 2026 hybrid-casual notes | `gamedesign/sources/voodoo_new_big_three_hybrid_casual_2026-06-15.md` | local source note from AppMagic article | 2026-06-15 | current hybrid-casual pattern: satisfying core action + light gameplay + deeper meta | RPG-specific retention |

## Current Competitor Signals

### 2026-06-17 Current Store Refresh

Checked live Google Play pages again after the lead feedback that Voxelheim's UI
is ugly/unclear and the GDD feels banal. These are not raw gameplay-frame
deconstructions, but they are current enough to update competitor positioning:

- **Legend of Slime: Idle RPG War** - Google Play lists 10M+ downloads, 177K
  reviews, 3.8 rating, updated May 22, 2026. The store page sells auto-battle,
  offline coins, attack/health/recovery upgrades, slime squads, weapons/armor,
  companions, and tap power-up mechanics. Recent reviews also flag random ads,
  overwhelming returned-player UI, and inconsistent UI across systems.
- **Slayer Legend: Idle RPG** - Google Play lists 5M+ downloads, 147K reviews,
  4.7 rating, updated May 29, 2026. Its store copy leads with "10 minutes a
  day," AFK rewards, retro pixel identity, gear, magic talents, and elemental
  skill combinations. Reviews praise constant progression and visible character
  growth, but also show that balance nerfs/offline reward changes can break
  trust.
- **Blade Idle** - Google Play lists 1M+ downloads, 79.7K reviews, 4.1 rating,
  updated June 8, 2026. Store copy sells stage/dungeon farming, skills, merge
  equipment, pets, relics, insignia, skins. A useful negative review says the
  game loses engagement when the character stays in one place, fights repeated
  mobs, and the rest is mostly menus/buttons.
- **Tap Titans 2** - Google Play lists 10M+ downloads and 1.07M reviews with a
  4.7 rating. The page still sells tap-to-slay, heroes, pets, skills, prestige,
  artifacts, equipment, clans, raids, events, tournaments, cards, and dust.
- **AFK Journey** - Google Play lists 5M+ downloads, 297K reviews, 4.4 rating,
  updated June 3, 2026. The store page sells strategic formations, timed
  ultimates, shared levels/equipment, 100 heroes, seasons, storybook world
  quality, auto-battle, and AFK rewards.

Refresh takeaway:

- Voxelheim cannot compete as "auto battle + four upgrades." Every current
  competitor wraps idle in either a distinct avatar, deep visible collection,
  strategic setup, prestige/social/events, or a memorable repeated action.
- The reviews are as important as the feature lists: too many menus, UI
  mismatch, boring static combat, and unclear returned-player state are live
  product risks in this genre.
- Voxelheim's safest ownable hook remains **visible Frost Keep rebuilding**.
  That hook must be louder than generic DPS/gold, and the UI must stage systems
  instead of exposing a menu pile.

### Legend of Mushroom

Observed/store facts:

- Google Play lists 5M+ downloads, 99K+ reviews, 4.1 rating, updated June 12,
  2026, and tags it as Role Playing / Idle RPG / Casual.
- The official store copy claims 30M+ worldwide downloads, "infinite levels,"
  tap-the-Genie equipment, class choice, alliance/boss challenge, and a garden.
- The guide shows a dense ecosystem: rush events, lamps/gear, mounts, pals,
  skills, stats, day-based unlocks, battle passes, privileges, adventure rank,
  and auto-gear filters.

What it teaches Voxelheim:

- A silly/cute avatar hook plus many visible collection/status systems can make
  an idle RPG feel like a product, not just a damage simulator.
- UI density is high, but the player always has a concrete collection goal:
  gear, mount, pal, rank, event, class, garden.
- Copy-risk is high for "mushroom/lamp" identity. Borrow the visible
  collection/status fantasy, not the mushroom/lamp skin.

### Capybara Go!

Observed/store and analysis facts:

- Google Play lists 10M+ downloads, 457K reviews, 4.5 rating, updated May 26,
  2026, and tags it as Role Playing / Idle RPG.
- Store copy sells a "text-based roguelike RPG" with random events, companions,
  choices, gear, and luck.
- AppGrowing describes a loop where players press a `Next Day` button, trigger
  random events, fight enemies, choose one skill card from three on level-up,
  then deepen through equipment, talents, pets, chests, tower, arena, and
  dungeons.
- AppGrowing's synthesis for Habby is "addictive casual gameplay + roguelike
  elements + peripheral progression."

What it teaches Voxelheim:

- The first action can be extremely simple if every click triggers a readable
  event and an immediate decision/reward.
- "One of three" choice cards are a practical way to add agency to idle.
- A strong character meme/hook matters. Current Voxelheim has no equivalent.

### AFK Journey

Observed/store and guide facts:

- Google Play lists 5M+ downloads, 297K reviews, 4.4 rating, updated June 3,
  2026.
- Store copy emphasizes strategic formations, six classes, timed ultimates,
  shared levels/equipment, 100 heroes, seasonal adventures, world exploration,
  auto-battle, and AFK rewards.
- The FAQ frames AFK Journey as auto-chess in an open-world environment with
  strategic team-building, common daily tasks, and multiple endgame modes.

What it teaches Voxelheim:

- Idle/AFK is not enough by itself. Modern successful AFK products sell
  strategy, characters, world, and seasons around the idle resource layer.
- Shared levels/equipment reduce friction. Voxelheim can borrow the principle:
  new companions/rooms should be usable immediately, not trapped behind grind.

### Tap Titans 2

Observed/store facts:

- Google Play lists 10M+ downloads, 1M+ reviews, 4.7 rating, updated June 9,
  2026.
- Store copy sells tap-to-slay, heroes, pets, skills, prestige for artifacts,
  equipment, clans, raids, seasonal events, tournaments, cards, and dust.

What it teaches Voxelheim:

- Tap/active layer plus prestige/meta keeps a simple battle screen alive.
- The battle screen is only half the game; the persistent upgrade, clan/event,
  equipment, and prestige layers carry retention.
- Voxelheim should borrow the clear combat + prestige grammar, not the exact
  Sword Master/Titan structure.

### Legend of Slime

Observed/store and review facts:

- Google Play lists 10M+ downloads, 177K reviews, 3.8 rating, updated May 22,
  2026.
- Store copy sells auto-battle, offline rewards, coins, attack/health/recovery,
  slime squads, weapons/armor/equipment, companions, and tap mechanics.
- Recent reviews complain about random ads, overwhelming new systems, and UI
  that does not match across features.

What it teaches Voxelheim:

- More systems can help retention, but dumping them into the UI can hurt
  readability and trust.
- The rescue should add visible depth through staged unlocks, not expose every
  system on the first screen.

## Observation Ledger

| Beat | Source | Visible/action fact | Reward/UI feedback | Inferred design meaning |
|---|---|---|---|---|
| 1 | Capybara Go! AppGrowing | player advances by pressing one `Next Day` style button | random event or battle appears | one simple input can support deeper RPG if event feedback is immediate |
| 2 | Capybara Go! AppGrowing | level/events can offer 1 of 3 skill cards | player chooses a build modifier | idle needs choice beats, not only passive waiting |
| 3 | Legend of Mushroom guide | main page links to lamps, gear, stats, pals/skills, events | multiple visible growth objects | successful idle RPGs make progression collectible and inspectable |
| 4 | AFK Journey store/FAQ | formation/team and `Battle` are the primary action before auto-combat | team position, hero level, and battle button are visible | AFK loops can still ask the player for strategic setup |
| 5 | Tap Titans 2 store | tap, heroes, pets, skills, prestige/artifacts, raids/events are all sold as key features | battle and upgrade/meta layers coexist | one-screen combat must connect to a broader meta loop |
| 6 | Voxelheim current capture | four equal upgrade buttons under a static scene | numbers rise, but keep/hero do not transform | current loop lacks a visible product fantasy |

## Synthesis

The market does not reward "idle RPG with gold and upgrades" as a complete
idea anymore. Current competitors wrap the idle layer in at least one of these:

1. a distinctive avatar or meme hook;
2. a visible collection identity;
3. a build/choice layer;
4. a social/event/season layer;
5. a satisfying repeated core action;
6. a strong readable UI hierarchy.

Voxelheim currently has none of these strongly enough. Its best available
differentiator is the **blocky Frost Keep**. That should become the visible
progression object.

## Borrow / Avoid / Copy-Risk

Borrow:

- Capybara Go!: one simple action plus 1-of-3 build choices.
- Legend of Mushroom: visible gear/companion/status collection and silly
  approachable identity.
- AFK Journey: shared progression friction reduction and readable strategy
  setup before auto-resolve.
- Tap Titans 2: central enemy, active burst/skill, prestige grammar.
- Voodoo hybrid-casual pattern: one satisfying core moment plus deeper meta.

Avoid:

- burying the first session under many icons;
- forced/random ads as part of the first product promise;
- server-whale pressure and predatory social competition;
- UI that requires guide/wiki reading after one hour;
- adding systems before the first screenshot is readable.

Copy-risk:

- do not use mushrooms, lamps, capybara meme identity, AFK Journey hero
  factions, Sword Master/Titans, or exact store layouts;
- do not copy paid event pressure or random-item monetization patterns into the
  first slice.

## Mismatch Against Current Build

Current screenshot: `build/captures/after_final.png`.

- No distinctive avatar/hook beyond generic blocky hero.
- No visible collection/status fantasy.
- No 1-of-3 decision beat.
- Keep is decorative instead of player-built.
- UI is dark, dense, and low hierarchy.
- No active skill despite `balance.json` v4 describing one.
- No implemented expanded upgrades despite `balance.json` v4 describing them.
- No social/event layer. This is acceptable for first slice, but the GDD must
  leave room for staged events instead of pretending the four-button loop is
  enough.

## Rescue Direction

Make Voxelheim a **Frost Keep Rebuilder idle RPG**:

- auto-combat recovers Gold, Frost Blocks, and Gear Lamps;
- after short combat packets, player chooses one of three rune/loot cards;
- Frost Blocks repair visible rooms in the keep;
- rooms unlock companions, buffs, and new choice cards;
- bosses drop blueprints;
- prestige becomes an Avalanche Reset that preserves blueprints/cosmetics and
  converts highest Keep Rank into Frost Shards.

This is inspired by the competitor structures but is not a clone: the product
fantasy is "rebuild the frozen keep as a toy-block diorama while your hero
idles and chooses expeditions."

## Reference Digest

- Mode: central product deconstruction, source packet incomplete for raw
  gameplay timing.
- Sources checked: Google Play pages for Legend of Mushroom, Capybara Go!, AFK
  Journey, Tap Titans 2, Legend of Slime; AppGrowing Capybara analysis; AFK
  Journey FAQ; Theria Legend of Mushroom guide; local VOODOO 2026 notes.
- Observed facts:
  - Capybara Go! sells a text roguelike/idle RPG with random events, companions,
    choices, and 10M+ Google Play downloads.
  - Legend of Mushroom sells lamp gear, classes, alliance/boss play, garden, and
    30M+ claimed worldwide downloads.
  - AFK Journey sells formation strategy, shared levels, auto-battle/AFK
    rewards, 100 heroes, and seasons.
  - Tap Titans 2 still sells tap, heroes, pets, skills, prestige/artifacts,
    clans, raids, and events.
  - Legend of Slime shows the retention value and UX danger of piling on
    equipment, companions, passes, ads, and many UI systems.
- Current-build mismatch: Voxelheim has only generic auto-combat + four
  upgrades, with no visible keep rebuilding, no choice beat, no distinctive
  collection hook, and a weak UI hierarchy.
- Borrow: 1-of-3 choices, visible collection/build progression, active burst,
  central enemy, staged meta.
- Avoid: clutter, monetization pressure, random ads, exact identities/layouts.
- Next native proof: one screenshot with combat, visible keep repair, one
  primary action, and a 3-card choice.
