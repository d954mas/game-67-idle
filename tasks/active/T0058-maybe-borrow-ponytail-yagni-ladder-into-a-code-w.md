---
id: T0058
title: Maybe borrow ponytail YAGNI ladder into a code-writing skill
status: idea
epic: E003
priority: P3
tags: [skills, process, decision]
created: 2026-06-15
updated: 2026-06-15
---

## What

The lead asked whether https://github.com/DietrichGebert/ponytail is worth
applying. Assessment: it is a lightweight, advisory prompt plugin (a 6-rung
"laziest senior dev" decision ladder + a few markdown skills + ~50 lines of
state-tracking glue; MIT, ~13.6k stars, actively maintained). It maps to exactly
one slot here — skills/prompts — and its philosophy (YAGNI, prefer
stdlib/native/existing dep/one-liner over new machinery) is the same principle
behind the E003 subtract-not-add cleanup ([[pipeline-subtract-not-add]]).

Verdict: SKIP the dependency (its hooks, statusline, mode-tracker, multi-agent
adapters, and competing `AGENTS.md` are exactly the "second project" overhead we
just trimmed), but the IDEA is worth borrowing. The valuable part is ~5 lines:
a "does this need to exist -> stdlib -> native feature -> existing dep -> one
line -> only then minimal code" ladder, with an explicit "never lazy about
validation, security, error handling, accessibility, or explicitly-requested
features" guard.

## Done when

- [ ] Lead decides: borrow the ladder into `game-feature-iteration` (Implementation Rules) / AGENTS.md Direction, or skip entirely.
- [ ] If borrowed: ~5-line ladder + the "never lazy about X" guard added to one place (not a new skill), skills_eval + taskboard validate pass.

## Open questions

- Is the ladder useful for THIS work (mostly small C game code + node/python tooling), or is it redundant with the existing "smallest playable slice" / "subtract not add" rules already in the skills? Lean: low value-add given those rules already exist; P3.

## Log

- 2026-06-15: Captured from the lead's ponytail question. Research verdict: skip the dep, optionally steal the ladder; likely redundant with existing lean-slice rules, so P3.
