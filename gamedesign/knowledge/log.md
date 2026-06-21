---
type: Knowledge Log
title: Design Knowledge Log
description: Compact history of meaningful changes to the reusable design knowledge base.
tags: [knowledge, log, maintenance]
timestamp: 2026-06-13T00:00:00Z
---

# Design Knowledge Log

Keep this file short. Record only changes that affect how future agents or
humans should use the knowledge base.

## 2026-06-13

- Migrated legacy knowledge pages to OKF-lite frontmatter so agents can index
  them without reading every page first.
- Neutralized one old project-specific art-pipeline example so the knowledge
  base stays template-friendly.
- Added OKF-lite rules to [README](README.md): plain Markdown, small YAML
  frontmatter, links, citations, index, and log; no required SDK or validator.
- Added [Index](index.md) as the main navigation map for reusable game design
  knowledge.
- Added this log so knowledge-base changes are visible without turning design
  docs into status tracking.

## 2026-06-15

- Added source note `sources/voodoo_new_big_three_hybrid_casual_2026-06-15.md`
  (AppMagic: VOODOO's 2026 hits Castle Busters / Marble Sort! / Sand Loop; the
  loop Sort Puzzle wave and hybrid-casual monetization).
- Promoted reusable conclusions to [Hybrid-Casual Patterns](hybrid_casual_patterns.md)
  (satisfying core loop + one twist on a proven genre; IAP-led monetization
  shape) and registered it in [Index](index.md).

## 2026-06-17

- Added the project skill `.codex/skills/design-source-knowledge/` for source
  intake, evidence labels, routing, promotion, and knowledge-base hygiene.
- Added frontmatter to two reusable source packets and moved Voxelheim-specific
  application out of the global source shelf by linking to the project
  deconstruction.
- Added source note `sources/maxpower_roblox_player_needs_2026-06-17.md`
  (Max Power Gaming / Rotrends-backed Roblox top-game analysis) and promoted
  reusable conclusions to [Player Need Lanes](player_need_lanes.md): accessible
  progression vs active mastery as a first-screen/concept alignment model.

## 2026-06-20

- Added [Game Art Contract And Visual Gate](art_contract.md): the reusable
  per-game taste anchor (`art_contract.json`) and the three-way
  (pass/review/fail) visual gate flow (Universal Gate -> Art Contract ->
  Visual Critic -> Art Lead Judge -> Human Review Queue). Registered in
  [Index](index.md).
- Reshaped `tools/product_gate/review.mjs` to consume a machine
  `game.visual_critique` JSON (`--critique`), auto-resolve a per-game art
  contract (`--contract`, `pass_threshold` override), and accept the `review`
  verdict; `close_slice.mjs` blocks a `review` gate in strict mode (advisory
  otherwise). Six axes stay universal; the contract carries taste only.
- Added `tools/product_gate/visual_critic_run.mjs` (`node tools/ai.mjs critique`):
  the vision art-lead critic that looks at state screenshots with the contract +
  reference banks in context, runs an independent refute pass, and reconciles
  disagreement into `review`. Verified live on this box via codex CLI (gpt-5.5
  vision): instruction on stdin, screenshots with `-i {IMAGES}`. Emit mode (no
  `--model-cmd`) writes the instruction for a manual run.
- Added universal multi-state capture: `tools/devapi/state_capture.py`
  (`StateCapture`) captures one screenshot per key state and writes the
  `game.live_state_acceptance_matrix`; `visual_critic_run.mjs --state-matrix`
  auto-derives shots from covered states. A new game plugs in one
  `tools/<id>/capture_states.py` driver (see the "What A New Game Provides"
  section + the template); everything else is universal.

## 2026-06-18

- Added chroma cutout guidance to [AI Art Iteration Pipeline](ai_art_iteration_pipeline.md):
  clean key-color holes are repairable with explicit holes/soft-matte modes,
  while key color baked into outlines, material, shadows, and semishadows needs
  benchmarked repair, safer-key regeneration, true alpha, split shadow layers,
  or dual-plate alpha extraction.
