---
type: Knowledge Guide
title: UI Scaling Across Screens
description: How engines, apps and portals decide how big an interface is on a phone, a tablet and a monitor, and where the studio's ui-kit sits among them.
tags: [ui, responsive, scaling, touch-targets, research]
timestamp: 2026-09-11T00:00:00Z
---

# UI Scaling Across Screens

One design has to be readable on a phone held in either orientation and on a
monitor. Three industries answer this differently, and the difference is not
taste: apps and games optimise for different things.

## The three strategies, named

Unity's Canvas Scaler is the one place all three are spelled out as options:

- **Constant Pixel Size** — "Makes UI elements retain the same size in pixels
  regardless of screen size."
- **Constant Physical Size** — "Makes UI elements retain the same physical size
  regardless of screen size and resolution", relying on the device reporting
  DPI.
- **Scale With Screen Size** — "Makes UI elements bigger the bigger the screen
  is."

Apps pick the second. Games pick the third. Nobody ships the first on purpose.

## Games: scale against ONE edge, and prefer the short one

**Godot** states the rule outright: *"To support both portrait and landscape
mode with a similar automatically determined scale factor, set your project's
base resolution to be a square (1:1 aspect ratio) instead of a rectangle"* — a
square base means the scale is decided by the short edge, because that is the
edge that runs out first in either orientation.

**Unreal (UMG)** offers DPI Scale Rules of Shortest Side, Longest Side,
Horizontal and Vertical, and marks **Shortest Side** the "Most Common Setting".
It does not scale linearly: the rule evaluates a **DPI curve** the designer
authors, mapping a short-side resolution to a scale multiplier. The curve is the
interesting part — it lets the interface grow sub-linearly, so a monitor does not
get a HUD scaled up by its full pixel count.

**Unity** scales against a reference resolution with a Match slider between
width and height; community practice is Match = 0 for portrait and 1 for
landscape, which is the same statement as "match the short edge" for a single
orientation. Unity's own guidance pairs scaling with **anchoring**: anchor
elements to the corners so the layout adapts to aspect, rather than expecting
scale to do it.

## Apps: do not scale at all — keep physical size, change the layout

Android's **window size classes** are the canonical formulation. Breakpoints are
on the available window, in dp: width compact `< 600`, medium `600–840`,
expanded `840–1200`, large `1200–1600`, extra-large `≥ 1600`; height compact
`< 480`, medium `480–900`, expanded `≥ 900`. Two rules matter more than the
numbers:

- They are *"explicitly not determined by the size of the device screen … not
  intended for isTablet-type logic"* — they follow the window the app actually
  gets.
- *"The window size class can change throughout the lifetime of your app"* —
  rotation, split screen, folding.

So orientation never enters the decision, and the interface's physical size never
changes. What changes is arrangement.

Touch targets are then fixed in physical units, not in screen shares:

- Apple HIG: **44 × 44 pt** minimum.
- Material: **48 × 48 dp**, chosen because it lands at *about 9 mm* on any
  screen.
- WCAG 2.5.5 (AAA): **44 × 44 CSS pixels**.

## Web: fluid between two authored bounds, and ask what is pointing

Two web practices are directly relevant:

- **`clamp(min, proportional, max)`** is the dominant fluid-typography and
  fluid-spacing technique: size scales with the viewport between an authored
  floor and ceiling rather than either being fixed or scaling without limit.
- **`@media (pointer: coarse)`** reports that the *primary* input is "a pointing
  device of limited accuracy, such as a finger on a touchscreen". MDN's own
  example for the feature is enlarging a control for touch and shrinking it for
  a mouse. This is the only signal in the stack that says "this screen is being
  touched" without guessing from size or orientation.

## Portals: Poki

Poki requires the game to "scale to cover the full canvas", scaling
proportionally to 640×360, 836×470 or 1031×580, and on mobile to "cover the full
screen in portrait or landscape, or both for the best experience". It also
requires games to "force mobile control schemes on tablets" — the portal itself
treats input class, not screen size, as what decides the control scheme.

## Where the studio's ui-kit sits

`features/ui-kit` is in the games camp: authored units are a share of the SHORT
edge (Godot's square base, Unreal's Shortest Side), with no floor and no cap, so
the interface owns the same fraction of any window.

The curve Unreal authors is reduced here to **two authored points** — `ref_hand`
and `ref_desk` — and the pick is made by the web's `pointer: coarse`, because
that is the signal that survives a phone held sideways. The bar the pair has to
clear is borrowed from the app world: at `ref_hand`, the `hit` token must still
be 44 CSS pixels on a 390-pixel phone (Apple/WCAG's number), which a test
asserts.

Two known gaps, both numbers rather than layouts:

- **A tablet is a coarse pointer on a big screen.** It currently gets the hand
  reference, so the interface is larger than it needs to be. Unreal's answer is
  the curve; the cheap version here is to interpolate the reference between the
  two points across the short edge in CSS pixels for touch screens only, leaving
  mouse screens on `ref_desk`.
- **Arrangement is still per-game.** `ui_metrics()` reports `portrait` and
  `in_hand`; what the app world does with them — one breakpoint where a row
  becomes a column — is not yet a kit-level pattern.

## Sources

- Unity, Canvas Scaler: https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/script-CanvasScaler.html
- Unity, Designing UI for Multiple Resolutions: https://docs.unity3d.com/Packages/com.unity.ugui@1.0/manual/HOWTO-UIMultiResolution.html
- Godot, Multiple resolutions: https://docs.godotengine.org/en/stable/tutorials/rendering/multiple_resolutions.html
- Unreal Engine, DPI Scaling: https://dev.epicgames.com/documentation/unreal-engine/dpi-scaling-in-unreal-engine
- Android, Window size classes: https://developer.android.com/develop/ui/compose/layouts/adaptive/window-size-classes
- MDN, `pointer` media feature: https://developer.mozilla.org/en-US/docs/Web/CSS/@media/pointer
- Material Design, Accessibility (touch target size): https://m2.material.io/design/usability/accessibility.html
- W3C, WCAG 2.5.5 Target Size (Enhanced): https://www.w3.org/WAI/WCAG21/Understanding/target-size.html
- Poki, Requirements: https://developers.poki.com/guide/requirements-quality
