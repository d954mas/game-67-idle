# Player Clarity Rules

Use this group when changed work affects whether the player can understand and
use what is visible on screen: UI/UX, HUD, scene elements, sprites, interactive
elements, feedback, goals, rewards, danger, or responsive layout.

This group checks player-facing clarity. It does not judge art direction, asset
readiness, input-system correctness, or game-loop design.

## Use For

- visible player-facing screens and gameplay scenes;
- HUD, menus, overlays, buttons, and other interactive elements;
- feedback after player action;
- visible goals, rewards, danger, state changes, and next action;
- click/touch target clarity and responsive layout.

## Not For

- art direction, style, composition, or polish: use [Art](../art/README.md);
- asset license, provenance, or runtime format: use [Assets](../assets/README.md);
- input event delivery, focus, keybinds, pointer capture, or platform input:
  use [Technical](../technical/README.md);
- core loop, rewards, progression, or motivation: use
  [Game Design](../game_design/README.md).

## Order

1. Start with [COMMON.md](COMMON.md).
2. Add numbered checks only when their "Use When" section matches the task.

## Checks

### [QCLR_COMMON - Player Clarity Common](COMMON.md)
Checks obvious player-facing clarity blockers: blank output, unreadable text,
hidden actions, invisible important objects, misleading overlap, or no
actual-output proof.

Use first for any player-facing output change.

### [QCLR_001 - Screen Clarity](checks/QCLR_001_screen_clarity.md)
Checks whether the player understands the current screen/state and next action.

Use when a screen, gameplay scene, onboarding step, feedback state, or task flow
changed.

### [QCLR_002 - Responsive Layout](checks/QCLR_002_responsive_layout.md)
Checks whether layout and click/touch targets stay visible, reachable, and
non-overlapping.

Use when viewport, target surface, responsive layout, or click/touch target
geometry changed.

### [QCLR_003 - Player-Visible Runtime Invariants](checks/QCLR_003_player_visible_runtime_invariants.md)
Checks hard runtime invariants for player-facing text, rendering, and layout.

Use when runtime visual code changed and could affect text rendering,
debug/final visual boundaries, or Y-up/Y-down layout boundaries.

Record applied checks in the task log as `Quality: QCLR_001=pass` or
`Quality: QCLR_001=block`.
