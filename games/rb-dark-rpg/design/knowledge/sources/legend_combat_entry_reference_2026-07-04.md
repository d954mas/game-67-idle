---
type: Project Source Notes
title: Legend Combat Entry Reference
description: Source notes for Legend: Legacy of the Dragons combat-entry UX and what rb-dark-rpg should borrow.
tags: [project, references, combat, uiux, autobattle]
timestamp: 2026-07-04T00:00:00Z
game_id: rb-dark-rpg
status: draft-source
source_quality: mixed
checked: 2026-07-04
---

# Legend Combat Entry Reference

Scope: deconstruct how `Legend: Legacy of the Dragons` / `War of Dragons`
frames combat entry, then adapt the entry ritual for `rb-dark-rpg` pure
autobattle.

This note is about flow and readability. It does not license copying assets,
layout, icons, names, combat formulas, or live-service systems.

## Source Matrix

| Source | Link | Quality | Checked | Proves | Does Not Prove |
|---|---|---|---|---|---|
| Official Bestiary | `https://warofdragons.com/info/library/index.php?obj=cat&id=45` | official library | 2026-07-04 | PvE enemies are presented as location-bound creatures with level, life, damage, reward, and possible loot. | The exact live click path from map to PvE fight. |
| Official hunt UI atlas config | `https://warofdragons.com/images/data/canvas/ui/hunt.json` | official runtime data | 2026-07-04 | The hunt/location layer contains combat-action UI identifiers such as `big_fight_btn` and `left_corner_icon_attack`, alongside non-combat actions. | The exact tooltip text or whether ordinary PvE uses a confirmation modal. |
| Official training fight page | `https://warofdragons.com/first_battle.php` | official runtime page | 2026-07-04 | Training combat is presented as a dedicated `CanvasFirstBattle` screen rather than as a tiny inline widget. | The normal PvE entry flow for an authenticated session. |
| Official training fight UI atlas config | `https://warofdragons.com/images/data/canvas/ui/first_battle.json` | official runtime data | 2026-07-04 | The battle screen contains HP/MP fills, center battle controls, spell slots, level battle buttons, and victory/defeat/wait popup surfaces. | That rb-dark-rpg should copy active battle controls. |
| Official Super-Blows library | `https://warofdragons.com/info/library/index.php?obj=cat&id=377` | official library | 2026-07-04 | Legend combat depth includes active blow sequences and effects. | Suitability for a first-slice pure autobattle game. |

## Observation Ledger

- `observed` Legend separates location/hunt interaction from the battle screen.
  The hunt UI config includes attack/fight affordances, while the training fight
  page instantiates a separate battle canvas.
- `observed` Enemy information in the official bestiary is pre-combat-readable:
  level, life, damage, reward, location, and potential rewards.
- `observed` The battle UI is active and dense: HP/MP, battle center controls,
  spell surfaces, and state popups.
- `observed` Super-Blows are an active combat layer, not a passive autobattle
  layer.
- `inferred` For `rb-dark-rpg`, the best borrow is the entry ritual:
  location target -> clear combat affordance -> focused battle screen.
- `unknown` Public unauthenticated sources do not prove the exact ordinary PvE
  click path, tooltip wording, or whether a normal monster attack shows a
  confirmation modal.
- `unknown` No official source found in this pass proves that Legend's core PvE
  battle is an autobattle. Treat any "auto" language as automatic screen
  switching unless a stronger source is found.

## Borrow

- Combat starts from the current place or a visible local target, not from a
  global bottom-nav button.
- Before combat, show the opponent as a concrete encounter with level/threat,
  HP, damage, expected reward, and location context.
- After the player commits, switch to a focused battle screen.
- Keep the fight result explicit: victory/defeat surface, reward, and next
  quest step.

## Avoid

- Do not copy Legend's active hit zones, blocks, spells, super-blow input,
  PvP/battlefield queue, or MMO density into the first `rb-dark-rpg` fight.
- Do not make the first fight launch from a generic `Autoboy` bottom-nav button.
- Do not claim Legend validates pure autobattle. It validates a browser-RPG
  combat-entry ritual and stat/equipment depth; the autobattle decision is
  `rb-dark-rpg`'s own scope choice.

## RB Dark RPG Decision

Accepted for first slice:

1. `Place` / location context exposes the threat.
2. Player selects `Падальщик у ворот` or the highlighted gate threat.
3. A pre-fight card opens with opponent stats, threat label, reason line, and
   rewards.
4. Primary action is `В бой`.
5. The battle opens as a focused pure-autobattle screen.
6. Combat has no active skills, no manual block, no zone targeting, and no
   consumable timing in v1.
7. Result returns the player to the guard/quest flow.

## Evidence Gaps

- Need an authenticated live capture if we later want exact Legend hunt-screen
  microcopy or modal behavior.
- Need native `rb-dark-rpg` screenshots after implementation to verify that the
  entry flow reads like an old browser RPG without inheriting Legend's UI
  clutter.
