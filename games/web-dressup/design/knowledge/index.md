---
type: Game Knowledge Index
title: Game Knowledge Index
description: Index of durable game-specific knowledge for web-dressup.
tags: [game-knowledge, index]
game_id: web-dressup
status: draft
---

# Game Knowledge Index

First stop for durable game-specific knowledge.

## Accepted Design Facts

- Repo id: `web-dressup`. Working player title: **Переодевалка** / window title **Dressup**.
- Genre: browser dress-up / avatar try-on (not RPG, not idle).
- Platform priority: web (WASM); native is validation only.
- First playable screen: single **Dress Room** with layered slots
  `hair`, `top`, `bottom`, `shoes`, `acc`.
- First slice: all starter items free/unlocked; no combat or quests.
- Hit path: **Poki**, **2D**, **portrait+landscape**, **no multiplayer**,
  **fake shows OK** (see `product-locks.md`, `competitor-strategy.md`).
- Second loop: Theme → Fake Show (NPC rivals, local stars) → restyle.
- Do not chase DTI live CCU, Vortella real MP, or Nikki gacha in v1.

## Reference Lessons

- **Concept (accepted):** `../concept.md`
- Competitor/hit research (global): `sources/2026-07-09-competitor-hit-research.md`
- **Poki competitor research:** `sources/2026-07-09-poki-competitors.md`
- Strategy (Poki locks): `competitor-strategy.md`
- Product locks: `product-locks.md`
- Vortella lesson still applies for freeplay: no text modals; session length
  from freedom + second loop (for us: Fake Show, not real MP).
- DTI juice without MP: theme + runway + scores + podium as **single-player theatre**.
- Portal graveyard: generic dolls, story walls, early paygates, desktop-only UI.

## Playtest Or Build Findings

- (none yet)

## Open Questions

- Final Poki listing title and art style lock.
- Body/skin preset count in v1.
- Fake-social tone (how “real” names/chat feel).
- Recolor in M0 vs fixed colors first.
