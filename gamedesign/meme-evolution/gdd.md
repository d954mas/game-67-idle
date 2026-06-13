# 67 World Minimal GDD

Status: first playable slice spec after accepted visual direction.

## Accepted Direction

- Game identity: `67 World`.
- Audience: children.
- Tone: meme-loud, bright, polished, funny, safe.
- Genre: merge/evolution collection game.
- Visual proof: `visuals/67-world-gameplay-fake-shot-v1.png`.
- Reference research: `reference_research.md`.
- Release roadmap: `release_roadmap.md`.
- Key decision: every collectible is a different 67 character. Fruit and meme
  elements are costumes, mutations, props, or world motifs for 67.
- Collection promise: 67 total 67 variants over the full game. The current
  native release-track slice proves the first 30 in state/progression with
  generated runtime sprites.

## Player Fantasy

The player fills a colorful toybox world with living 67 characters. Matching
67s merge into louder, funnier variants. Each new variant looks like a toy the
player wants to keep, poke, and unlock.

## First 7 Variants

| Order | Id | Name | Visual idea | Unlock |
|---:|---|---|---|---|
| 1 | `tiny_67` | Tiny 67 | Blue living number mascot | Start |
| 2 | `berry_67` | Berry 67 | 67 with strawberry body/hat | Merge two `tiny_67` |
| 3 | `banana_67` | Banana 67 | 67 with banana peel costume | Merge two `berry_67` |
| 4 | `smoothie_67` | Smoothie 67 | 67 inside smoothie cup | Merge two `banana_67` |
| 5 | `cool_67` | Cool 67 | 67 with glasses and chain | Merge two `smoothie_67` |
| 6 | `portal_67` | Portal 67 | 67 glowing with world portal energy | Merge two `cool_67` |
| 7 | `mystery_67` | Mystery 67 | Silhouette/unlock tease for next world | First milestone |

These are gameplay IDs and starter names, not final marketing names.

## Batch 2 Runtime Variants

The native runtime and balance data now include variants 8-18 as the second
content batch:

| Order | Id | Name | Visual idea | Unlock |
|---:|---|---|---|---|
| 8 | `jelly_67` | Jelly 67 | Wobbly candy/jelly 67 | Merge two `mystery_67` |
| 9 | `lemon_67` | Lemon 67 | Sour yellow 67 | Merge two `jelly_67` |
| 10 | `watermelon_67` | Watermelon 67 | Green-pink slice costume 67 | Merge two `lemon_67` |
| 11 | `bubblegum_67` | Bubblegum 67 | Pink bubble 67 | Merge two `watermelon_67` |
| 12 | `sticker_67` | Sticker 67 | Peel-off sticker 67 | Merge two `bubblegum_67` |
| 13 | `party_67` | Party 67 | Confetti party 67 | Merge two `sticker_67` |
| 14 | `arcade_67` | Arcade 67 | Arcade cabinet neon 67 | Merge two `party_67` |
| 15 | `cloud_67` | Cloud 67 | Puffy sky 67 | Merge two `arcade_67` |
| 16 | `crown_67` | Crown 67 | Royal crown 67 | Merge two `cloud_67` |
| 17 | `rocket_67` | Rocket 67 | Toy rocket 67 | Merge two `crown_67` |
| 18 | `rainbow_67` | Rainbow 67 | Rainbow aura 67 | Merge two `rocket_67` |

## Batch 3 Release-Track Variants

The native runtime and balance data now include variants 19-30 as the first
hour content target:

| Order | Id | Name | Visual idea | Unlock |
|---:|---|---|---|---|
| 19 | `neon_67` | Neon 67 | Blue/pink neon sign 67 | Merge two `rainbow_67` |
| 20 | `gummy_67` | Gummy 67 | Glossy candy gummy 67 | Merge two `neon_67` |
| 21 | `pixel_67` | Pixel 67 | Blocky pixel-art 67 | Merge two `gummy_67` |
| 22 | `lava_67` | Lava 67 | Hot lava-rock 67 | Merge two `pixel_67` |
| 23 | `donut_67` | Donut 67 | Sprinkle donut 67 | Merge two `lava_67` |
| 24 | `slime_67` | Slime 67 | Green slime toy 67 | Merge two `donut_67` |
| 25 | `disco_67` | Disco 67 | Mirror disco 67 | Merge two `slime_67` |
| 26 | `dragon_67` | Dragon 67 | Friendly dragon 67 | Merge two `disco_67` |
| 27 | `ninja_67` | Ninja 67 | Soft toy ninja 67 | Merge two `dragon_67` |
| 28 | `galaxy_67` | Galaxy 67 | Starfield 67 | Merge two `ninja_67` |
| 29 | `golden_67` | Golden 67 | Gold crown 67 | Merge two `galaxy_67` |
| 30 | `cosmic_67` | Cosmic 67 | Cosmic dome 67 | Merge two `golden_67` |

## Core Loop

Intent -> action -> timer/check -> reward -> visible change -> unlock/choice ->
next intent:

1. Player wants a new 67.
2. Player taps the big spawn button.
3. If a board slot is free, the crate spawns `tiny_67` at first, then the best
   unlocked crate tier after Better Crate upgrades.
4. Existing 67 variants generate giggle coins over time.
5. Player drags two matching variants together.
6. Merge creates the next 67 variant, plays celebration feedback, and adds it
   to the collection tray.
7. New variant increases passive income and reveals the next unlock hint.
8. Player decides whether to spawn more, merge more, clear a stuck full board,
   or buy the next upgrade.

## First 30 Seconds

- Start screen shows the 67 World playfield, top currency bar, collection tray,
  and one glowing spawn button.
- Player taps spawn twice and gets two `tiny_67` characters.
- UI hints by showing a soft arrow/outline between matching 67s.
- Player drags one `tiny_67` onto the other.
- Merge creates `berry_67`, shows confetti, adds slot 2 to the collection tray,
  and awards a small giggle coin bonus.

## First 5 Minutes

- Player discovers `tiny_67`, `berry_67`, `banana_67`, and ideally reaches
  `smoothie_67`.
- Player buys one simple upgrade: `faster_spawn`.
- Board never requires more than one screen or complex navigation.
- The next goal remains visible: "discover the next 67 variant" represented by
  icon/silhouette, not tutorial prose.

## Systems

- Primary action: tap spawn.
- Passive/idle action: owned 67 variants generate giggle coins.
- Upgrade 1: faster spawn.
- Upgrade 2: repeatable Better Crate levels.
- Stats affected: spawn cooldown and crate spawn tier.
- Currency source: passive production and merge bonus.
- Currency sink: faster spawn and Better Crate upgrades.
- Unlock: new 67 variants by merge tier.
- Visual/status change: collection tray fills, next silhouette glows, variant
  art changes.
- Reason to return: idle coins make the next merge/upgrade easier.

## Economy

- Soft currency: giggle coins.
- First reward: 1 `tiny_67` per spawn.
- First upgrade cost: 25 giggle coins.
- First upgrade effect: spawn cooldown goes from 1.5s to 1.0s.
- Better Crate upgrade: after Faster Spawn, the same progress upgrade slot
  raises the crate spawn tier. Level `N` spawns the highest discovered variant
  at index `N`, capped below Cosmic so the final unlock still requires merging
  two Golden 67s.
- Board-pressure recovery: if all 12 slots are full and no pair exists, the
  crate action clears the lowest 67 into coins and frees one slot.
- Passive production: each placed variant produces coins every 5 seconds.
- Merge bonus: creating a new highest variant grants a one-time coin bonus.
- First milestone: discover 7 variants, then unlock the next world preview.
- Release-track milestone: discover 30 variants and reach Cosmic 67 in the
  55-60 minute simulator window. Current simulator result: Cosmic at 57.19m.

## UI Flow

The first slice uses one main screen:

- top bar: giggle coins, collection progress `N / 67`;
- center: merge board with fixed slots;
- bottom tray: first 7 collection slots;
- primary button: spawn 67, or clear the lowest 67 if the board is full with no
  pairs;
- progress upgrade slot: buys Faster Spawn first, then Better Crate levels;
- feedback: merge burst, coin flyout, new slot glow;
- blocked state: board full, merge or clear space;
- next goal: glowing silhouette of the next 67 variant.

## Out Of Scope For First Slice

- Ads, premium currency, timers longer than a few seconds.
- Full 67-character content production beyond the first 30 release-track
  variants.
- Multiple worlds beyond one preview state.
- Online features, accounts, leaderboards.
- Copying Cow Evolution art, UI, characters, naming, or monetization.

## Implementation Notes

- Use `data/balance.json` for first-slice values.
- Use `data/ui_flow.json` for the one-screen UI contract.
- Use `reference_research.md` for Cow Evolution loop lessons and copy-risk
  boundaries.
- Treat `visuals/67-world-gameplay-fake-shot-v1.png` as direction only, not a
  cut-up runtime asset.
- New production variants should use generated source sheets, crop manifests,
  runtime PNGs, explicit pack builds, and native screenshot evidence.
