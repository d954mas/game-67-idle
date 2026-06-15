---
id: T0045
title: Make visual fake-shot gate continuous and binding definition of done
status: backlog
epic: E003
priority: P0
tags: [visual, quality, gate, ai-workflow]
created: 2026-06-15
updated: 2026-06-15
---

## What

The fake-shot / product-read gate already exists (T0020 visual-first, T0027
first-slice artifact, T0030 visual-strict rubric, T0032 critic packet) but
stayed advisory and post-hoc, so both prototypes passed automation while the
rendered screen was rejected: "There was no visual acceptance checklist that
could fail the build" (rune review :127-131); "Validators prove consistency,
not quality" (primary-gdd-pipeline:114). Make the visual match the definition of
done: every visual iteration scores the native screenshot against the named fake
shot, and system/state/content/automation expansion is FROZEN until the first
screen passes. This is the core fix for problem A (games unlike refs).

## Done when

- [ ] `AGENTS.md` validation section redefines "done" as screen-vs-fake-shot match first; probes/manifests/provenance demoted to supporting evidence.
- [ ] `game-feature-iteration` (and gdd/visual skills) require: no system/content expansion while the current first-screen visual gate is failing.
- [ ] The visual gate runs each visual iteration (not only as a rescue), comparing the native screenshot to the named fake shot, with an explicit pass/fail verdict.
- [ ] The rule is stated once (coordinate with T0048/T0051 dedup), not duplicated across skills.
- [ ] `node tools/skills_eval.mjs` + `node tools/taskboard/cli.mjs validate` pass.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Mechanisms existed (T0020/T0027/T0030/T0032) but were advisory/post-hoc; both prototypes shipped ugly screens behind green probes.
