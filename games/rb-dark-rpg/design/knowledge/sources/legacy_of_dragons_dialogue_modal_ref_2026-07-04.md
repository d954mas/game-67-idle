---
type: Source Note
title: Legacy Of Dragons Dialogue Modal Reference
description: Reference deconstruction for first-slice dialogue window shape.
tags: [project, references, ui, dialogue]
game_id: rb-dark-rpg
status: draft-source
source_quality: mixed
---

# Legacy Of Dragons Dialogue Modal Reference

Mode: central deconstruction for the first runtime dialogue window.

## Source Matrix

| Source | Link | Quality | Useful evidence | Gap |
|---|---|---:|---|---|
| User reference direction | current chat, 2026-07-04 | direct design input | Dialogue should be a full modal window over the scene, not a small side panel. | Not a pixel screenshot by itself. |
| DWAR Social-network Game UI project | https://www.behance.net/gallery/70465837/DWAR-Social-network-Game | portfolio/source-adjacent | Shows a central parchment-like quest panel with NPC portrait, task text, rewards/actions, and surrounding HUD pushed behind the main panel. | Social-game UI variant, not necessarily the exact live browser client. |
| Legend: Legacy of the Dragons overview | https://ru.wikipedia.org/wiki/Легенда:_Наследие_Драконов | secondary | Confirms browser MMORPG/RPG context, quests from NPCs, and goal guidance as core UX expectations. | Does not document exact dialogue layout. |

## Observations

- `observed` The useful UI grammar is not a speech bubble. It is a blocking
  modal state: backdrop/scene dim, large central panel, NPC/quest identity, long
  text, then explicit player choice/action.
- `observed` The reference reads as browser RPG UI: parchment/wood frame,
  visible borders, dense but legible text, and persistent world/HUD behind the
  modal rather than a cinematic full-screen cutscene.
- `inferred` The modal should support quest actions and rewards later, so the
  layout needs stable zones: NPC identity, body text, current objective/status,
  and choices.

## Borrow

- Full-scene dimming backdrop that blocks click-through.
- Large centered panel occupying most of the readable viewport.
- Left NPC identity/portrait column and right dialogue/task column on landscape.
- Warm parchment/brown RPG palette for the dialogue surface.
- Choices as clear bottom actions inside the modal.

## Avoid

- Copying exact DWAR ornaments, icons, faction marks, or button shapes.
- Small right-side bubble that competes with the hub scene.
- Modern blue HUD button styling as the primary dialogue surface.
- Dialogue text that depends on hover hints or hidden UI.

## Current-Build Mismatch

- Before this pass, `rb-dark-rpg` displayed the guard dialogue as a narrow
  bottom-right panel over the scene. It did not dim/block the whole view and did
  not feel like the full modal quest/dialogue state from the reference.

## Implementation Direction

- Runtime dialogue should use the engine modal helper for backdrop/input gating.
- First slice can use a stylized placeholder NPC panel until final portrait art
  is wired from `asset_manifest.json`.
- The modal may still be data-light in v1, but its visual contract should
  already match the reference shape.
