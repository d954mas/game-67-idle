---
type: Game Design Document
title: Web Dressup GDD
description: Implementation-facing design for Poki 2D single-player dress-up with fake shows.
tags: [gdd]
game_id: web-dressup
status: draft
---

# Web Dressup GDD

Status: draft — **Poki / 2D / dual orientation / no multiplayer locked**

## Product locks

| Lock | Decision |
|------|----------|
| Target | **Poki** web hit |
| Presentation | **2D** layered dress-up |
| Layout | **Portrait + landscape** both supported |
| Multiplayer | **No** |
| Spectacle | **Fake shows** (runway, NPC rivals, fake scores, fake crowd) OK |
| Core fantasy | Style → optional showtime → back to closet |

## Definition Of Done (design package)

1. Locks above are durable in concept + GDD + knowledge.
2. First playable = **Dress Room freeplay** on 2D layers.
3. Second loop designed = **Theme + Fake Show** (single-player).
4. UI contracts exist for portrait and landscape.
5. Implementation can start without chat history.

Out of scope for first code slice: full catalog polish, real multiplayer, gacha,
UGC, account systems.

## Player Fantasy

"I dress up in the browser, hit Show, and feel like I won a fashion round —
even though the rivals are fake."

## First 30 Seconds (Poki)

1. Boot straight into **Dress Room** (no story modal).
2. Avatar + categories + catalog visible; first equip in **&lt;3s**.
3. Random / Reset available.
4. Visible **Theme** chip or **Show** button (may soft-lock until first equip).
5. No login, no multiplayer matchmaking UI.

## Core Loop

See `data/core_loop.json`.

Short form:

```
Dress Room freeplay
  → pick theme (optional)
  → style outfit
  → Fake Show (runway + NPC rivals + score/stars)
  → podium / reward juiciness (cosmetic unlock later)
  → back to Dress Room
```

## Player Verbs

- **Select category / equip item** (core).
- **Randomize / Reset**.
- **Pick theme** (prompt for the next show).
- **Enter Fake Show** — timed or untimed styling handoff, then auto-runway.
- **Pose** (optional) before show.
- **Screenshot / share** (Poki-friendly; no hard dependency on native share).
- **(Later)** Save favorite looks locally.

## Fake Show rules (single-player)

- Player never waits on a real human.
- 2–5 **NPC rivals** with prebuilt or seeded random outfits (themed loosely).
- Runway sequence: player walk → rival flashes → **star rating** (algorithm +
  juice, not true ML).
- Scoring can weight: theme tags on items, variety, completeness of slots,
  optional “risk” accessories — keep formula simple and tunable.
- Always allow a **fun failure** and a **clear win**; avoid pure RNG frustration.
- Fake names, fake chat one-liners, fake crowd SFX = allowed spectacle.
- **Honest framing optional**: soft fantasy (“Fashion Night”) is fine; do not
  need to label “bots” to the player.

## Rules And Feedback

- One item per slot (or empty); fixed 2D draw order.
- All interaction works with touch and mouse.
- Portrait and landscape rearrange chrome; **stage never becomes unusably small**.
- Starter catalog free/unlocked for freeplay.
- No real currency gate on Show in v1 freeplay (or only soft tutorial gate).

## UI Flow

See `data/ui_flow.json`.

### Dual layout (required)

| Orientation | Stage | Chrome |
|-------------|-------|--------|
| **Portrait** | Top ~45–55% height | Categories + catalog + actions stacked below; big thumbs |
| **Landscape** | Left/center ~50–60% width | Categories + catalog on right or bottom strip; actions docked |

Resize/orientation change must reflow without resetting the outfit mid-session
unless the engine forces a full reload (avoid if possible).

### Screens

1. **Dress Room** — primary freeplay.
2. **Theme Picker** — modal or sheet (short labels, icons; minimal text).
3. **Fake Show** — runway stage, scores, podium; skip allowed after first play.
4. **Settings** — audio only if needed; never blocks first session.

## Content model (first slice)

Slots: `hair`, `top`, `bottom`, `shoes`, `acc` (+ base body).  
Minimum: **4 items/slot** + theme tags on items for Fake Show scoring.  
2D art: aligned anchors, transparent PNG layers, provenance per asset rules.

## Validation (Poki-facing)

- Portrait phone + landscape desktop both playable.
- Time-to-first-equip &lt; 3s on warm load.
- Full freeplay → Fake Show → return path without softlock.
- No multiplayer/network dependency for Show.
- Session goal: freeplay holds interest; Show adds a reason to restyle and replay.

## Implementation sequence

1. Dual-layout Dress Room shell (strip template demo fantasy).
2. 2D layer compositor + starter catalog.
3. Random / Reset.
4. Theme tags + Fake Show sequence + score juice.
5. Screenshot / pose polish.
6. Poki build package + orientation QA matrix.

## Success metrics (Poki)

- Time to first equip
- Avg session length (climb via freeplay depth + Show replays)
- Shows started / session
- Outfit changes after a Show (restyle loop)
- Orientation switch without rage-quit (manual QA)
