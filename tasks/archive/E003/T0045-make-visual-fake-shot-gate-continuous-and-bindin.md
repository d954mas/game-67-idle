---
id: T0045
title: Make visual fake-shot gate continuous and binding definition of done
status: done
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

- [x] `AGENTS.md` validation section redefines "done" as screen-vs-fake-shot match first; probes/manifests/provenance demoted to supporting evidence ("A game that passes every automated check but does not look like the fake shot or is not fun is NOT done").
- [x] `game-feature-iteration` requires the visual-first freeze: no system/state/route/content/automation expansion while the first-screen visual gate is failing.
- [x] The visual gate runs each visual iteration (continuous gate), comparing the native screenshot to the named fake shot, treating divergence as a failing build.
- [x] The binding rule is stated once in AGENTS.md (canonical); the skill references it instead of restating (further dedup in T0048/T0051).
- [x] `node tools/skills_eval.mjs` (9/9) + `node tools/taskboard/cli.mjs validate` (ok) pass.

## Open questions

## Log

- 2026-06-15: Created from full pipeline review. Mechanisms existed (T0020/T0027/T0030/T0032) but were advisory/post-hoc; both prototypes shipped ugly screens behind green probes.
- 2026-06-15: Added the binding definition of done to AGENTS.md Validation (screen-vs-fake-shot first + continuous visual gate + visual-first freeze as default, not rescue). Wired `game-feature-iteration` step 6 to apply it (continuous gate, freeze, "core moment feels right"). Also brought skill steps 2/12 in line with T0044 (profiling optional/advisory). skills_sync regenerated `.claude`; skills_eval 9/9; taskboard validate ok. "Feels right" procedure itself is T0046.
