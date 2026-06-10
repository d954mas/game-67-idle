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
- Use [FTUE](ftue.md) to reduce first-session UI complexity.
- Use [Meta Progression](meta_progression.md) for upgrade, currency, and unlock state rules.

## References

- Nielsen Norman Group, [10 Usability Heuristics](https://www.nngroup.com/articles/ten-usability-heuristics/)
- W3C WAI, [WCAG 2.2 Target Size Minimum](https://www.w3.org/WAI/WCAG22/Understanding/target-size-minimum.html)
- Game Accessibility Guidelines, [Full list](https://gameaccessibilityguidelines.com/full-list/)
- Apple, [Human Interface Guidelines: Accessibility](https://developer.apple.com/design/human-interface-guidelines/accessibility)
