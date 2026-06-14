---
type: Project Wiki
title: Rune Marches
description: Casual open-world fantasy RPG for native PC and web/mobile.
tags: [rpg, open-world, casual, pc, web, mobile]
timestamp: 2026-06-13T00:00:00Z
---

# Rune Marches

`Rune Marches` is the active game iteration for the casual Skyrim-like RPG
goal. It is a new original fantasy setting, not an Elder Scrolls derivative.

## Source-Of-Truth Order

1. `AGENTS.md`
2. `gamedesign/projects/rune-marches/handoff_status.md`
3. `gamedesign/projects/rune-marches/concept.md`
4. `gamedesign/projects/rune-marches/gdd.md`
5. `gamedesign/projects/rune-marches/data/balance.json`
6. `gamedesign/projects/rune-marches/data/combat.json`
7. `gamedesign/projects/rune-marches/data/ui_flow.json`
8. `gamedesign/projects/rune-marches/data/asset_manifest.json`
9. `gamedesign/projects/rune-marches/references/reference_deconstruction.md`
10. `gamedesign/projects/rune-marches/playtest/first_session_poki_packet.md`
11. `gamedesign/projects/rune-marches/data/playtest_telemetry.json`
12. `gamedesign/projects/rune-marches/game_implementation_plan.md`

## Current Gate

Status: `design gate partial`.

The concept, first playable slice, and machine-readable contracts exist. The
reference deconstruction is intentionally marked partial until raw gameplay
frames or screenshots are gathered for the named references.

## Next Runtime Target

Native PC first. Web/mobile is in scope because the user explicitly requested
PC and web/mobile, but the native slice remains the first playable proof.

## First Audience-Test Packet

- Packet:
  `gamedesign/projects/rune-marches/playtest/first_session_poki_packet.md`
- Event map:
  `gamedesign/projects/rune-marches/data/playtest_telemetry.json`
- Platform notes:
  `gamedesign/projects/rune-marches/sources/poki_platform_notes.md`
- Native probe:
  `tools/playtest/rune_marches_probe.py`

This packet is ready for internal native-proxy validation with runtime telemetry
through `game.rune.telemetry`. It does not claim Poki submission readiness and
does not reactivate the web/mobile build lane.
