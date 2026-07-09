---
type: Game Concept
title: Web Dressup Concept
description: Locked product concept for a Poki 2D single-player dress-up with optional fake fashion show.
tags: [concept]
game_id: web-dressup
status: accepted
date: 2026-07-09
---

# Concept: Fashion Lab (working title) — web-dressup

Status: **accepted product concept** (title TBD). Repo id: `web-dressup`.

## One-liner

A **2D browser dress-up lab for Poki**: free mix-and-match styling with zero
pressure, plus an **optional Fake Fashion Show** (theme, runway, NPC rivals,
stars, podium) that feels like a DTI-style round — **without multiplayer, map
running, or waiting for other players**.

## Player fantasy

"I open Poki, dress someone up in seconds, and when I want drama I hit Show and
feel like I won a runway — then I go restyle."

## Why this game exists

| Market fact | Our answer |
|-------------|------------|
| Poki has 65+ dress-up titles; most are thin makeover/one-shot dolls | Deeper freeplay lab + clear second loop |
| Vortella owns **live multiplayer** fashion on Poki | We **do not** compete there; single-player + fake show |
| DTI taught theme → style → score → podium | Same **loop**, 2D UI closet, no Roblox/3D/map |
| Portal bounce kills text walls and forced tutorials | Boot straight into Dress Room |

## Product locks

| | |
|--|--|
| Distribution | **Poki** (web primary) |
| Presentation | **2D** layered sprites |
| Layout | **Portrait + landscape** first-class |
| Multiplayer | **None** |
| Social spectacle | **Fake shows** (NPC rivals, fake scores, runway juice) OK |
| Navigation | **UI catalog only** — no 2D walk-around map in v1 |
| Auth | No login for core loop |
| Monetization (v1) | Portal ads path; no gacha energy on freeplay |

## Design pillars

1. **Instant look** — first equip in under ~3s; no full-screen text modals.
2. **Sandbox first** — freeplay is complete fun without Show.
3. **Opt-in showtime** — player chooses theme (or random) and taps Show.
4. **No waiting** — no fake lobby queues; Fake Show starts immediately.
5. **Dual layout craft** — phone portrait and desktop landscape both feel native.
6. **Readable silhouette** — looks photograph well for screenshots/social.

## Core experience

### Default: Dress Room (classic freeplay)

- Center stage: 2D character base + equipped layers.
- Categories: hair / top / bottom / shoes / acc (expand later).
- Catalog grid/strip; tap equip; tap again to clear.
- **Random** and **Reset**.
- No timer. No forced theme.

### Optional: Fake Show (DTI-loop without multiplayer)

```
Player styles (freeplay)
  → picks theme OR random theme
  → taps Show
  → runway sequence + 2–5 NPC rivals
  → local star score + podium
  → Restyle / Show again
```

- **Timer:** not default. Optional later as **Challenge** mode.
- **Theme:** player-selected or random; not "wait for server theme."
- **Map:** none. Closet is UI. Runway is a **sequence screen**, not a world.

### Roadmap (scope discipline)

| Phase | Ships | Does not ship |
|-------|--------|----------------|
| **M0** | Dual-layout freeplay, layers, random/reset, starter catalog | Show, timer, map, multiplayer |
| **M1** | Theme picker + Show button path (can be light) | Hard challenge timer |
| **M2** | Full Fake Show (NPC, stars, podium, restyle) | Live players |
| **M3** | Poses, screenshot polish, more content, optional Challenge timer | 3D, real MP |

## Audience

Poki casuals on **phone + desktop**: kids/teens and fashion-curious players who
want creative styling and short "round" dopamine, not accounts or gacha.

## Tone and presentation

- Bright, readable fashion fantasy (exact art style **still open**).
- UI chrome minimal; avatar is the hero.
- Fake-social copy (names, one-liners, crowd SFX) is allowed but must not claim
  real multiplayer deceptively in store text — frame as **Fashion Night / Show**.

## No-go list

- Real multiplayer, chat, trade, live human voting.
- 2D overworld where player **runs to clothes** (v1).
- Forced timer on every session.
- Forced theme / forced show on boot.
- 3D character pipeline.
- Combat, idle grind, RPG maps.
- Login wall for core play.
- Gacha energy gates on freeplay.
- Long spa/story before free styling.
- Handmade `draw_text`; SVG as final game art.

## Competitive stance (summary)

See `knowledge/sources/2026-07-09-poki-competitors.md`.

- **Do not** out-Vortella on multiplayer.
- **Do** outclass thin makeover dolls on freeplay depth + dual layout.
- **Do** offer single-player theme+show juice that most Poki dolls lack.
- Borrow DTI **ritual**, not DTI **map/3D/live lobby**.

## Success criteria (concept level)

A stranger on Poki:

1. Understands "dress the character" in 5 seconds.
2. Enjoys freeplay without reading anything.
3. Discovers Show as a bonus, not a requirement.
4. Can play portrait on phone and landscape on PC without rage.
5. After Show, wants to restyle (loop closes).

## Open (lead still decides)

- Final English Poki title.
- Art style lock (chibi / fashion flat / semi-real 2D).
- Body/skin preset count.
- Fake-social intensity.
- Recolor in M0 vs fixed colors first.
