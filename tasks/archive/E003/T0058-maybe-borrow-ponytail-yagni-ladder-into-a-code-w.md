---
id: T0058
title: Maybe borrow ponytail YAGNI ladder into a code-writing skill
status: done
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

- [x] Lead chose "apply our own way." Borrowed the idea (not the dependency) as a "build-less ladder" in `game-feature-iteration` Implementation Rules, in one place, no new skill/tool.
- [x] Adapted to this project: rungs map to kept infra (skills, engine, DevAPI, state_codegen, taskboard, installed deps) and stdlib/native, then one small edit, then minimal code; the "never lazy about X" guard maps to OUR non-negotiables (input/state validation, the visual + core-moment bar, security, explicit lead asks). skills_eval 9/9, taskboard ok.

## Open questions

## Log

- 2026-06-15: Captured from the lead's ponytail question. Research verdict: skip the dep, optionally steal the ladder.
- 2026-06-15: Lead said "apply our own way." Added the build-less ladder (5 rungs + non-negotiables guard) to game-feature-iteration Implementation Rules, adapted to this repo's kept infra and quality bar. Dependency NOT adopted (its hooks/statusline/adapters/competing AGENTS.md are the "second project" overhead we trimmed). skills_sync + skills_eval 9/9 + taskboard ok.
