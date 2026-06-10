# Knowledge Capture Playbook

Load this file only when a GDD session produces reusable knowledge: a user
preference, workflow rule, design lesson, validation pattern, or repeated
failure mode worth preserving outside chat history.

## Goal

Capture what worked for this user's process, not generic game design theory.
Keep notes short, linked, and actionable so future agents can reuse them.

## Capture Criteria

Write a durable note only when at least one is true:

- the user corrected the same issue more than once;
- a visual/gameplay direction was accepted or explicitly rejected;
- a workflow rule prevented wasted work;
- a validation step caught a real problem;
- a design pattern will likely apply to future projects;
- the next agent would otherwise rediscover the same context.

Do not capture generic advice, obvious facts, or temporary task state.

## Note Shape

Use compact Markdown:

```markdown
# Topic

## Rule
[One actionable rule.]

## Why
[What happened or what pain this prevents.]

## Applies When
- [condition]

## Links
- [related note/file]

## Example
[Concrete project example, if useful.]
```

## Durable Vs Temporary

- Durable: accepted user preference, reusable GDD workflow, validation rule,
  visual acceptance criteria, known anti-pattern.
- Temporary: current DoD, latest generated image path, failed one-off prompt,
  current tool output, in-progress todo.

Temporary state belongs in `tmp/session_state.md`.

## Obsidian-Style Linking

Prefer short topic files with links:

- `core_loop.md`
- `ftue.md`
- `ui_ux_patterns.md`
- `visual_direction.md`
- `playtest_validation.md`
- `design_review.md`

Use wiki-style links only if the existing knowledge base uses them. Otherwise
use normal Markdown links.

## Update Rules

- Update existing notes before creating new ones.
- Keep each note focused on one topic.
- Add examples only when they clarify a real decision.
- Mark project-specific examples as examples, not universal rules.
- Do not let knowledge notes become a duplicate GDD.

## Review Before Commit

Before committing knowledge notes, verify:

- the note contains an actionable rule;
- it is not just a chat summary;
- it links to relevant design files when useful;
- it does not include temp paths or raw generated artifacts;
- it would help a future agent make a better decision faster.
