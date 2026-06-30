# Player Clarity Rules

Use this group when changed work affects whether the player can understand and
act on visible output: UI/UX, HUD, scene elements, sprite readability or state
clarity, feedback, goals, danger, state changes, responsive viewports, or
virtual controls.

This group checks player-facing clarity. It does not judge art direction, asset
readiness, input-system correctness, or game-loop design.

## Not For

- art direction, style, composition, or polish: use [Art](../art/README.md);
- asset license, provenance, or runtime format: use [Assets](../assets/README.md);
- input event delivery, focus, keybinds, pointer capture, or platform input:
  use [Technical](../technical/README.md);
- core loop, rewards, progression, or motivation: use
  [Game Design](../game_design/README.md).

## Checks

### [QCLR_001 - Player Clarity](checks/QCLR_001_player_clarity.md)

Checks: changed screen/HUD/feedback/transition/flow does not make the player
misread state or next action.

Use when: the player could misread what happened, what matters now, or what to
do next.

### [QCLR_002 - Responsive Viewports](checks/QCLR_002_responsive_viewports.md)

Checks: 4:3, 16:9, and tall-phone viewports keep game, HUD, text, and actions
visible.

Use when: viewport ratio or orientation changes can crop, hide, overlap, or
misplace player-facing layout.

### [QCLR_003 - Virtual Controls](checks/QCLR_003_virtual_controls.md)

Checks: mobile controls are visible, understandable, reachable, and do not block
important gameplay or HUD information.

Use when: virtual buttons, joystick zones, or touch control hints can be hidden,
too small, overlapping, misleading, or unreachable. If control layout depends on
orientation, also use QCLR_002.

Record applied checks in the task log using the outcome format from the Quality
README.
