---
id: T0011
title: Research casual mobile web mech builder battler references
status: review
epic: ""
priority: P1
tags: [research, gamedesign, mobile, web, mechs]
created: 2026-06-19
updated: 2026-06-19
---

## What

Research and deconstruct mobile/web-friendly mech and adjacent casual vehicle
battler references for the next game concept. Produce project-specific durable
notes that cover reference collection, quick deconstruction, core loop,
gameplay, mechanics, meta progression, and first-slice implications.

Working game id: `mech-builder-battler`.

Scope boundaries:

- In scope: research, reference/source packet, GDD-facing decomposition, risks,
  and next source gaps.
- Out of scope: runtime implementation, final art, monetization design, and any
  claim that store-page quick checks are implementation-ready central
  deconstructions.

## Done when

- [x] `gamedesign/projects/mech-builder-battler/README.md` captures the working
      concept, audience, platform, boundaries, and research entry points.
- [x] `gamedesign/projects/mech-builder-battler/references/mobile_mech_analogs_2026-06-19.md`
      lists checked mobile analog sources with evidence labels, borrow/avoid,
      copy-risk, and source gaps.
- [x] `gamedesign/projects/mech-builder-battler/design/core_loop_gameplay_meta.md`
      decomposes the core loop, gameplay pillars, mechanics, meta progression,
      first-minute target, first vertical slice, and implementation risks.
- [x] The docs explicitly mark store-page research as quick-check/source-packet
      incomplete and name the central deconstruction gaps before implementation.
- [x] `node tools/taskboard/cli.mjs validate` passes or any failure is logged
      with the exact blocker.
- [x] Central deconstruction for Mech Arena records first screen, first input,
      visible response, reward/progression UI, borrow/avoid/copy-risk, and
      mobile/web translation.
- [x] Central deconstruction for CATS records build screen, part slots, battle
      proof, reward timing, difficulty/ad pressure, borrow/avoid/copy-risk, and
      mobile/web translation.
- [x] Central deconstruction for Mechangelion records first battle, simple
      controls, upgrade cadence, boss framing, borrow/avoid/copy-risk, and
      mobile/web translation.
- [x] Visual reference packet records 3D mech style, hangar composition, enemy
      silhouettes, phone-scale effect readability, and copy-risk.

## Open questions

- Should the working title remain `Mech Builder Battler` after the first
  playable, or be renamed during branding?

## Log

- 2026-06-19: Created working project wiki folder
  `gamedesign/projects/mech-builder-battler/` for the user-approved casual
  mobile/web 3D mech-builder battler concept. Did not run
  `tools/game_context/new_prototype.mjs` during research because the immediate
  work was concept/reference capture, not a native implementation slice.
- 2026-06-19: Added mobile analog quick-check packet at
  `gamedesign/projects/mech-builder-battler/references/mobile_mech_analogs_2026-06-19.md`
  using checked Google Play pages for War Robots, Mech Arena, Mech Wars Online,
  Mechangelion, Pocket Bots, CATS, and Tank Stars.
- 2026-06-19: Added GDD-facing core loop/gameplay/meta decomposition at
  `gamedesign/projects/mech-builder-battler/design/core_loop_gameplay_meta.md`.
- 2026-06-19: `node tools/taskboard/cli.mjs validate` passed (`ok: no problems
  found`). Initial source packet checkpoint is complete, but the task remains
  `doing` because the broader goal still needs central deconstructions and a
  visual reference packet before implementation.
- 2026-06-19: `node tools/ai.mjs validate` passed quick validation. Updated
  `AGENTS.md` and `tasks/STATUS.md` so the active research concept no longer
  appears as "no active game concept selected"; implementation remains gated on
  central reference deconstructions and accepted slice scope.
- 2026-06-19: Added central deconstruction drafts for Mech Arena, CATS, and
  Mechangelion under
  `gamedesign/projects/mech-builder-battler/references/`. These satisfy the
  written research fields for first pass deconstruction, but each doc still
  marks source-packet gaps and forbids implementation-level copying until
  stronger gameplay/screenshot evidence is captured. Visual reference packet
  remains open.
- 2026-06-19: Re-ran `node tools/taskboard/cli.mjs validate` and
  `node tools/ai.mjs validate`; both passed after adding the central
  deconstruction drafts and updating current status.
- 2026-06-19: Added visual reference packet at
  `gamedesign/projects/mech-builder-battler/references/visual_reference_packet_2026-06-19.md`
  covering 3D mech style, hangar composition, enemy silhouettes, phone-scale
  effects, UI visual rules, copy-risk, and fake-shot requirements. Moved task
  to `review`: research deliverables are present, while final art/runtime
  rough native PC implementation may start from the accepted handoff; final art
  remains gated on accepted fake shots or playable screenshots.
- 2026-06-19: `node tools/taskboard/cli.mjs validate` passed after the visual
  packet update. Did not keep touching broader pipeline/tool validation because
  another agent is currently working on pipeline, tools, and engine changes.
