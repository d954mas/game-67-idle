---
type: Game Design Knowledge
title: UI/UX Patterns
description: Reusable patterns for readable game UI, hierarchy, controls, and states.
tags: [ui, ux, patterns]
timestamp: 2026-06-13T00:00:00Z
---

# UI/UX Patterns

Reusable UI/UX rules for readable casual game screens.

## Goal

The interface should make state, action, reward, and progress readable at a
glance. A player should know what they can do now, why they should do it, and
what changed after they did it.

## Screen Hierarchy

Use this order unless the game has a strong reason to differ:

1. Current player state or main fantasy number.
2. Primary action.
3. Current goal.
4. Main reward or resource.
5. Secondary actions.
6. Locked or future content.

## Core Principles

- One screen should have one visually dominant action.
- Every interactive element needs a stable component state.
- Feedback should appear near the action first, then travel or resolve into the affected resource.
- Icons need labels until the player has learned them.
- Do not rely on hover-only behavior for games that may run on touch devices.
- Use large targets and generous spacing for primary actions.
- Keep critical text outside generated bitmap art where possible.
- Avoid layout shifts when text, counts, timers, badges, or effects update.
- Locked, affordable, active, ready, claimed, and disabled states must be visually distinct.
- Error or blocked states should explain what to do next.

## Game UI Surface Taxonomy

Separate screens by job before drawing panels:

- `persistent_hud`: always-needed state such as health, currency, energy, or
  current phase.
- `contextual_hud`: appears only when the player can act on it now.
- `primary_action`: the one strongest action for the current screen or FTUE
  step.
- `secondary_actions`: useful but visually quieter actions.
- `modal_decision`: interrupting choice, purchase, reward claim, settings, or
  confirmation.
- `objective_feedback`: current goal, blocker, reward, and next step.
- `decoration`: theme and material language only; it must not compete with the
  action hierarchy.

If these jobs are not named, art direction will usually overproduce decorative
frames and underproduce readable interaction.

## Runtime UI Asset Taxonomy

Generate and integrate these as separate reusable families:

- `backgrounds`: screen backdrops, parchment, vignette, map layers, scenery.
- `slice9_bases`: blank panels, buttons, slots, tabs, chips, and tooltips.
- `decor_overlays`: corner caps, top plaques, gems, screws, locks, crests,
  dividers, glow strips, selected rings.
- `icons`: semantic symbols with consistent silhouette and padding.
- `state_overlays`: hover, press, disabled, locked, selected, affordable,
  claimable, cooldown, warning.
- `bars`: track, fill, caps, marker, glow, disabled overlay, runtime label.
- `runtime_text`: labels, values, timers, quest text, localization, debug text.
- `hit_targets`: invisible/tinted layout rectangles, often larger than art on
  touch screens.

The same visual richness can remain, but it should be assembled from reusable
parts instead of baked into one full panel image.

## Slice9 Design Rules

- Slice9 bases should be structurally simple: protected corners, straight or
  repeatable edges, and a calm center/fill.
- Unique plaques, gems, medallions, lock plates, banners, cap ornaments, and
  state effects belong in `decor_overlays` or `state_overlays`, not in
  stretch zones.
- Content safe area is part of the asset. Text and icons must not collide with
  corners, grime, screws, or decorative caps.
- Minimum usable size is part of the asset. A button that works at 240x72 may
  be invalid at 128x48.
- Preview hostile sizes: minimum, normal, large, very wide, very tall, and the
  smallest supported portrait composition.
- If an edge only looks good at source size, it is a static mockup edge, not a
  reusable slice9 edge.

## Atlas And Reuse Rules

- Pack by runtime lifetime and screen family, for example `ui_common`,
  `ui_panel_family`, `ui_icons_core`, `ui_map`, and `ui_fx`.
- Store metadata with every runtime asset: semantic id, kind, pack group,
  source crop, atlas rect, trim rect, original size, pivot/anchor, slice9
  margins, content safe area, state role, source family, scale variant, and
  usage policy.
- Use trim only with alpha bleed, premultiplied-alpha-safe resizing, extrusion,
  border padding, and shape padding. Tight crops without padding cause halos
  and neighboring-pixel leaks.
- Do not rotate slice9 assets in an atlas unless the runtime explicitly knows
  how to rotate slice margins. Default is `allow_rotation: false`.
- Prefer overlays and aliases over duplicated full controls: base button +
  selected overlay + locked overlay + icon + runtime label is usually cheaper
  and more flexible than four full button PNGs.
- Alias identical pixels when two semantic ids share the same art. Duplicate
  meaning in metadata, not bitmap storage.

## Component State Checklist

For each repeated UI component, define:

- `normal`: visible and usable.
- `pressed`: immediate response to input.
- `disabled`: cannot be used; reason is visible if important.
- `locked`: future content; one clear unlock condition.
- `affordable`: can be bought or started now.
- `active`: currently running or selected.
- `ready`: reward or claim is available.
- `completed`: done and no longer the next action.
- `new`: recently unlocked; attention pulse plays once, then stops.

## GDD Checklist

- What is the primary action on each screen?
- What is the resource/status area?
- What is the current goal area?
- Which components can be tapped or clicked?
- What are the minimum target sizes and spacing rules?
- What text can wrap, truncate, or resize?
- What animations confirm tap, reward, purchase, unlock, completion, and error?
- How does the UI behave on the smallest supported viewport?
- What visual state indicates locked, affordable, ready, active, and claimed?
- What information must remain visible during modal dialogs or tutorials?

## Useful Patterns

- Next-action card: one visible objective with a direct button or pointer.
- Reward fly-to-resource: reward appears near action, then moves to the resource counter.
- Affordance pulse: affordable or ready state pulses once, not forever.
- Progress bar with label: timers and long goals show both fill and remaining state.
- Badge stack limit: avoid stacking multiple alerts on one component.
- Locked preview: show the reward silhouette/name plus one unlock reason.
- Empty state action: empty screens point to the next useful action.

## Anti-Patterns

- Decorative UI competing with the primary action.
- Same color treatment for locked and ready states.
- Tiny text inside generated images.
- Buttons whose label changes layout size every frame.
- Reward counters that update without local visual feedback.
- Disabled controls with no reason.
- Modal dialogs that hide the thing the player is being asked to understand.
- Multiple pulsing elements in the first session.

## Validation

- Screenshot test: the next action is readable in a still frame.
- Smallest viewport test: no overlap, no cropped critical text, no unreachable control.
- Input test: all primary controls work with mouse, touch, and scripted input when applicable.
- Target test: important controls meet or exceed the project's minimum target size.
- State test: locked, affordable, active, ready, completed, and disabled states are distinguishable.
- Feedback test: every purchase, reward, unlock, and error has visible feedback.

## Links

- Use [Core Loop](core_loop.md) to decide which action, feedback, and reward must dominate the screen.
- Use [Playtest Validation](playtest_validation.md) to verify screenshots, input, and state clarity.
- Use [Reward Feedback](reward_feedback.md) for reward, blocked, error, ready, and completion feedback.
- Use [Visual Direction](visual_direction.md) for palette, silhouette, and readability rules.
- Use [Accessibility](accessibility.md) for contrast, input, readable text, and backups.
- Use [Mobile/Web Platform Design](mobile_web_platform.md) for viewport and touch constraints.
- Use [FTUE](ftue.md) to reduce first-session UI complexity.
- Use [Meta Progression](meta_progression.md) for upgrade, currency, and unlock state rules.

## References

- Nielsen Norman Group, [10 Usability Heuristics](https://www.nngroup.com/articles/ten-usability-heuristics/)
- W3C WAI, [WCAG 2.2 Target Size Minimum](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html)
- Game Accessibility Guidelines, [Full list](https://gameaccessibilityguidelines.com/full-list/)
- Apple, [Human Interface Guidelines: Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility)
