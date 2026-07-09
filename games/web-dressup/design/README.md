---
type: Game Design Guide
title: Web Dressup Design
description: Design source-of-truth map for web-dressup.
tags: [game-design]
game_id: web-dressup
status: draft
---

# Web Dressup Design

This folder is the game-owned design source of truth for **web-dressup**
(player-facing working title: **Переодевалка**).

## Map

- `concept.md` — hook, audience, pillars, no-go list.
- `gdd.md` — implementation-facing first playable (Dress Room).
- `knowledge/` — private game knowledge base.
- `knowledge/sources/` — reference packets and source notes.
- `data/core_loop.json` — try-on loop.
- `data/ui_flow.json` — screens and first path.
- `data/asset_manifest.json` — starter art needs.

## Current gate

**Concept accepted** (`concept.md`): Poki 2D freeplay lab + optional Fake Show;
no map, no MP, no forced timer. Poki competitor research:
`knowledge/sources/2026-07-09-poki-competitors.md`.

Next: art style lock + implement dual-layout Dress Room (M0), then Fake Show.
