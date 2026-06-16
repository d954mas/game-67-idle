---
id: T0066
title: "Critter Corral teachability: text, clear upgrades, FTUE, difficulty display"
status: done
epic: E004
priority: P1
tags: []
created: 2026-06-16
updated: 2026-06-16
---

## What

## Done when

- [ ]

## Open questions

## Log

- 2026-06-16: Increment 5 (teachability). Added TEXT rendering (nt_text_renderer + slug_text material + UI font packed into the atlas; draw_text helper). Upgrade cards now show NAME + plain description + Level N/3 + key hint -> fully clear. FTUE onboarding (first run only, 3 beats, advance-by-doing): "move to herd" -> "bring red critters into the red pen" (arrow cue) -> "pen them all to finish the level". Level/difficulty shown: "Level N" HUD, "Level N cleared!", title "CRITTER CORRAL", "New color: X!" callout. Builds clean -Werror. Captures (audit pass): corral_upgrade2.png (clear cards), corral_ftue.png (in-field instruction), corral_level.png. Directly fixes lead feedback (unclear upgrades / no tutorial / difficulty not shown).
