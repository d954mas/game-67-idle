---
type: UI Acceptance Matrix
title: Voxelheim Live-State UI Acceptance Matrix
description: Voxelheim fixture instantiating the reusable live-state UI acceptance matrix.
tags: [voxelheim, ui, visual-gate, product-gate, fixture]
checked: 2026-06-17
---

# Voxelheim Live-State UI Acceptance Matrix

Reusable template:
`gamedesign/knowledge/live_state_acceptance_matrix.md`.

This file is a fixture/example, not the universal rule. It exists because
Voxelheim exposed the failure mode: a product gate captured one state and was
mistaken for broad UI acceptance.

## Required States

| Reusable tag | Voxelheim state | Evidence status |
|---|---|---|
| `first_screen` | fresh first fight with Gold/Blocks HUD, hero, enemy, keep | covered by earlier rescue screenshots; refresh before any final pass |
| `primary_action_ready` | Gate repair CTA affordable/unaffordable | covered by `build/captures/ui_text_live_overlap_fix.png` for affordable Gate |
| `modal_or_choice_open` | 1-of-3 rune card choice | covered by `build/captures/ui_rescue_card_choice.png` |
| `progression_panel_open` | Frost Keep objective + Frost Blueprints visible | covered by `build/captures/ui_rescue_blueprints_layout.png` |
| `primary_action_feedback` | repair, card pick, shard buy, offline collect feedback | partially covered by reward screenshots |
| `reward_active` | reward floaters/pulses visible without hiding UI | partially covered; needs final stress proof |
| `returning_player_state` | offline popup and post-offline collect live state | popup covered; live state covered by `build/captures/ui_text_live_overlap_fix.png` |
| `transient_stress_state` | combat floaters active while CTA + Blueprints + HUD are visible | covered by `tmp/ui_text_overlap_probe.py`; keep as required regression fixture |
| `locked_or_disabled_state` | unaffordable Blueprint/repair rows | partially covered; refresh before final pass |

## Required Audits

- Readability zoom for every UI-heavy state:
  `py -3.12 tools/devapi/ui_readability.py <screenshot>`.
- Before/after comparison when changing UI:
  `py -3.12 tools/devapi/ui_readability.py <new> --compare <old>`.
- Source-to-runtime edge audit for CTA/button/panel/icon art:
  `py -3.12 tools/assets/audit_runtime_ui_edges.py --image assets/raw/button.png`
  and
  `py -3.12 tools/assets/audit_runtime_ui_edges.py --image build/captures/ui_text_live_overlap_fix.png --crop 330,430,625,500`.

## Example Gate Coverage

The live CTA hotfix can only claim these states:

- `primary_action_ready`
- `progression_panel_open`
- `returning_player_state`
- `transient_stress_state`

It does not prove final room art, final reward timing, full fun/retention, or
all locked/disabled states.
