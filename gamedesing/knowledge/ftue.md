# FTUE

First Time User Experience design for casual games.

## Goal

The first session should let a new player understand the core loop by doing it,
not by reading a manual. The player should always know one useful next action,
see an immediate result, and reach a small reward quickly.

## Core Principles

- Put the first meaningful action on screen immediately.
- Show one primary next action at a time until the core loop is understood.
- Teach by interaction first; use text only to confirm or clarify.
- Make the first reward visible, audible, or animated.
- Delay secondary systems until the player has completed the basic loop.
- Use recognition over recall: keep needed actions and goals visible.
- Let the player recover from wrong taps without punishment.
- Make every tutorial step prove a mechanic that will still matter later.

## GDD Checklist

- First screen: what does the player see before touching anything?
- First action: what is the obvious tap/click?
- First feedback: what moves, changes, counts up, or reacts?
- First reward: what does the player receive in the first 15-30 seconds?
- First spend or upgrade: when does the player turn reward into progress?
- First goal: what single objective is visible after the first action?
- First lock: what is withheld, and is the unlock reason clear?
- First mistake: what happens if the player taps the wrong thing?
- Text budget: what is the maximum copy shown before the first action?
- Exit rule: can the player skip, close, or ignore tutorial prompts?

## Recommended Sequence

```text
See goal
  -> perform one obvious action
  -> get immediate feedback
  -> collect first reward
  -> spend or apply reward
  -> see visible improvement
  -> receive next goal
```

## Anti-Patterns

- Explaining multiple systems before the first action.
- Showing three equally bright calls to action on the first screen.
- Covering the play area with tutorial text.
- Teaching a mechanic that disappears after the tutorial.
- Unlocking meta systems before the core loop has been completed.
- Giving a reward that changes only an invisible stat.
- Using disabled buttons without a clear reason.

## Validation

- In a still screenshot, the next action is obvious without explanation.
- A new player performs a meaningful action within 5 seconds.
- A new player reaches the first reward within 15-30 seconds.
- A new player reaches the first spend, upgrade, or visible improvement within 30-90 seconds.
- The player can describe the core loop in one sentence after the first minute.
- The tutorial still works when prompts are skipped or clicked quickly.

## Links

- Use [Core Loop](core_loop.md) to define the first repeatable action before tutorializing it.
- Use [UI/UX Patterns](ui_ux_patterns.md) for screen hierarchy and feedback rules.
- Use [Meta Progression](meta_progression.md) when the FTUE introduces upgrades, unlocks, or currencies.

## References

- Nielsen Norman Group, [10 Usability Heuristics](https://www.nngroup.com/articles/ten-usability-heuristics/)
- Nielsen Norman Group, [Recognition and Recall in User Interfaces](https://www.nngroup.com/articles/recognition-and-recall/)
- Game Accessibility Guidelines, [Basic guidelines](https://gameaccessibilityguidelines.com/basic/)
