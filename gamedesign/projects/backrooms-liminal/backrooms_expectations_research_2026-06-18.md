# Backrooms Expectations Research

Project: `backrooms-liminal`
Checked: 2026-06-18
Mode: quick-to-central reference correction after lead rejection

## Why This Exists

The current build was rejected as "not like Backrooms": it reads as one straight
path with a chase, not as exploration, lostness, or the feeling of being trapped
in a shifting place. Earlier project docs had only a quick digest. That was not
enough to drive the next gameplay pass.

## Sources Checked

| Source | Quality | What it proves |
|---|---|---|
| Backrooms Wiki, Level 0 - Threshold, https://backrooms-wiki.wikidot.com/level-0 | community lore / canonical fan expectation | Level 0 is a randomly segmented labyrinth with monotony, layout shifts, blackout zones, red rooms, arches, pillars, isolation, and unreliable sound. |
| Wikipedia, The Backrooms, https://en.wikipedia.org/wiki/The_Backrooms | broad cultural summary | Backrooms is known as a liminal-space creepypasta: empty rooms, yellow walls/carpet, fluorescent lighting, no-clipping, levels/entities. |
| Steam, Escape the Backrooms, https://store.steampowered.com/app/1943950/Escape_the_Backrooms/ | successful commercial comparator | Popular Backrooms game sells exploration of many lore levels, co-op, getting lost, entity mechanics, minimal UI, atmosphere. |
| Steam, Inside the Backrooms, https://store.steampowered.com/app/1987080/Inside_the_Backrooms/ | successful/visible comparator with mixed recent feedback | Co-op puzzle/exploration expectations: identical rooms, items, puzzles, entities, danger escalation. Recent reviews are mixed, so complexity/roughness is a risk. |
| Steam, The Complex: Expedition, https://store.steampowered.com/app/2172260/The_Complex_Expedition/ | single-player comparator | Slow-burn atmospheric exploration, VHS/found-footage view, being alone and lost in a seemingly infinite maze. |
| Steam, POOLS, https://store.steampowered.com/app/2663530/POOLS/ | adjacent liminal-space success | No monsters can still be scary if navigation, sound, darkness, tight spaces, and changing environments create dread. |
| Steam, Anemoiapolis: Chapter 1, https://store.steampowered.com/app/1522960/Anemoiapolis_Chapter_1/ | adjacent liminal/backrooms-like success | Explicitly promises procedural labyrinths, dynamic environments, doors that may not stay where they were, and feeling lost/confused. |
| PC Gamer, "Steam is flooded with liminal spaces games..." and Backrooms Steam week pieces | market overview / curation warning | There are many low-effort clones; the standouts are the ones with atmosphere, technical stability, exploration, and identity. |

## Observed Facts

1. Level 0 is not just a hallway. It is described as a labyrinth of rooms,
   halls, stairs, and architectural variations. Monotony is part of the horror,
   but it is monotony across a confusing space, not a straight line.
2. The iconic Level 0 palette matters: sickly yellow wallpaper, wet/mildewed
   carpet, fluorescent buzz, low retail/office-like interiors, no ordinary
   windows/furniture, and repeating partitions.
3. Player expectation includes getting lost. Successful references repeatedly
   mention maze-like traversal, exits, shifting entry points, side spaces,
   unreliable landmarks, and "going in circles".
4. Entities are optional for Level 0 and often uncertain. The stronger base
   fantasy is isolation, paranoia, sound, distance, and not trusting the layout.
5. Reviews praise atmosphere and discovery, but punish sparseness when there is
   too little to do. `Escape the Backrooms` is commercially strong, while
   GameSpot's summarized critique calls its loop sparse despite liking the look
   and level discovery. `Inside the Backrooms` has positive lifetime English
   reviews but mixed recent reviews, which warns against piling on rough
   puzzles/content without polish.
6. `POOLS` proves there can be no monster and still strong dread: no UI, no
   dialogue, no music, environmental sound, darkness, tight spaces, navigation
   uncertainty, and changing rooms carry the experience.

## Similar Games And Review Takeaways

| Game | Current public signal checked | What players/reviewers seem to reward | Design warning |
|---|---:|---|---|
| Escape the Backrooms | Steam English reviews Very Positive, 89% of 40,350; recent 84% of 6,913 | Many levels, lore breadth, co-op, entity rules, getting lost together, proximity voice | Single-player slice cannot copy co-op comedy; needs stronger solo paranoia. |
| Inside the Backrooms | Steam English reviews Mostly Positive, 79% of 10,185; recent Mixed, 50% of 146 | Puzzles, items, iconic entities, co-op exploration, varied levels | Recent mixed response implies rough puzzles/EA friction can sour players. Avoid overcomplicated puzzle systems now. |
| The Complex: Expedition | Steam English reviews Very Positive, 88% of 1,502; recent 90% of 151 | Slow-burn, VHS, being alone, story expedition, shifting entry points, maze | Needs atmosphere and pacing; not enough to place a monster in a hallway. |
| POOLS | Steam English reviews Overwhelmingly Positive, 95% of 3,270; recent 95% of 148 | Oppressive exploration, sound, darkness, tight spaces, changing rooms, no UI clutter | A "walking only" game can bore some players, so our fuse/exit objective should remain, but the path must feel uncertain. |
| Anemoiapolis | Steam overall Very Positive, 1,136 reviews | Mundane horrors, dynamic environments, procedural labyrinths, doors/pathways that change | This is the closest mechanic lesson: changing layout and unreliable return paths are core, not optional flavor. |

## Current Build Mismatch

- It has yellow walls, lights, fog, silhouette, and audio, but the playable
  structure is still mostly a straight corridor.
- Route choices are lane prompts, not real exploration. The player does not
  discover side rooms or form/lose a mental map.
- The fuse objective is spatially too certain: "go forward, turn back" reads as
  a haunted hallway, not "I am trapped in a place that rearranges itself".
- The stalker/chase has become louder than the Backrooms fantasy. Level 0 should
  first make the player distrust space, then use threat as pressure.
- Current UI over-instructs route direction. Backrooms should provide enough
  goal clarity to play, but the world itself should create doubt.

## Borrow / Avoid / Copy Risk

Borrow:

- Randomly segmented rooms, side passages, dead ends, arches/pillars, blackout
  pockets, red-room danger cues, repeating walls, unreliable return path.
- Sound-based orientation: hum seems ahead, then behind; buzzing grows/vanishes.
- Minimal but clear objective: find fuse / find exit, with route uncertainty in
  between.
- Slow-burn paranoia before chase: isolation, empty space, no trustworthy map.

Avoid:

- Directly copying named extended-lore entities, Kane Pixels story/Async, or
  exact rooms from specific games.
- A huge procedural maze before first playable proof; use a small authored
  labyrinth first.
- Co-op-specific mechanics in the solo prototype.
- Puzzle clutter before the player believes the place.

Copy risk:

- Do not use exact Level 0 text, named entities, or recognizable screenshots.
- Use "inspired by Level 0 expectations" rather than cloning wiki canon.

## Next Native Proof

T0009 should prove one thing: the player is no longer in a straight hallway.

Minimum proof:

- Native screenshot of a fork/side-room view where multiple yellow passages are
  visible.
- Native screenshot of a loop/dead-end/blackout pocket or red-room warning.
- DevAPI report showing the player can move into a side route, loop/return or
  get displaced, and still pursue the fuse/exit objective.
- Product gate language must explicitly answer: "why does this feel like
  Backrooms, not a corridor chase?"
