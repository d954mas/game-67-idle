---
type: Game Knowledge
title: Product locks
description: Lead-locked constraints for web-dressup (Poki, 2D, orientations, no MP).
tags: [locks, poki, product]
game_id: web-dressup
status: accepted
date: 2026-07-09
---

# Product locks — web-dressup

Accepted from lead (2026-07-09):

1. **Poki** is the distribution target (web portal hit path).
2. Game is **2D** (layered dress-up, not 3D).
3. **Portrait and landscape** are both first-class layouts.
4. **No multiplayer** (no live players, no net matchmaking).
5. **Fake shows and similar fake social spectacle are allowed**
   (NPC rivals, fake scores, runway, crowd juice).

Implications:

- Session ladder ends at freeplay + theme + fake show (+ content drops).
- Do not design server authority, lobbies, or human voting for v1.
- Scoring and “social” pressure are **local simulation + presentation**.
- Responsive UI work is core product, not polish.
