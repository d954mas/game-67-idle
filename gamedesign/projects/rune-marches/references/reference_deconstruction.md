---
type: Reference Deconstruction
title: Rune Marches Reference Deconstruction
description: Central reference gate for Skyrim, Daggerfall, The Quest, and Poki target constraints.
tags: [references, skyrim, daggerfall, the-quest, poki]
timestamp: 2026-06-13T00:00:00Z
---

# Rune Marches Reference Deconstruction

## Reference Lock

- Study mode: `central deconstruction`.
- Reference question: how to translate "casual Skyrim for PC and web/mobile"
  into a small original native-first RPG slice with open-world fantasy,
  readable magic/progression, quests, and Poki-style first-session clarity.
- Durable doc path:
  `gamedesign/projects/rune-marches/references/reference_deconstruction.md`.
- Source packet status: partial but usable for the first native placeholder
  slice. Official/store/reference pages and provisional screenshot evidence
  are captured; raw timestamped gameplay video still needs to be gathered
  before final UI, economy pacing, or final art implementation.
- Current native capture: `tmp/rune_marches/current_template_mismatch.png`
  captured the pre-implementation template mismatch; `tmp/rune_marches/native_first_slice.png`
  captured the first Rune Marches placeholder slice after implementation.
- No-coding/no-final-art boundary: do not implement reference-driven combat,
  economy pacing, or final art beyond the documented first slice until this doc
  has stronger frame evidence or the user approves a narrow exception.
- Expected native proof: first screen shows map, stats, primary scout action,
  combat/quest status, upgrade lock/unlock, and readable reward feedback.

## Reference Digest

- Sources checked: user brief, Bethesda/Steam pages for Daggerfall, Steam page
  for The Quest, Wikipedia gameplay summaries for Skyrim/Daggerfall/The Quest,
  screenshot/image search evidence for Skyrim/Daggerfall/The Quest, Poki
  developer page scrape attempt.
- Observed facts:
  - Skyrim is an open-world action RPG where players can freely roam and
    postpone the main storyline.
  - Daggerfall emphasizes a huge world, many towns/dungeons, guild/reputation,
    role-playing skill progression, spell creation, and multiple end choices.
  - The Quest is first-person, open-world, grid-based, turn-based, and was
    designed across PDA/mobile and PC contexts.
  - The Quest's Steam page foregrounds hand-drawn open world, grid movement,
    turn-based combat, quests, skills, magic, crafting, persuasion, and
    interactable locations.
  - Skyrim's mature content warning is a copy-risk and audience-risk signal for
    broad casual web/mobile testing.
- Current-build mismatch before implementation: native runtime was `Game Seed`,
  a shape cycling template with no RPG map, quest, combat, magic, progression,
  or FTUE.
- Current-build status after first implementation: native runtime has a
  placeholder Rune Marches map, scout action, Mire Wisp encounter, rewards,
  Spark Ward upgrade, Old Bell Tower unlock, DevAPI action path, and screenshot
  proof. Final art/UI reference readiness remains open.
- Borrow: open-world permission, local quest hooks, visible progression,
  magic-as-build identity, map unlocks, optional side objectives.
- Avoid: Elder Scrolls lore/names/dragons/shouts, mature gore/sexual themes,
  dense character creation before play, huge procedural world claims, mouse
  gesture combat, complex inventory first.
- Copy-risk: direct Dragonborn/Numidium/Kings/Lysandus equivalents, Skyrim UI
  compass/stat layout, The Quest exact grid/UI composition, Daggerfall faction
  names and spell-maker shape.
- Native screenshot/scenario proof: `tmp/rune_marches_scenario.py` verifies
  `Scout Road -> fight Mire Wisp -> earn reward -> buy Spark Ward I -> unlock
  Old Bell Tower -> save/load`, with screenshot
  `tmp/rune_marches/native_first_slice.png`.

## Source Matrix

| Source | Quality | Checked | Proves | Does not prove |
| --- | --- | --- | --- | --- |
| User brief in current goal | user-provided | 2026-06-13 | Desired fantasy, platforms, references, Poki test goal, importance of visual/gameplay/balance/FTUE | Exact art style, monetization, target age |
| Bethesda Daggerfall page, https://elderscrolls.bethesda.net/en/daggerfall | primary/studio | 2026-06-13 | Daggerfall official story framing and PC platform | Moment-to-moment UI or first 60 seconds |
| Steam Daggerfall, https://store.steampowered.com/app/1812390/The_Elder_Scrolls_II_Daggerfall/ | marketplace/store | 2026-06-13 | Current store framing, free availability, official description, mature content note | Full gameplay loop or onboarding |
| Wikipedia Daggerfall, https://en.wikipedia.org/wiki/The_Elder_Scrolls_II:_Daggerfall | secondary article | 2026-06-13 | Broad world scale, guild/reputation, spell creation, role-playing progression | Primary frame evidence |
| Steam The Quest, https://store.steampowered.com/app/428880/The_Quest/ | marketplace/store | 2026-06-13 | Store-facing promise: hand-drawn open world, grid movement, turn combat, quests, skills, magic, interactable locations | Exact first-minute tutorial timing |
| Wikipedia The Quest, https://en.wikipedia.org/wiki/The_Quest_(2006_video_game) | secondary article | 2026-06-13 | First-person open world, turn combat, grid movement, mobile/PC history | Primary screenshots or first input |
| Wikipedia Skyrim, https://en.wikipedia.org/wiki/The_Elder_Scrolls_V:_Skyrim | secondary article | 2026-06-13 | Open-world freedom, character advancement, quests, first/third person, free roaming | Exact UI or combat frame evidence |
| Steam Skyrim age gate, https://store.steampowered.com/app/489830/The_Elder_Scrolls_V_Skyrim_Special_Edition/ | marketplace/store | 2026-06-13 | Mature content warning to avoid for casual/Poki-facing tone | Gameplay details due age gate |
| Poki Developers, https://developers.poki.com/ | primary/studio | 2026-06-13 | Target portal exists; user specifically named Poki audience | Page scrape returned no usable lines; requirements need manual/API follow-up |

## Observation Ledger

| Beat | Source | Visible fact | Player action | Visible response | Reward/UI feedback | Inference |
| --- | --- | --- | --- | --- | --- | --- |
| O1 | Skyrim summary | Open world with free travel and optional main story pacing | Player chooses where to go | World permits postponing main quest | Character improves through skills/quests | Borrow freedom as map choice, not full 3D simulation |
| O2 | Daggerfall summary | Very large world with towns, dungeons, guilds, religions | Player explores or joins groups | Reputation/rank changes how world views player | Faction status becomes progression | Borrow static authored factions later; first slice uses reputation seed |
| O3 | Daggerfall summary | Spell creation/enchantment exist | Player invests in magic systems | Magicka cost scales with effects | Magic customization becomes identity | Borrow upgradeable spell verbs, not full spell maker first |
| O4 | The Quest store/wiki | First-person open world, grid movement, turn combat | Player moves/explores and enters turn combat | Combat resolves in readable turns | Quests and skills frame progress | Borrow readable turn/check combat for mobile friendliness |
| O5 | The Quest store | Store promise includes magic, potions, persuasion, locks, houses, dungeons | Player chooses activities in a compact RPG world | Locations offer many small interactions | Optional quests support exploration | Borrow activity variety over huge first map |
| O6 | Skyrim Steam age gate | Store warns about mature content | Player/parent must pass age gate | Friction before access | Mature content limits broad casual suitability | Avoid mature tone for Poki test |

## Provisional Reference Evidence Board

This board is enough to support a native placeholder slice, not final art or
deep pacing. It must be upgraded with raw gameplay video timestamps before
claiming final reference readiness.

| Frame | Source | Covers | Visible observation | Translation |
| --- | --- | --- | --- | --- |
| F1 | Daggerfall gameplay montage, image search result from CyberPowerPC | First-person encounter / combat HUD | Dungeon view keeps the world large while the bottom HUD exposes bars and action icons. | Rune Marches can use a clear map/action panel instead of full inventory density. |
| F2 | Daggerfall inventory screenshot, image search result from Superjump Magazine | Progression/equipment UI | Inventory is dense, slot-heavy, and text-heavy. | Avoid first-session inventory; defer equipment lists. |
| F3 | Daggerfall Unity retro HUD, image search result from Daggerfall Workshop | Exploration HUD / status bars | Retro HUD keeps health/magic/status and navigation visible while exploring. | Keep HP/mana/silver visible during exploration. |
| F4 | The Quest Steam screenshot | First screen / town exploration | First-person town view has a right-side minimap, bars, spell/item icons, and large movement arrows. | Use big touch targets and compact resources for mobile readability. |
| F5 | The Quest App Store combat screenshot | Combat response / enemy state | Enemy name and health bars are prominent while weapons/spells remain visible. | First combat needs enemy HP, player action buttons, and direct damage feedback. |
| F6 | The Quest TouchArcade battle screenshot | Combat blocked/choice state | Enemy label, minimap, HP/mana, spell icons, and movement arrows are all visible together. | Use turn/check combat with few actions, not real-time dexterity. |
| F7 | Skyrim screenshot, image search result from Coco Wang UX case | First-person combat/status | Skyrim spreads compass and resource bars over a scenic world view. | Borrow scenic openness, but cluster casual resources more tightly. |
| F8 | Skyrim quest prompt screenshot, image search result from Nexus Mods | Quest feedback / spell unlock | Quest text and learned spells appear over the world without leaving exploration view. | Reward feedback can be overlay/toast, not a separate menu. |

## Systems Extraction

- Open world is translated to a static node/region map with authored
  locations, visible locks, and optional objectives.
- Skyrim-like freedom becomes "main quest can wait after the first loop";
  the first slice still needs one clear FTUE path.
- Daggerfall-like scale becomes named regions, factions, reputation, and many
  future roads, not procedural geography in this iteration.
- The Quest-like readability becomes turn/check encounters with big touch
  buttons and minimal simultaneous stats.
- Magic progression begins as one upgradeable spell with visible map impact.
- Mature/dense elements are removed for casual web/mobile testing.

## Translation Gate

Implementation may use the following scoped translation for a native
placeholder slice:

- first slice has 3 locations, 1 enemy, 1 spell upgrade, 1 main quest, 1 side
  quest choice;
- map is authored and static;
- combat is turn/check based;
- first screen prioritizes one action and one next goal;
- RPG depth is represented by XP, silver, rune spark, spell level, HP, and
  mana only.

Implementation must not claim:

- full Skyrim-scale world;
- Daggerfall-scale faction simulation;
- final Poki readiness;
- final reference study completion.

## Remaining Evidence Gaps

- Need timestamped raw gameplay observations for first screen, first input,
  response, reward, upgrade UI, and blocked state across the named references.
- Provisional image-search frames must be replaced or backed by durable source
  captures before final art/UI claims.
- Need Poki developer/platform requirements captured in a durable source note
  before final web/mobile acceptance.
- Need current native screenshot after the first runtime pass.
