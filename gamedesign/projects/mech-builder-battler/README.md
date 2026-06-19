---
type: Project Wiki
title: Mech Builder Battler
description: Working wiki for the casual mobile/web 3D mech builder battler concept.
tags: [project, mech-builder-battler, mobile, web, casual, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Mech Builder Battler

Working project wiki for a casual mobile/web 3D game about building, upgrading,
and battling with a customizable mech.

## Concept

The player owns one expressive 3D mech, sends it into short readable battles,
collects salvage/parts, and returns to the hangar to change the mech's play
style. The fantasy is not a full mech simulator; it is a casual build-battle-
upgrade loop that preserves the genre expectation that parts matter.

The visual expectation is a bright, juicy 3D mech presentation: real/model-like
assets, lighting, shadows, normals, painted metal, emissive accents, and
readable effects are core to the concept, not late polish.

## Audience And Platform

- Primary audience: casual mobile and web players who want fast progress,
  readable action, and visible customization.
- Primary lane: accessible progression first.
- Secondary lane: active mastery through loadout choices, boss patterns, and
  optional higher-risk encounters after the first loop is understood.
- Platform target: phone and web. Touch-first UI, short sessions, readable
  controls, and quick resume matter.

## Current Research Artifacts

- [Mobile mech analogs](references/mobile_mech_analogs_2026-06-19.md) - source
  matrix and quick checks for War Robots, Mech Arena, Mech Wars Online,
  Mechangelion, Pocket Bots, CATS, and Tank Stars.
- [Reference evidence capture plan](references/reference_evidence_capture_plan_2026-06-19.md)
  - capture plan for upgrading current source-incomplete reference docs into
  implementation-ready evidence boards and observation ledgers.
- [Mobile control patterns](references/mobile_control_patterns_2026-06-19.md)
  - comparison of fixed virtual joystick, floating virtual joystick / drag
  movement zone, and tap-to-move for the first playable.
- [Visual reference packet](references/visual_reference_packet_2026-06-19.md)
  - first-pass visual direction for 3D mech style, hangar composition, enemy
  silhouettes, phone-scale effects, UI visual rules, and copy-risk.
- [Core loop and meta decomposition](design/core_loop_gameplay_meta.md) -
  derived loop, gameplay pillars, mechanics, meta, and first-slice shape.
- [GDD draft](design/gdd_draft_2026-06-19.md) - consolidated first-pass design
  covering concept, first minute, combat, mech assembly, progression, screens,
  visual direction, MVP scope, risks, and lead review questions.
- [First slice spec](design/first_slice_spec_2026-06-19.md) - production-facing
  spec for the hangar -> battle -> reward -> upgrade -> second battle prompt
  loop, including screen contracts, first content, tuning targets, fake-shot
  requirements, and acceptance criteria.
- [Mechanics and meta matrix](design/mechanics_meta_matrix_2026-06-19.md) -
  reference-backed breakdown of combat controls, mech assembly slots, enemy
  roles, grind guardrails, resources, archetypes, MVP scope, and anti-scope.
- [Design review](design/design_review_2026-06-19.md) - review verdict for the
  current reference, GDD, mechanics, meta, fake-shot, and first-slice package.
- [Reference readiness and prototype plan](design/reference_readiness_and_prototype_plan_2026-06-19.md)
  - consolidated audit of what the references prove, what remains blocked, and
  the first native PC playable slice decomposition.
- [Lead review packet](design/lead_review_packet_2026-06-19.md) - decision
  matrix and review checklist for accepting or revising first-slice choices
  before fake shots or implementation.
- [Reference traceability audit](design/reference_traceability_audit_2026-06-19.md)
  - maps reference evidence to GDD/spec decisions, evidence strength, gaps, and
  implementation stop rules.
- [Fake-shot art request](design/fake_shot_art_request_2026-06-19.md) - draft
  generation brief for hangar, battle, and reward/upgrade fake shots; requires
  lead acceptance before image generation.
- [Visual target review](design/visual_target_review_2026-06-19.md) - accepted
  fake-shot verdict and model/lighting quality bar for the first playable.

## Current Boundaries

- Do not build PvP service complexity for the first version.
- Do not copy names, UI layouts, exact monetization flows, or protected mech
  designs from references.
- Do not let ads, popups, random-item pressure, or pay-to-win progression be
  the core progression answer.
- Do not expose a full simulator-style MechLab in the first minute.
- Do not implement final reference-driven gameplay or UI until the relevant
  central reference deconstruction has gameplay or screenshot evidence.

## Accepted First-Slice Direction

- One owned mech, PvE first.
- Native PC harness for implementation; mobile/web remain UX targets.
- Landscape-first, fixed three-quarter/isometric camera.
- Semi-auto arena with floating virtual joystick / drag movement zone on
  mobile target and WASD in the PC harness.
- Battle grants salvage/resources; player buys/crafts shoulder rockets in the
  hangar.
- Second battle proves shoulder rockets against drone swarm.
- Short dash as starter defense/mobility.
- Heat mechanics with `Cooling` UI label.
- Industrial salvage sport tone with bright accents.
- First mini-boss: Foundry Warden industrial machine.
- First playable should use ready/generated GLB-style models, lighting,
  shadows, normals/materials, and juicy effects rather than debug shapes.

## Current Implementation Readiness

The first native PC prototype can start from the accepted decisions above and
the [readiness/prototype plan](design/reference_readiness_and_prototype_plan_2026-06-19.md).
Exact UI, economy, battle pacing, and final reference-driven art remain locked
behind current-build screenshots and stronger evidence boards.

Next implementation task:
[T0021 - First native PC playable slice](../../../tasks/active/T0021-first-native-pc-playable-slice-for-mech-builder-battler.md).
