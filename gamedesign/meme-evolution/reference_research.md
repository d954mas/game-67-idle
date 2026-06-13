# 67 World Reference Research: Cow Evolution

Status: second corrective pass. Research date: 2026-06-12.

Purpose: understand the merge/evolution genre reference named by the user, then
translate useful patterns into original, child-safe `67 World` systems. This is
not permission to copy Cow Evolution characters, UI, text, monetization, or IP.

Latest corrective deconstruction:

- `gamedesign/meme-evolution/cow_evolution_deconstruction_v2.md`

The most important update: Cow Evolution is field-first. The main interaction is
creatures and crates inside a living world, with compact supporting HUD. The
current 67 World native screenshot overweights board/UI/card composition and
must be redesigned around field interaction before adding more content.

## Ref: Cow Evolution: Idle Merge Game

- Source quality: marketplace/store, primary listing.
- Link/date: Google Play, observed 2026-06-12:
  <https://play.google.com/store/apps/details?id=br.com.tapps.cowevolution&hl=en_US>
- Why included: closest user-provided genre reference and current public store
  description.
- Core loop: merge animals to discover new mutant species, collect coins and
  crystals, upgrade equipment, expand a farm/collection.
- UI/status pattern: top currencies, visible collection/progress fantasy,
  repeated merge board, customization/equipment hooks, daily/quest language.
- Progression fantasy: become a mutant animal farm tycoon; discover more
  species, worlds, stages, items, and customizations.
- Borrow: merge identical collectibles, visible catalog pressure, passive
  currency, upgrades, world/stage reveals, silly mutation surprise.
- Avoid: forced-ad pressure, hard-currency lockups, crude humor level, copying
  cows/mutant silhouettes/farm UI/wordplay.
- Copy-risk: exact cow mutation chain, farm framing, taped note UI style,
  cow-themed puns, aliens/alpacas/monster cow staging.

## Ref: Cow Evolution iOS/Android Store Mirror

- Source quality: secondary marketplace mirror.
- Link/date: WorldsApps, observed 2026-06-12:
  <https://worldsapps.com/download-cow-evolution-animal-merge>
- Why included: provides iOS-oriented summary, in-app purchase examples, and
  visible user review themes.
- Core loop: merge and combine mutant species; collect coins/crystals; upgrade
  farm.
- UI/status pattern: explicit IAP categories such as ad removal, income doubler,
  instant coin doubler, currency packs.
- Progression fantasy: lots of merges, comic absurdity, long idle growth.
- Borrow: players respond to "so many merges" and childish/stupid humor when
  the loop keeps giving new creatures.
- Avoid: building first-slice progression around ads, currency packs, or hard
  locks. Reviews complain when ads dominate or diamonds become the only path.
- Copy-risk: pricing structure, ad-removal economy, income doubler pacing.

## Ref: Cow Evolution Art Direction Board

- Source quality: secondary portfolio / art reference.
- Link/date: Behance project by Viviane Matsukuma, published 2015:
  <https://www.behance.net/gallery/25457769/Cow-Evolution>
- Why included: shows how the original game sells mutation variety through
  readable silhouettes and exaggerated costumes.
- Core loop: not a mechanics source; useful for visual grammar only.
- UI/status pattern: icon rows for currencies/rewards and a lineup of extreme
  character mutations.
- Progression fantasy: "what weird form comes next?" is communicated through
  silhouette and costume exaggeration.
- Borrow: strong silhouette change per tier, toy-like expression, each variant
  feeling like a new collectible.
- Avoid: cow anatomy, cow spots, deity cow, singer cow, rainbow cow concepts,
  and direct costume riffs.
- Copy-risk: making 67 variants map one-to-one onto famous Cow Evolution forms.

## Ref: Cow Evolution Secondary iPhone Description

- Source quality: secondary app page.
- Link/date: Softonic, observed 2026-06-12:
  <https://cow-evolution-merge-animals.en.softonic.com/iphone>
- Why included: confirms common public framing: idle clicker, stages, catalog,
  customization, quests.
- Core loop: merge cows into mutants, complete main quests, earn items/coins,
  upgrade the farm.
- UI/status pattern: stages/worlds and catalog expansion.
- Progression fantasy: monster/alpaca/alien stages broaden the same merge base.
- Borrow: multiple world layers can extend one core character identity.
- Avoid: replicating the three-stage theme or "farm tycoon" wrapper.
- Copy-risk: monster/alpaca/alien stage naming and structure.

## Cow Evolution Decomposition

### Player Verbs

- Generate: get a base creature from a box/spawner.
- Merge: combine two identical creatures into the next tier.
- Collect: gather currency from creatures over time.
- Upgrade: spend currency to improve income/spawn/progression.
- Discover: fill a catalog with new creature forms.
- Customize: decorate creatures with hats, clothes, accessories.
- Travel: unlock new worlds/stages once the current space is exhausted.

### Systems

- Board/pasture: limited visible play space creates merge decisions.
- Spawner/crate: supplies low-tier creatures.
- Merge resolver: deterministic `2 of tier N -> 1 of tier N+1`.
- Catalog: records discovered forms and creates completion pressure.
- Soft currency: generated passively and from actions.
- Hard currency: appears in reference, but should be out of scope for our first
  child-safe prototype.
- Upgrades: increase production, spawn speed, or unlock helper systems.
- Quest/daily layer: retention wrapper; out of scope for first prototype.
- World/stage layer: new visual context and new set of evolutions.
- Customization layer: cosmetic hats/accessories; useful later for meme skins.

### Balance Shape

The core merge math is exponential: tier 2 costs 2 base spawns, tier 3 costs 4,
tier 4 costs 8, tier 5 costs 16, tier 6 costs 32, tier 7 costs 64. A good early
slice must make the first 3-4 discoveries fast, then slow down before the full
7-character milestone.

For `67 World`, the first prototype should target:

- first spawn: immediate;
- first merge / `Berry 67`: under 30 seconds;
- `Banana 67`: under 90 seconds;
- `Smoothie 67`: under 3 minutes;
- first upgrade: around 2-4 minutes;
- `Cool 67` or higher: stretch goal for the first session, not required in the
  first minute;
- full 7 variants: not required in the first prototype session, but the
  silhouette should be visible.

## Translation To 67 World

### Adopt

1. `2 matching -> 1 next variant` should remain the core rule.
2. The collection tray must always show the next missing 67 silhouette.
3. Every new variant must visibly change silhouette, color, pose, or prop.
4. Idle coins should make waiting useful, but clicking/merging should be the
   fun part.
5. Worlds should be variations of `67 World`, not separate species families.

### Avoid

1. No cows, farm wrapper, cow puns, or mutation chain copied from the reference.
2. No forced ads, hard-currency locks, or upgrade pacing that makes a child wait
   days.
3. No crude humor; our humor is meme-loud, visual, slapstick, and safe.
4. No full economy before the first board loop feels good.
5. No invisible math. The next goal must be visually obvious.

### Prototype Balance Adjustments

Current `data/balance.json` is acceptable for a first runnable slice, with two
things to watch:

- The `faster_spawn` cost of 25 giggle coins may be too high if passive income
  is only checked every 5 seconds and the player has few variants.
- `mystery_67` as a real merge result requires 64 base spawns. That is fine as
  a milestone, but the prototype should not require reaching it to prove fun.

Recommended prototype tuning if the first run feels slow:

- Set first upgrade cost to 15 instead of 25.
- Add a small merge bonus every merge, not only first discovery.
- Keep `Mystery 67` as a locked silhouette until the loop has juice.

### UI Requirements Derived From Reference

- Top bar: soft currency + collection progress `N / 67`.
- Center: board with visible 67 pieces and obvious matching pairs.
- Bottom: collection tray with locked/unlocked variants.
- Buttons: spawn, merge, upgrade must be stable DevAPI targets.
- Feedback: coin flyouts, merge burst, new variant glow.

## Open Research Questions

- Should `67 World` have worlds by meme theme, such as Fruit World, Portal
  World, Arcade World, instead of animal-style stages?
- Should customization be a later layer of hats/accessories for each 67, or are
  all costumes separate collectible variants?
- Should the first version expose manual drag merge, one-click merge, or both?

## Immediate Implementation Implications

- Continue the prototype with placeholders; do not block on final art.
- Keep `game.action.spawn_67`, `game.action.merge_matching_67`, and
  `game.action.buy_faster_spawn` as semantic DevAPI endpoints.
- Use screenshots to judge whether the player can identify matching pairs and
  the next collection goal without reading design docs.
