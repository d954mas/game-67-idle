---
id: T0016
title: Eval primary-gdd-pipeline behavior after trim (skill-eval-playbook)
status: done
epic: E003
priority: P2
tags: [ai-pipeline, qa]
created: 2026-06-11
updated: 2026-06-12
---

## What

Evaluate the trimmed `primary-gdd-pipeline` skill against its behavior playbook
and add a small regression guard so future edits do not remove the key
activation triggers or output/process anchors.

## Done when

- [x] `tools/skills_eval.mjs` checks `primary-gdd-pipeline` activation triggers.
- [x] The eval checks core behavior anchors for DoD, visual proof, first slice,
  machine-readable contracts, handoff, review, and validation.
- [x] Current repo and a fresh export pass skill eval and task validation.
- [x] Task log records any gaps found and whether the skill needed patching.

## Open questions

## Log

- 2026-06-12: Started T0016. Scope: static regression guard for
  `primary-gdd-pipeline` based on `references/skill-eval-playbook.md`; no game
  GDD changes.
- 2026-06-12: Added `primary-gdd-pipeline` activation and behavior anchors to
  `tools/skills_eval.mjs`. No gap required patching `SKILL.md`; the existing
  trimmed skill still contains the required DoD, visual proof, first playable
  slice, machine-readable contract, handoff, review, and validation anchors.
- 2026-06-12: Evidence passed: `node tools/skills_eval.mjs`;
  `node tools/taskboard/cli.mjs validate`; `node --test tools/taskboard/test.mjs`;
  `node tools/bootstrap/export_base.mjs --target tmp/export-primary-gdd-eval-test-20260612`;
  in the exported project, `node tools/skills_eval.mjs` and
  `node tools/taskboard/cli.mjs validate`.
