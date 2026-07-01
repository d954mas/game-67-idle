---
type: Game Design Knowledge
title: Accessibility
description: Reusable rules for readable, controllable, recoverable game experiences.
tags: [accessibility, ux, validation]
timestamp: 2026-06-13T00:00:00Z
---

# Accessibility

Reusable accessibility rules for casual game design.

## Goal

Accessibility should make the intended experience easier to perceive, understand,
control, and recover from. It is not a separate mode added at the end; it shapes
input, readability, feedback, pacing, and validation from the first playable slice.

## Core Principles

- Essential information should not rely on color alone.
- Essential feedback should not rely on sound alone.
- Interactive elements should be large, well spaced, and visually distinct.
- Text should use simple wording, readable size, and strong contrast.
- Players should be able to progress through instructional text at their own pace.
- Avoid flicker, excessive screen shake, and constant distracting motion.
- Provide clear current objectives and control reminders when needed.
- Allow recovery from mistakes without punishment that breaks learning.
- Expose options for volume, motion, speed, and difficulty when the game needs them.

## GDD Checklist

- What information is conveyed by color, sound, motion, text, or icon?
- What is the non-color backup for each important state?
- What is the visual backup for important sound feedback?
- What text must be readable on the smallest viewport?
- What inputs require precision, speed, repeated tapping, or simultaneous actions?
- What can be slowed, skipped, retried, or assisted?
- What motion, flashing, or camera behavior can be reduced?
- What settings must be available before gameplay starts?
- What accessibility checks are part of playtest validation?

## Common Design Patterns

- Color plus icon plus label for important states.
- Captions or visible effects for important audio cues.
- Progress bars with labels, not fill alone.
- Clear locked-state reason next to the locked action.
- Press/tap actions instead of hold, mash, or multi-touch when possible.
- One objective visible at a time during FTUE.
- Reduced-motion fallback for decorative movement.
- Difficulty or assist changes that can be made during play.

## Anti-Patterns

- Red/green as the only distinction between bad and good states.
- Tiny text inside generated images.
- Tutorial text that advances before the player can read it.
- Required rapid tapping before the player understands the loop.
- Sudden motion or flashing used as decoration.
- Important sound cues without visible confirmation.
- Accessibility settings hidden behind several menus.
- Treating children or casual players as if they can read dense system text.

## Validation

- The first screen is understandable without sound.
- Important states remain distinguishable in grayscale.
- Text and controls remain readable at the smallest supported viewport.
- The core loop can be completed without precision input unless precision is the core challenge.
- Tutorial prompts can be read at the player's pace.
- Motion-heavy feedback can be reduced while preserving meaning.
- At least one playtest pass explicitly checks accessibility risks.

## Links

- Use [FTUE](ftue.md) to reduce early cognitive load.
- Use [UI/UX Patterns](ui_ux_patterns.md) for state clarity and readable components.
- Use [Visual Direction](visual_direction.md) for contrast, silhouettes, and motion rules.
- Use [Mobile/Web Platform Design](mobile_web_platform.md) for touch and viewport constraints.
- Use [Playtest Validation](playtest_validation.md) for evidence capture.

## References

- Game Accessibility Guidelines, [Full list](https://gameaccessibilityguidelines.com/full-list/)
- W3C WAI, [WCAG 2.2 Contrast Minimum](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html)
- W3C WAI, [WCAG 2.2 Target Size Minimum](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html)

