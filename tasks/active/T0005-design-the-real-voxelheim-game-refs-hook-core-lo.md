---
id: T0005
title: "Design the REAL Voxelheim game: refs -> hook -> core loop -> progression/economy -> FTUE (pass the core-loop gate)"
status: doing
epic: E001
priority: P1
tags: [voxelheim, design, core-loop, gate]
created: 2026-06-16
updated: 2026-06-17
---

## What

The first slice is a polished visual/tech proof, NOT a designed game (lead:
"I don't get what the game is, the loop, the idea, the refs"). Do the design
work that should have come first, and make the build pass the new
**Game / core-loop gate** in AGENTS.md. BLOCKED on the genre decision (idle vs
real-time action-RPG, see Open questions) — once chosen, run the design.

## Done when

- [x] Genre/direction chosen by the lead (idle / action-RPG / other).
- [x] Reference deconstruction: 1-3 named real games in the chosen genre, with a
      Reference Digest (observed loop, economy, progression, retention hook,
      borrow/avoid) per `gamedesign/knowledge/reference_deconstruction.md`.
- [x] Hook/fantasy stated in ONE sentence a player would repeat.
- [x] Core loop with NUMBERS: >=3 interlocking verbs, the reward, and the reason
      to repeat; first-30s, first-5-min, and the session loop.
- [x] Progression / economy: what grows, what is spent, why; the "next 5 minutes"
      pull; meta/retention (upgrades / prestige / unlocks as fits the genre).
- [x] `data/balance.json` (+ economy data) so the loop is implementable from
      files, not invented in code.
- [x] gdd.md rewritten around the loop (not the screen); concept.md hook updated.
- [ ] Build re-judged by a **game-design critic/playtest** (fun + loop +
      why-replay), separate from the art/UX critic — passes the core-loop gate.

## Open questions

- (resolved 2026-06-16) Genre locked to idle/incremental.
- Open: broader fun/retention critic pass after offline return and stronger
  feedback are synced to the rescue loop.

## Log

- 2026-06-16 created from the session retrospective (AI_PIPELINE_HISTORY): the
  pipeline gated appearance + screen-teachability but not the GAME; added the
  Game/core-loop gate to AGENTS.md. This task is the real design work.
- 2026-06-16 Genre LOCKED = idle (lead). Design written DESIGN-FIRST per the new core-loop gate: references/idle_reference_digest.md (named refs: Tap Titans 2 / Idle Slayer / NGU Idle / Melvor), gdd.md rewritten around the idle loop (hook, loop w/ numbers, 2 currencies, 4 upgrades, bosses/10, prestige@25, offline), data/balance.json with the economy. AGENTS active concept -> idle. Next: implement the idle slice (convert voxelheim_main.c) after lead accepts the design + judge with a game-design critic.
- 2026-06-16 Reference research: saved external sources to gamedesign/sources/ (idle design: Pecorella "Quest for Progress" GDC math, Eric Guan principles, gamedeveloper postmortem; deconstruction method: Deconstructor of Fun, Koster "Theory of Fun"). Sharpened primary-gdd-pipeline with the reference anti-pattern (genre digest != deconstruction). The per-game idle deconstruction subagent failed (transient) -> rerun next. Pecorella flags balance.json prestige exponent (1.5 super-linear) should be fractional/sqrt.
- 2026-06-16 REAL per-game deconstruction done (myself, web research; subagents flaky): references/idle_deconstruction.md grounded in Clicker Heroes (HP ~x1.55/encounter, boss x10, souls=totalLevels/2000 = +10%/soul) + Tap Titans 2 (HP 18.5*1.57^stage, relics->artifacts); honest SECONDARY source-packet (wikis/guides, no first-hand video frames). Corrected balance.json v2: HP growth 1.15->1.45, gold 1.18->1.42, prestige ^1.5(super-linear, wrong) -> ^0.8/3(fractional), +10%/shard. GDD repointed to the deconstruction. Design grounded; next = build the idle slice (convert voxelheim_main.c).
- 2026-06-17 Lead rejected current product quality: UI/UX ugly, unclear, unreadable; GDD too simple/banal; agent has permission to change game design, art, sprites, UI/UX. Ran rescue deconstruction and redirected Voxelheim from generic idle RPG to **Frost Keep Rebuilder**. Added `reviews/prototype_deconstruction_2026-06-17.md`, `references/competitor_deconstruction_2026-06-17.md`, `visual/ui_ux_rescue_spec.md`, and `data/rescue_loop.json`; rewrote `gdd.md`, `concept.md`, `game_implementation_plan.md`; updated `tasks/STATUS.md`. Current runtime is now explicitly legacy until synced to rescue loop.
- 2026-06-17 Runtime sync started: native build now implements the first rescue-loop proof (Frost Blocks, Keep Rank, Gate/Forge/Campfire repair UI, Gate repair, 3-card rune choice, card-choice combat pause). Screenshot/readability evidence: `build/captures/rescue_gate_cards.png` and `build/captures/rescue_gate_cards_uizoom.png`. This proves the new hook at screen level but still needs balance sync, Forge/Campfire effects, final UI art polish, and a game-design/player critic pass.
- 2026-06-17 Runtime/data sync extended: `data/balance.json` now describes the rescue loop instead of the old v4 Frost Fury/Crit/Greed/Multi/prestige-tree plan; `rescue_loop.json` status moved to `runtime_first_chain_synced`. Full native proof now covers Gate -> card -> Forge -> Campfire, with helper unlock and +25% damage (`rescue_campfire_helper.png`). Still not a full game pass: prestige/offline and stronger room-specific art remain open, and a focused player/critic pass is still required.
- 2026-06-17 First meta-loop implemented: **Avalanche Reset** unlocks at Keep
  Rank 3, pays persistent Frost Shards, and resets Gold/Frost Blocks/rooms/run
  progress so the player re-enters the repair loop with meta currency. Evidence:
  `py -3.12 tmp/avalanche_reset_probe.py 9146 build/captures/avalanche_reset_after.png`,
  `build/captures/avalanche_reset_after.png`,
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17T06-41-25-128Z_desktop_avalanche_reset.md`.
  Remaining design gaps: offline, shard-upgrade UX, stronger feedback, and a
  broader 5-minute retention/fun critic pass.
- 2026-06-17: product gate PASS (desktop_shard_blueprints); review: gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17T06-57-04-108Z_desktop_shard_blueprints.md; screenshot: build/captures/shard_blueprints_after_buy.png; next: continue to the next narrow slice
- 2026-06-17 Frost Blueprints runtime proof: clicking the visible Sharper Steel
  row after Avalanche Reset spends 1 Shard (3 -> 2), raises permanent damage
  blueprint to Lv1, and increases hero damage from 5 to 5.5. Evidence:
  `py -3.12 tmp/shard_blueprints_probe.py 9147 build/captures/shard_blueprints_after_buy.png`,
  `build/captures/shard_blueprints_after_buy_uizoom.png`. Remaining design
  gaps: offline return, stronger feedback/audio, polished room art, and a
  broader fun/retention critic pass.
- 2026-06-17 Offline return synced to the rescue loop: first Avalanche Reset now
  unlocks offline return and Camp Supplies; the return popup grants both Gold
  and Frost Blocks so idle time feeds back into Keep repair. Evidence:
  `py -3.12 tmp/offline_return_probe.py 9148 build/captures/offline_return_popup.png`,
  `build/captures/offline_return_popup.png`,
  `build/captures/offline_return_popup_uizoom.png`,
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17T07-23-12-151Z_desktop_offline_return.md`.
  Remaining design gaps: stronger feedback/audio, polished room art, and a
  broader fun/retention critic pass.
- 2026-06-17: product gate PASS (desktop_offline_return); review: gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17T07-23-12-151Z_desktop_offline_return.md; screenshot: build/captures/offline_return_popup.png; next: continue to the next narrow slice
- 2026-06-17 Current competitor refresh added after renewed lead feedback:
  Legend of Slime, Slayer Legend, Blade Idle, Tap Titans 2, and AFK Journey
  live Google Play pages rechecked. The design conclusion sharpened: Voxelheim
  must lead with visible Frost Keep rebuilding and staged UI, not generic
  auto-battle + upgrades.
- 2026-06-17 UI rescue pass: runtime panels shifted toward frosted-blue
  hierarchy, room rows now state unlock effects, Frost Shards badge no longer
  overlaps Frost Keep title, and card-choice gets a dedicated instruction
  plate. Evidence:
  `py -3.12 tmp/rescue_probe.py 9157 build/captures/ui_rescue_card_choice.png`,
  `py -3.12 tools/devapi/ui_readability.py build/captures/ui_rescue_card_choice.png --region "hud=0.00,0.00,1.00,1.00"`,
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_card_choice_ui_rescue.md`.
  Remaining design gaps: right-rail meta layout, stronger feedback/audio,
  polished room art, and broader fun/retention critic pass.
- 2026-06-17 Right-rail meta layout cleaned: after Avalanche Reset, Frost Keep
  collapses to a compact `Rooms N/3 / Next` objective strip and Frost
  Blueprints becomes the separate permanent-upgrade spend panel. Evidence:
  `py -3.12 tmp/shard_blueprints_probe.py 9162 build/captures/ui_rescue_blueprints_layout.png`,
  `py -3.12 tools/devapi/ui_readability.py build/captures/ui_rescue_blueprints_layout.png --region "right_meta=0.68,0.00,1.00,0.86"`,
  `gamedesign/projects/voxelheim/reviews/product_read_gate_2026-06-17_blueprints_layout.md`.
  Remaining design gaps: stronger feedback/audio, polished room art, and
  broader fun/retention critic pass.
