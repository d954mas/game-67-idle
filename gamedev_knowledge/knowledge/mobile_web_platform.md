---
type: Platform Design Knowledge
title: Mobile/Web Platform Design
description: Reusable design constraints for touch, viewport, browser, and resume behavior.
tags: [mobile, web, platform, ux]
timestamp: 2026-06-13T00:00:00Z
---

# Mobile/Web Platform Design

Reusable design rules for games that may run on phones, tablets, desktop browsers,
or embedded web portals.

## Goal

Platform design should make the same core experience playable across input
methods, screen sizes, browser constraints, and interrupted sessions. The GDD
should state what must adapt before implementation starts.

## Core Principles

- Design for touch first when mobile is a target.
- Do not rely on hover, right click, tiny cursors, or keyboard-only shortcuts.
- Keep the primary action reachable and readable on the smallest supported viewport.
- Treat orientation, aspect ratio, safe area, and browser UI as design constraints.
- Avoid requiring precise timing or repeated rapid input unless that is the core skill.
- Make pause, resume, mute, fullscreen, and interrupted sessions predictable.
- State what happens when the game loses focus, reloads, or resumes.
- Keep the first load path short and visually responsive.

## GDD Checklist

- Supported viewports and orientations.
- Primary input methods: touch, mouse, keyboard, controller, scripted input.
- Minimum target size and spacing.
- Safe area behavior and edge controls.
- Text size and wrapping rules for the smallest viewport.
- Portrait/landscape layout priority.
- Loading, pause, resume, reload, and reconnect behavior.
- Audio start and mute behavior.
- Save/checkpoint behavior after short sessions.
- Performance budget that affects design: particles, animation density, asset size, object count.
- What validation must run on desktop, mobile viewport, and real or emulated touch.

## Layout Patterns

- Primary action near the natural thumb zone when touch is central.
- Status and resources pinned where they do not cover gameplay.
- Bottom navigation only when it does not compete with the primary action.
- Modal dialogs with clear close/confirm states and no hidden off-screen actions.
- Dense screens split into tabs only after the core loop is understood.
- Scrolling lists with stable item height and obvious affordance.

## Anti-Patterns

- Desktop layout scaled down until text and buttons become tiny.
- Controls placed under browser chrome, safe areas, or gesture zones.
- Gameplay that requires hover tooltips to understand.
- First session blocked by settings, account flow, or multi-step menus.
- Effects or particles that hide input targets on small screens.
- Critical information visible only in landscape when portrait is supported.
- Saves that require long sessions before persistence.

## Validation

- First-screen screenshot works at the smallest supported viewport.
- Primary action is reachable and readable with touch.
- The core loop works with touch and mouse.
- No critical UI element overlaps browser or safe-area edges.
- Text does not clip or require impossible precision.
- The game recovers from focus loss, reload, and short interruption.
- Performance-heavy effects can be reduced without breaking feedback.

## Links

- Use [UI/UX Patterns](ui_ux_patterns.md) for component states and hierarchy.
- Use [Accessibility](accessibility.md) for input, readability, and cognitive load.
- Use [Playtest Validation](playtest_validation.md) for viewport and input evidence.
- Use [Iteration Scope](iteration_scope.md) to validate one platform slice at a time.

## References

- W3C WAI, [WCAG 2.2 Target Size Minimum](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html)
- Game Accessibility Guidelines, [Full list](https://gameaccessibilityguidelines.com/full-list/)
- Apple, [Human Interface Guidelines: Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility)

