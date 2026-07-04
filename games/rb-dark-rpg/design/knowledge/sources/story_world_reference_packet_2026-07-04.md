---
type: Project Source Notes
title: Story And World Reference Packet
description: Source notes for rb-dark-rpg story, world, lore, characters, and locations.
tags: [project, references, story, world, lore]
timestamp: 2026-07-04T00:00:00Z
game_id: rb-dark-rpg
status: draft-source
source_quality: mixed
checked: 2026-07-04
---

# Story And World Reference Packet

Scope: support `Дракон не вернулся` as a compact, 2D illustrated browser RPG
about ordinary seekers in a borderland after the Great Dragon disappears.

This packet supports story, world, lore, characters, locations, and tone. It is
not a gameplay implementation deconstruction: no raw gameplay footage or current
native/browser capture was analyzed in this pass.

## Source Matrix

| Source | Link/Path | Quality | Checked | Proves | Does Not Prove |
|---|---|---|---|---|---|
| V0 concept/GDD draft | `games/rb-dark-rpg/design/knowledge/sources/v0_concept_gdd_draft.md` | user-provided | 2026-07-04 | Core pitch, title candidates, Act I structure, Last Post, old mill, Black Sun clues, FITGAME-like browser RPG direction. | External market fit, final visual style, production feasibility. |
| V0 concept idea | `games/rb-dark-rpg/design/knowledge/v0_concept_idea.md` | user-provided distilled source | 2026-07-04 | Current distilled direction: `Дракон не вернулся`, 2D illustrated browser RPG, Act I `Лишний свидетель`, hub-map-location-autobattle loop. | Full character bible, final act structure, runtime implementation contract. |
| DragonFable official pages | `https://www.dragonfable.com/` and `https://www.dragonfable.com/About` | official/store/trailer-style | 2026-07-04 | Browser-era fantasy RPG packaging: animated RPG, ongoing story quests/events, lands/factions, allies/foes, gear/classes, approachable fantasy tone. | Exact current gameplay timing, modern browser UX, anything to copy directly. |
| Legend: Legacy of the Dragons / War of Dragons | `https://w1.dwar.ru/` and `https://warofdragons.com/` | official live site | 2026-07-04 | Old browser MMORPG identity around dragons, factions/clans/ranks, persistent world scale, strong "online RPG about dragons" signal. | Single-player pacing, clean jam scope, safe onboarding, non-MMO structure. |
| The Witcher 3 official page | `https://www.thewitcher.com/us/en/witcher3` | official/store/trailer-style | 2026-07-04 | Professional monster-slayer fantasy, preparation before danger, dark-fantasy world regions, complex choices/consequences. | Low-complexity browser flow, non-adult tone, exact contract structure for this game. |
| Roadwarden Steam page | `https://store.steampowered.com/app/1155970/Roadwarden/` | official store | 2026-07-04 | Borderland professional fantasy: guard routes, connect settlements, collect hints, investigate a dangerous region, story through locations and choices. | Visual style target, autobattle, old browser RPG UX. |
| Fallen London official page | `https://www.failbettergames.com/games/fallen-london` | official press/game page | 2026-07-04 | Browser-based dark narrative, secrets as currency, strange city institutions, bite-size story discovery, choices with consequences. | Combat, 2D RPG layout, suitable level of prose for jam. |
| Battle Brothers Steam page | `https://store.steampowered.com/app/365360/Battle_Brothers/` | official store | 2026-07-04 | Mercenary contract fantasy, fractured world, ordinary recruits, settlement troubles, risk/reward contracts, fame/fortune framing. | Quest UI, browser pacing, light jam scope. |
| Darkest Dungeon Steam page | `https://store.steampowered.com/app/262060/Darkest_Dungeon/` | official store | 2026-07-04 | Gothic adventuring pressure, flawed heroes, hamlet/twisted locations, stress/famine/disease/dark as location texture. | Target mechanics; this project should not import stress/permadeath complexity for v1. |

## Observations

- `user-provided` V0 frames the jam theme as: the missing `you` is the Great
  Dragon, not the player. The player is an ordinary seeker who begins with
  practical work, not prophecy.
- `user-provided` V0 fixes Act I as `Лишний свидетель`: a mundane mill contract
  turns into a Black Sun conspiracy clue.
- `observed` DragonFable presents browser fantasy as approachable story quests,
  new lands, factions, allies/foes, gear, and ongoing events rather than a
  realistic simulation.
- `observed` War of Dragons' official live pages strongly signal old browser
  RPG scale through players, clans, ranks, races, and dragon branding. This is a
  useful warning for what to avoid in a jam: MMO load, social systems, and rank
  pressure.
- `observed` The Witcher 3 official page frames dark fantasy through a
  professional monster slayer, preparation, regions, and choices with
  consequences. For this game, borrow professionalism and investigation, not
  Geralt, open-world scale, or adult presentation.
- `observed` Roadwarden frames the player as a hired professional connecting
  isolated settlements, securing roads, gathering information, and investigating
  a dangerous peninsula. This maps cleanly to a seeker in the Ash Border.
- `observed` Fallen London presents browser narrative as secret-rich places,
  reputation, professions, short sessions, and dark-humorous institutional
  weirdness. Borrow secrets and place identity; keep prose shorter.
- `observed` Battle Brothers anchors mercenary fantasy in contracts, pay,
  ordinary recruits, settlement problems, and risk/reward decisions.
- `observed` Darkest Dungeon's store page ties gothic adventure to flawed heroes,
  stressful expeditions, twisted forests, warrens, crypts, and pressure from
  darkness. Borrow location pressure and readability of dread; avoid v1 stress
  systems.

## Observation Ledger

| Beat | Source | Explicit / visible fact | Design meaning for this game | Evidence label |
|---|---|---|---|---|
| 1 | V0 draft / V0 idea | The missing `you` is the Great Dragon; the player is an ordinary seeker. | The story should not start with chosen-one identity or dragon inheritance. | user-provided |
| 2 | V0 draft / V0 idea | Act I is `Лишний свидетель`; the first contract is a mill/grain job. | The plot should begin with civic survival and only then reveal conspiracy. | user-provided |
| 3 | V0 draft | The mill basement contains Black Sun signs, cut grain sacks, a dead cultist, a burned chain bracket, and an order scrap. | The first clue set should be physical, specific, and readable as logistics. | user-provided |
| 4 | DragonFable official pages | Browser fantasy packaging emphasizes story quests, lands, factions, gear, allies, foes, and events. | Use screen-led quest packaging, not simulation-heavy free roaming. | observed |
| 5 | War of Dragons official pages | The live old-browser RPG surface foregrounds clans, ranks, races, and dragon-branded MMO scale. | Treat MMO scale as copy-risk; keep jam version single-player and compact. | observed |
| 6 | The Witcher 3 / Roadwarden / Battle Brothers official-store pages | Professional fantasy roles are framed through contracts, preparation, routes, settlements, risk, reward, and information. | The seeker fantasy should be practical work under danger, not epic destiny. | observed |
| 7 | Fallen London / Darkest Dungeon official-store pages | Dark worlds can be carried by institutions, secrets, flawed people, twisted locations, and pressure from darkness. | Put lore into places, objects, officials, and journal clues before exposition. | observed |

## Application To RB Dark RPG

### Borrow

- A world problem should become practical friction first: closed gates, grain
  shortage, paid permits, dangerous roads, missing workers.
- The player identity should be professional and low-status: seeker, mercenary,
  witness, courier of dangerous proof.
- Locations should each carry three layers: mundane need, visible consequence of
  the Dragon's absence, and one clue toward the conspiracy.
- The journal should be a narrative engine: quests answer "what next", clues
  answer "what does this mean".
- The Dragon should function as absent infrastructure: protector, border seal,
  public myth, and political secret.

### Avoid

- MMO systems: clans, PvP, rankings, social raids, multiple monetization
  currencies, faction grind.
- Open-world sprawl and long travel before the first story hook.
- Pure grimdark punishment loops, permadeath, stress meters, or adult bleakness
  in the first jam act.
- Too much Fallen London-style prose density before the player has acted.
- Copying exact contract, faction, dragon, city, or cult structures from any
  reference.

### Copy-Risk

- Do not copy DragonFable's DragonLord/chosen-hero framing.
- Do not copy War of Dragons' two-race dragon war or MMO faction ladder.
- Do not copy The Witcher names, monster-hunter organization, signs, or exact
  contract investigation beats.
- Do not copy Fallen London's proper nouns, surreal institutions, or premium
  story cadence.
- Do not copy Darkest Dungeon's Hamlet/Ancestor/estate premise or stress system.
- Do not copy Battle Brothers' company management or procedurally generated
  mercenary roster.

## Current Design Mismatch

- `concept.md`, `gdd.md`, `data/core_loop.json`, and `data/ui_flow.json` still
  contained the earlier starter direction: ruined gate, WASD movement, native
  desktop validation, and encounter cues.
- The v0 source instead points to a 2D illustrated browser RPG with screen flow:
  hub -> map -> location -> autobattle -> reward -> clue/journal -> hub.
- No current native/browser screenshot was captured in this pass, so this is a
  design-doc mismatch audit, not a build mismatch audit.

## Next Proof

- Design proof: updated `concept.md`, `gdd.md`, and structured loop/UI data all
  name the same title, Act I, screen flow, first locations, and story role.
- Later visual proof: one accepted Last Post hub fake shot or mock screen with
  gate guard, contract board, Dragon memorial, map gate, and locked secondary
  objects.
- Later gameplay proof: first playable capture showing quest journal -> map ->
  old mill -> autobattle result -> first clue journal update.

## Reference Digest

- Mode: story/world/lore reference packet; not gameplay implementation-ready.
- Sources checked: V0 draft, V0 distilled idea, DragonFable official pages, War
  of Dragons official pages, The Witcher 3 official page, Roadwarden Steam page,
  Fallen London official page, Battle Brothers Steam page, Darkest Dungeon Steam
  page.
- Observed facts: V0 fixes the absent Dragon premise; V0 fixes Act I as an
  ordinary mill contract becoming conspiracy evidence; official refs support
  screen-led browser fantasy, professional contract fantasy, and dark
  institution/place-driven storytelling.
- Current-design mismatch: earlier design docs still named WASD/ruined-gate
  exploration, while the active v0 direction is 2D browser hub/map/location
  flow.
- Borrow: practical civic failure, ordinary seeker role, clue journal, compact
  screen-led quest chain.
- Avoid: MMO overload, chosen DragonLord fantasy, open-world sprawl, adult
  grimdark, stress/permadeath systems.
- Next proof: accepted Last Post hub visual proof, then quest journal -> map ->
  old mill -> autobattle -> clue journal capture.
