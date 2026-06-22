# Little Lives Live-State Acceptance Matrix

Project: `little-lives`

Reusable rule: `gamedesign/knowledge/live_state_acceptance_matrix.md`.
Machine input: `visual/live_state_acceptance_matrix.json`.

Fill this before accepting a broad UI/visual/product pass. A product gate pass
only proves states explicitly covered by screenshot/probe evidence or explicitly
marked as not-covered debt.

## Required States

| State tag | First proof to capture | Status |
|---|---|---|
| `first_screen` | Fresh first native screenshot with scene, HUD, and first action visible. | pending |
| `hud_visible` | HUD labels, values, resource icons, and progress readable in a zoom/crop. | pending |
| `primary_action_ready` | Primary CTA/control visible, readable, and actionable. | pending |
| `primary_action_feedback` | Core action response: animation, state delta, damage, build, or other feedback. | pending |
| `reward_active` | Reward/loot/progress moment visible without hiding critical UI. | pending |
| `progression_panel_open` | Upgrade/inventory/build/meta panel if the slice has progression. | pending |
| `modal_or_choice_open` | Dialog, card, choice, confirmation, or explicit not-covered debt. | pending |
| `locked_or_disabled_state` | Unavailable/unaffordable/blocked control with readable reason. | pending |
| `resume_or_reentry_state` | Resume, restart, retry, or re-entering this screen, or explicit first-slice debt. | pending |
| `transient_stress_state` | Combat numbers, particles, toasts, timers, or flyouts active over normal UI. | pending |

## Product Gate Pattern

Use this matrix in the first product-read gate:

```powershell
node tools/ai.mjs gate `
  --project little-lives `
  --task <task-id> `
  --surface desktop `
  --screenshot <native-screenshot.png> `
  --verdict fail `
  --strict `
  --visual-strict `
  --state-matrix gamedesign/projects/little-lives/visual/live_state_acceptance_matrix.json `
  --require-state first_screen `
  --covered-state first_screen:<native-screenshot-or-probe> `
  --covered-state hud_visible:<hud-zoom-or-screenshot> `
  --covered-state primary_action_ready:<native-screenshot-or-probe> `
  --not-covered-state modal_or_choice_open:"not in this first slice yet" `
  --not-covered-state resume_or_reentry_state:"not in this first slice yet"
```

Before a `pass`, every required state must be either `--covered-state` with
evidence or `--not-covered-state` with a concrete reason.
