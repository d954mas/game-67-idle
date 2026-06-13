---
type: Game Design Knowledge
title: Visual Direction
description: Reusable rules for friendly, readable game visual direction.
tags: [visual, art-direction, readability]
timestamp: 2026-06-13T00:00:00Z
---

# Visual Direction

Reusable rules for readable, friendly casual-game visual direction.

## Goal

Visual direction should make the game instantly legible and desirable to touch.
The screen should communicate player fantasy, available action, reward value,
and progress without requiring explanation.

## Core Principles

- Shape language should match the audience and fantasy before adding detail.
- Important objects need strong silhouettes at gameplay size.
- Use color to group meaning, but never make color the only signal.
- Keep gameplay objects, UI controls, rewards, and background visually separated.
- Bright does not mean low contrast; text and controls still need readable value contrast.
- Avoid grey, technical, placeholder-like screens unless that is the explicit fantasy.
- Use visual hierarchy: primary action first, then goal, reward, secondary options.
- Motion should confirm state changes, not create constant visual noise.
- Major progression should change the world, character, board, or UI in visible ways.

## Style Brief Checklist

- Audience: who should find this instantly appealing?
- Fantasy: what role or transformation should the player feel?
- Shape language: soft, chunky, toy-like, sharp, realistic, miniature, etc.
- Palette: primary, secondary, warning, success, locked, premium, background.
- Materials: plastic, candy, paper, clay, plush, metal, glass, paint, etc.
- Scale rule: how large are characters, resources, rewards, and buttons relative to the screen?
- Silhouette rule: how does each object read in one color?
- Feedback rule: how do tap, reward, unlock, error, and completion look?
- Accessibility rule: contrast, text size, color-blind-safe signals, flicker limits.
- Asset boundary: what belongs in generated art, and what must remain editable UI/text?

## Casual/Toy-Like Patterns

- Chunky props with simple silhouettes.
- High-value rewards drawn larger than their physical logic requires.
- Soft shadows and clear outlines to separate interactable objects.
- Saturated accent colors balanced by calm background areas.
- Large readable numbers, badges, and resource icons.
- Small idle animations that make the screen feel alive without stealing focus.
- Progression shown through visible accumulation, decoration, area growth, or character change.

## Anti-Patterns

- Background detail competing with the primary action.
- Thin lines, tiny labels, or low-contrast grey UI.
- Important text baked into bitmap art where it cannot resize or localize.
- Same color and shape language for reward, warning, and disabled states.
- Constant motion on non-interactive elements near the primary action.
- Visual polish that hides weak feedback or unclear mechanics.
- Adult, corporate, or technical UI language for a casual/kids-facing game.

## Validation

- First-screen screenshot communicates genre and primary action.
- Main objects remain identifiable at the smallest supported viewport.
- Text and critical UI meet project contrast/readability rules.
- Interactable objects look more clickable/tappable than decorations.
- Rewards look more desirable than costs, locks, and neutral props.
- The scene still reads when color is removed or viewed quickly.
- Visual changes after progression are obvious in before/after screenshots.

## Links

- Use [UI/UX Patterns](ui_ux_patterns.md) for component states and screen hierarchy.
- Use [Reward Feedback](reward_feedback.md) to make rewards visible and desirable.
- Use [Content Planning](content_planning.md) to define visual roles for items, areas, and rewards.
- Use [Accessibility](accessibility.md) to keep color, contrast, motion, and sound usable.
- Use [FTUE](ftue.md) to keep early screens visually focused.
- Use [Playtest Validation](playtest_validation.md) to verify screenshots and readability.

## References

- Game Accessibility Guidelines, [Full list](https://gameaccessibilityguidelines.com/full-list/)
- W3C WAI, [WCAG 2.2 Contrast Minimum](https://www.w3.org/WAI/WCAG22/Understanding/contrast-minimum.html)
- W3C WAI, [WCAG 2.2 Target Size Minimum](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html)
