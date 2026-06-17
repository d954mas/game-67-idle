---
id: T0012
title: Require state-coverage tags in product gates
status: done
epic: E001
priority: P2
tags: [pipeline, product-gate, visual-gate, validation]
created: 2026-06-17
updated: 2026-06-17
---

## What

Improve the product gate so a PASS cannot silently stand in for UI states it did
not capture. This must be reusable for every future game, not Voxelheim-specific.

Voxelheim is only the regression fixture: its reward-feedback pass used an
offline/reward screenshot as product evidence, but later user review exposed
failures in a different live state. Future games need the same protection for
their own state set, such as menu/modal/combat/result/upgrade/offline/returning
player states.

## Done when

- [x] Product gate records explicit reusable scenario/state tags, e.g.
      `first_screen`, `primary_action_ready`, `modal_open`, `reward_active`,
      `progression_panel_open`, `returning_player`, `disabled_or_locked_state`.
- [x] Game-specific state tags can extend the reusable set, e.g. Voxelheim's
      `blueprints_visible`, `cta_affordable`, `floaters_active`.
- [x] A strict visual/UI pass requires either the matrix-required states or an
      explicit "not covered" debt line.
- [x] `product_read_gate_latest.json` exposes the covered states so reviewers
      can see what the pass does and does not prove.
- [x] Tests cover a generic future-game pass with complete state tags and a
      rejected/flagged pass with missing required tags.

## Open questions

- Should the required matrix live in `tools/product_gate/review.mjs`, a reusable
  `gamedesign/knowledge/` template, or a per-game matrix file consumed by the
  gate?

## Log
- 2026-06-17 created from Voxelheim process retrospective after the live CTA
  text/purple-edge regression showed that one screenshot gate was treated too
  broadly.
- 2026-06-17 lead clarified this must be universal for all future games.
  Voxelheim remains the first regression fixture, not the scope of the fix.
- 2026-06-17 implemented in `tools/product_gate/review.mjs`:
  `--require-state`, `--covered-state`, `--not-covered-state`; JSON/Markdown
  now include `state_coverage`; strict pass fails if required states are neither
  covered nor explicit debt. Evidence: `node --test tools/product_gate/test.mjs`
  23/23 pass; `node --test tools/ai.test.mjs` 16/16 pass.
