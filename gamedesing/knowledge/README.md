# Design Knowledge Base

Reusable design knowledge for game GDD work.

This folder is for universal patterns, checklists, anti-patterns, and references.
Current-game facts, current balance, implemented state, names, jokes, milestone
scope, and product decisions belong in the project's GDD files, not here.

## Use This Folder For

- FTUE and onboarding design rules.
- Meta progression and economy structure.
- UI/UX patterns for readable game screens.
- Reusable review checklists.
- Links to external references worth reusing across projects.

## Do Not Use This Folder For

- Current project state.
- Current sprint tasks.
- Implemented feature status.
- Game-specific lore, characters, currencies, or copy.
- One-off playtest notes unless they become a reusable principle.

## Index

- [FTUE](ftue.md) - first session, first 30 seconds, tutorial-by-doing.
- [Core Loop](core_loop.md) - repeatable action, feedback, reward, and changed state.
- [Meta Progression](meta_progression.md) - long-term goals, economies, unlocks, rewards.
- [UI/UX Patterns](ui_ux_patterns.md) - readable controls, feedback, component states.
- [Playtest Validation](playtest_validation.md) - first minutes, evidence, bugs, director review.
- [Reward Feedback](reward_feedback.md) - visible rewards, consequences, unlock clarity.
- [Visual Direction](visual_direction.md) - readable friendly casual-game visual rules.
- [Balance Tuning](balance_tuning.md) - pacing targets, economy checks, simulations.
- [Content Planning](content_planning.md) - content roles, matrices, scope control.
- [GDD Application](gdd_application.md) - turn reusable knowledge into project specs.
- [Design Review](design_review.md) - pre-implementation GDD review checklist.

## How To Apply

1. Start from the topic file closest to the design problem.
2. Copy only the relevant checklist into the game-specific GDD.
3. Replace generic examples with project-specific names, numbers, and screens.
4. Add validation criteria before implementation starts.
5. If a repeated problem appears in playtests, extract the reusable rule back here.

For feature work, use [GDD Application](gdd_application.md) to write the project-specific
section, then use [Design Review](design_review.md) before implementation handoff.

## Maintenance Rules

- Keep files short enough to scan before a design or implementation pass.
- Prefer checklists and decision prompts over essays.
- Link related knowledge files instead of duplicating sections.
- Keep external links in a `References` section.
- Split a file when one topic starts hiding another topic.

## Contribution Rules

- Add a new file only when an existing file would become less readable.
- Add a rule only if it can change a design decision, review finding, or validation step.
- Keep project-specific examples out; use neutral examples or placeholders.
- If a playtest finding is project-specific, write it in the project GDD first.
- Promote a playtest finding into this folder only when it becomes reusable across games.
- New files should include: `Goal`, practical checklist, anti-patterns, validation, and links.
- Prefer one strong page over many shallow pages.
