# Meme Evolution Concept Seed

Working status: accepted concept seed. The minimal GDD lives in `gdd.md`.

## Definition Of Done For This Pass

- Capture the selected project direction without inventing final lore or title.
- Define the target audience, genre reference, core verbs, design pillars, and
  no-go list.
- Record open questions that must be answered before visual proof or gameplay
  implementation.
- Keep visuals, runtime assets, and implementation work out of scope for this
  pass.

## Known Direction

- Audience: children.
- Genre reference: a merge/evolution game in the broad style of Cow Evolution.
- Core identity: `67 World`. Every collectible character is a variation of one
  central 67 character.
- Meme anchors from the user: strawberry, banana, 67, and similar playful meme
  content. Strawberry and banana are treatments of the 67 character, not
  separate non-67 creature families.
- Current platform target from project rules: mobile and PC.
- Current intent: a kid-friendly game where 67 characters evolve into many
  funny meme variants through simple combinations.
- Collection size decision: the full game promise is 67 different 67
  characters. The first playable slice implements 7.

## Working Fantasy

The player enters a bright `67 World`, collects cute and silly versions of the
67 character, combines matching ones, and discovers increasingly absurd meme
forms. Fruit motifs, stickers, costumes, sounds, and environments all orbit
around 67 rather than replacing it. The game should feel like a safe toybox of
internet-flavored jokes rather than a direct feed of current internet trends.

## Core Verbs

- Spawn: create a basic 67 character.
- Merge: combine two matching 67 variants into the next evolution.
- Collect: earn soft currency from discovered creatures.
- Discover: reveal new fruit, number, costume, sound, and joke variants of 67.
- Arrange: move creatures around a small readable playfield.

## First-Slice Loop

1. The player taps to spawn a basic 67 character.
2. Two matching 67 characters merge into a stranger, funnier 67 variant.
3. Each creature produces a small safe currency, such as giggle coins.
4. Currency unlocks strawberry-67 and banana-67 treatments.
5. The first session goal is to discover 5-7 variants from a larger promise of
   67 total characters and see one clear surprise transformation.

## Design Pillars

1. Child-safe meme absurdity
   - Humor is visual, musical, slapstick, and harmless.
   - Internet meme references must be filtered before use.

2. Instant merge clarity
   - Children should understand the action in the first 5 seconds.
   - Matching, dragging, combining, reward feedback, and next goal must be
     visually obvious.

3. Collectible toybox progression
   - Every evolution should feel like a new toy, not just a stat upgrade.
   - Progression should unlock new 67 variants, sounds, poses, costumes, and
     small environments.

## Pillar Violations

- Do not copy Cow Evolution characters, art, UI, names, or monetization beats.
- Do not use adult, political, hateful, scary, sexual, or cruel meme material.
- Do not depend on a meme that children cannot understand without social media.
- Do not treat strawberry, banana, or other meme anchors as separate main
  species; they are variations of the 67 character inside 67 World.
- Do not make 67 meaningful in an unsafe or obscure way; treat it as the
  central silly character/world identity unless the user defines otherwise.
- Do not add gambling pressure, dark patterns, or aggressive ad mechanics.

## 67 Variant Families

- Core 67 line: tiny 67, dancing 67, crown 67, cosmic 67.
- Strawberry-67 line: berry hat 67, jam 67, strawberry crown 67.
- Banana-67 line: banana peel 67, sleepy banana 67, banana band 67.
- Mixed meme line: smoothie 67, sticker 67, party 67, glitchy celebration 67.
- World line: garden 67, kitchen 67, arcade 67, cloud 67.

These are placeholders for direction only. Final names and visuals need user
approval.

## First Playable Slice Target

Playable proof should demonstrate:

- one small playfield;
- tap-to-spawn;
- drag or click-to-merge;
- 5-7 visible 67 variants from the planned 67-character collection;
- child-safe reward feedback;
- one currency and one unlock;
- desktop and mobile portrait readability.

## Visual Proof

- Current fake shot: `visuals/67-world-gameplay-fake-shot-v1.png`
- Tier: gameplay fake shot, not runtime asset.
- Direction: bright, polished, meme-loud 67 World screen where the base 67
  character evolves into fruit/costume/meme variants.
- Review status: accepted by the user on 2026-06-12.

## Superseded Visual Direction

The earlier fake-shot direction with fruit creatures as separate families is
superseded. Future visuals must show every character as a 67 variant in 67
World.

## Open Questions

1. Which 7-character roster names should be renamed before implementation?
2. Should the next proof be a character lineup of 67 variants or a playable
   prototype first?

## Next Gate

Review `gdd.md`, `data/balance.json`, and `data/ui_flow.json`, then implement
the first playable slice.
