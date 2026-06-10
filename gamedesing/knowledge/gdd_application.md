# GDD Application

Reusable workflow for turning knowledge-base rules into a project-specific GDD.

## Goal

The knowledge base should not be copied wholesale into a project. It should help
an agent or designer produce clear, project-specific decisions with player-visible
behavior, tuning knobs, and validation criteria.

## When To Use

- Starting a new feature spec.
- Revising an unclear GDD section.
- Preparing a handoff to implementation.
- Converting playtest findings into design changes.
- Checking whether a proposed mechanic has enough detail to build.

## Input Checklist

- Project goal or feature goal.
- Target player and platform.
- Relevant current GDD files.
- Relevant knowledge files.
- Existing implementation constraints.
- Known open questions.
- Required validation method.

## Workflow

1. Identify the design problem in one sentence.
2. Pick the smallest relevant knowledge files.
3. Extract only the checklist items that affect the current problem.
4. Write project-specific answers using the game's actual names, numbers, screens, and assets.
5. Mark unknowns as open questions instead of inventing certainty.
6. Define expected player-visible behavior.
7. Define tuning knobs separately from fixed rules.
8. Define validation evidence before implementation starts.
9. Link back to source knowledge files for future review.

## Output Template

```md
## Feature / System Name

### Goal
Why this exists for the player.

### Player Behavior
What the player does, sees, chooses, repeats, avoids, or learns.

### System Behavior
What the game does in response.

### Feedback
What changes on screen, in audio, in UI state, or in progression.

### Tuning Knobs
Numbers likely to change during balancing.

### Content
Required items, states, copy, art, audio, and unlocks.

### Validation
How to prove this works in a build or playtest.

### Open Questions
Unresolved decisions that should block or shape implementation.

### References
Links to project GDD files and knowledge files.
```

## Decision Quality Checklist

- The section names a player-visible outcome.
- The first-time experience is covered if the feature can appear early.
- Rewards and errors have feedback.
- Locked, disabled, active, ready, and completed states are defined if relevant.
- Balance knobs are named and easy to find.
- Content requirements are explicit enough for art/UI/code work.
- Validation is executable, not just "test it."
- The scope can be cut without losing the core loop.

## Anti-Patterns

- Copying a generic checklist into the GDD without project-specific answers.
- Writing lore or mood where implementation rules are needed.
- Mixing confirmed decisions with brainstorms.
- Hiding tuning numbers in prose.
- Specifying mechanics without feedback.
- Specifying rewards without validation.
- Leaving "make it fun" as a requirement.

## Links

- Use [Core Loop](core_loop.md) for the smallest repeatable action.
- Use [FTUE](ftue.md) for first-session decisions.
- Use [Meta Progression](meta_progression.md) for long-term systems.
- Use [UI/UX Patterns](ui_ux_patterns.md) for screen and component states.
- Use [Design Review](design_review.md) before implementation handoff.
- Use [Iteration Scope](iteration_scope.md) to define the smallest playable pass.
- Use [Telemetry Evidence](telemetry_evidence.md) to define proof and diagnostics.
