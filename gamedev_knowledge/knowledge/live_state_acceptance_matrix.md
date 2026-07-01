---
type: Knowledge
title: Live-State UI Acceptance Matrix
description: Reusable state-coverage evidence for game UI, player clarity, and art-direction acceptance across future prototypes.
tags: [ui, ux, player-clarity, validation, reusable]
checked: 2026-06-17
---

# Live-State UI Acceptance Matrix

Use this before calling a game UI, player-clarity slice, or first playable surface
accepted. A quality review only proves the states it captured. It does not prove
the whole UI system unless the required state matrix is covered.

## Rule

For player-clarity/UI/FTUE/player-read work, define required states before implementation
or before the first acceptance review. Each required state needs evidence or an
explicit `not covered` debt entry.

Acceptance language must be scoped:

- Correct: `PASS for first_screen, primary_action_ready, reward_active`.
- Incorrect: `UI pass` when modal, returning-player, disabled, or transient
  stress states were not captured.

## Reusable State Categories

Every game should adapt these categories to its genre and loop:

| State tag | Required when | Player-read question |
|---|---|---|
| `first_screen` | every first playable slice | Where am I, and what is the game fantasy? |
| `primary_action_ready` | every interactive slice | What should I press/do now? |
| `primary_action_feedback` | after the core input | What changed because of my input? |
| `reward_active` | any reward/loot/progress feedback exists | What did I get, and where did it go? |
| `progression_panel_open` | upgrades, meta, rooms, inventory, build, or collection exist | What grows, what can I afford, what is locked? |
| `modal_or_choice_open` | choices, cards, dialogs, popups, confirmations exist | What choice is being asked, and what happens next? |
| `locked_or_disabled_state` | any unavailable button/row/feature exists | Why is it locked, and how do I unlock it? |
| `resume_or_reentry_state` | resume, restart, retry, re-entering a screen, saved-state restore, or long-session return exists | What should I understand when I return to this state? |
| `transient_stress_state` | combat numbers, particles, toasts, timers, or reward flyouts can overlap UI | Does anything cover text, buttons, costs, or critical feedback? |
| `small_viewport_state` | web/mobile/portrait is in scope | Does the primary action still fit and remain touchable? |

## Evidence Per State

Each required state should name:

- native screenshot path;
- zoom/readability crop or montage when text/UI is present;
- probe, DevAPI command, or manual scenario that reproduces the state;
- player-read answers:
  - where am I?
  - what can I do now?
  - what changed?
  - what reward did I get?
  - why continue?
- asset audit when generated/chroma-key UI sprites are visible.

## Source-To-Runtime Edge Audit

Generated or chroma-key UI assets require source-to-runtime proof when they are
used for buttons, panels, icons, or other readable UI:

1. source PNG audit: no visible key-color fringe on the accepted crop;
2. pack/atlas rebuild from that source;
3. runtime screenshot crop of the actual UI placement;
4. edge pixel audit or zoom proof on the runtime crop.

Do not accept "looks fine in one full screenshot" as proof for generated UI
assets with transparent/chroma edges.

Use reviewed AI Studio asset-prep tools for source alpha/cutout evidence and
Quality rules for player-facing proof. Source-sheet and cutout checks live under
`ai_studio/assets/prep/`; broad player-facing acceptance lives under
`ai_studio/quality/rules/player_clarity/` and `ai_studio/quality/rules/assets/`.

## Quality Review Usage

Use the matrix as evidence for existing rules in `ai_studio/quality/rules`, such
as `QCLR_001`, `QCLR_002`, or `QART_001`.

Game-specific notes may explain state coverage, references, or debt, but they
must not define new quality rule IDs outside `ai_studio/quality/rules`.

Matrix file example:

```json
{
  "schema": "game.live_state_acceptance_matrix",
  "required_states": ["first_screen"],
  "states": {
    "primary_action_ready": {
      "status": "covered",
      "evidence": "tmp/captures/primary-action.png"
    },
    "reward_active": {
      "status": "not_covered",
      "reason": "reward state belongs to the next slice"
    }
  }
}
```

If a required state is neither covered nor explicitly marked not covered, the
quality review must fail.

## Fixture: Voxelheim Failure

Voxelheim's reward-feedback pass captured one reward state and was later
treated as broad UI acceptance. The missed state was a returning live-combat
stress state with:

- meta/progression panel visible;
- primary action affordable;
- HUD resources visible;
- transient combat/reward floaters active;
- generated/chroma-key CTA button visible.

The reusable lesson is not Voxelheim-specific: every game needs the state matrix
that matches its own loop before a broad UI quality pass can be claimed.
