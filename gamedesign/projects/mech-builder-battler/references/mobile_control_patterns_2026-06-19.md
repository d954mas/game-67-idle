---
type: Project Source Notes
title: Mobile Control Patterns For Mech Builder Battler
description: Project-specific notes comparing virtual joystick, floating joystick, drag zone, and tap-to-move control patterns.
tags: [project, references, controls, mobile, web, mechs]
timestamp: 2026-06-19T00:00:00Z
---

# Mobile Control Patterns

Scope: clarify first-input options for `mech-builder-battler` before choosing
the native PC slice control scheme.

Lead decision on 2026-06-19:

- Mobile target: floating virtual joystick / drag movement zone.
- Native PC harness: WASD movement as the development/playtest adapter for the
  same positioning intent.

## Source Matrix

| Source | Link/Path | Quality | Checked | Proves | Does Not Prove |
|---|---|---|---|---|---|
| Mech Arena battle controls support | `https://mecharena-support.plarium.com/hc/pt/articles/12575581245340-Controles-e-Ajustes-de-Batalha` | official support screenshot/article | 2026-06-19 | Mech Arena uses a left circular movement joystick plus right-side weapon/ability buttons and battle-control customization | Whether this exact layout is best for casual PvE or our first slice |
| Mech Arena Google Play | `https://play.google.com/store/apps/details?id=com.plarium.mechlegion&hl=en_US` | official store page | 2026-06-19 | The game markets intuitive TPS controls and customizable controls for short mech battles | Exact first-session control tutorial |
| Brawl Stars iMore guide | `https://www.imore.com/brawl-stars` | secondary guide with screenshot | 2026-06-19 | Distinguishes joystick mode from tap-to-move: joystick mode uses a virtual joystick dragged anywhere; tap-to-move taps destination and drags/releases to aim/fire | Current live Brawl Stars control state or mech-specific suitability |
| Brawl Stars Interface In Game screenshot | `https://interfaceingame.com/screenshots/brawl-stars-controls/` | UI screenshot archive | 2026-06-19 | Shows mobile action UI control customization, including draggable movement control and right-side buttons | Design intent or performance data |

## Observations

- `observed`: Mech Arena support imagery shows a left circular movement control
  and right-side fire/ability controls.
- `observed`: Mech Arena store copy says the game has intuitive TPS controls
  and lets players customize controls.
- `secondary`: The Brawl Stars guide says joystick mode means moving with a
  virtual joystick by dragging anywhere on screen, while tap-to-move means
  tapping a destination and dragging/releasing to aim/fire.
- `observed`: Brawl Stars control customization screenshots show movable
  control elements, reinforcing that mobile action games often let players
  reposition the movement/control widgets.

## Pattern Definitions

### Fixed Virtual Joystick

What it is:

- A visible joystick sits in a fixed place, usually lower left.
- Player drags inside it to move.

Pros:

- familiar;
- clear in screenshots;
- easiest to explain visually.

Cons:

- can feel like a heavy shooter control;
- takes fixed screen space;
- less forgiving if the thumb starts outside the control.

### Floating Virtual Joystick

What it is:

- Player touches a movement zone.
- The joystick origin appears under the finger or near the touch point.
- Drag direction controls movement.

Pros:

- still a real virtual joystick;
- more forgiving on phones;
- works well as "drag anywhere on the left side";
- maps cleanly to mouse drag on native PC.

Cons:

- less obvious in a static fake shot unless drawn as a translucent movement
  zone or ghost joystick;
- needs good tutorial feedback.

### Drag Zone

What it should mean in our docs:

- Not a separate control family.
- It is the broad interaction zone used by a floating virtual joystick.

Preferred naming:

- `floating virtual joystick / drag movement zone`.

Avoid using only `drag zone`, because it sounds like a different control model
from joystick and caused confusion.

### Tap-To-Move

What it is:

- Player taps a destination.
- The character/mech moves toward it.

Pros:

- simple for slower tactical games;
- reduces continuous thumb control.

Cons:

- can feel indirect for mech piloting;
- works worse with moving enemies and hazards unless the combat is more
  tactical/auto-battler;
- less similar to Mech Arena style TPS controls.

## Application To Mech Builder Battler

Borrow:

- left movement control plus right action buttons from mobile action grammar;
- customizable/mobile-readable control areas;
- broad movement zone that can become a floating joystick.

Avoid:

- precise manual aim;
- too many fire buttons;
- dense TPS HUD copied from Mech Arena;
- hiding the movement affordance in fake shots.

Copy-risk:

- do not copy exact Mech Arena HUD layout, icons, labels, or button positions;
- do not copy Brawl Stars control art style.

Current-build mismatch:

- no native PC mech slice exists yet, so there is no current control proof.

Next proof screenshot/scenario:

- native PC battle screen with a left `floating virtual joystick / drag
  movement zone`, two right-side action buttons, auto-target highlight, and
  phone-scale readability check.

## Recommendation

For the first playable, phrase the input as:

> Floating virtual joystick on a broad left-side movement zone, with auto-target
> combat and two large right-side action buttons.

This keeps the familiar mobile joystick grammar while staying more forgiving
than a small fixed stick.

For the native PC harness, use WASD for movement. Do not treat WASD as a
separate design direction; it is the local iteration adapter for the accepted
mobile control model.
