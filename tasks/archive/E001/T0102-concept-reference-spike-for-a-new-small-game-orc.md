---
id: T0102
title: Concept + reference spike for a new small game
status: done
epic: E001
priority: P2
tags: [design, research]
created: 2026-06-22
updated: 2026-06-22
---

## What

Produce a one-page concept + reference brief for a NEW small original game
(merge / idle / cozy — your pick; the previous game is finished). The brief must
cover, with enough grounding to act on:

- concept + core loop + first-screen (one location, primary path, next action,
  visible progress);
- currencies / early progression;
- an art direction with 2–3 concrete visual references;
- a CC0/CC-BY asset + UI-font shortlist, each with license + provenance;
- a short map of how this concept uses what the pipeline already provides;
- open questions.

Read-only / advisory: put all output in `tmp/orchestration-test/`. Do not change
runtime/game code (`src/`, `state/`), generated packs, or hot docs (`AGENTS.md`,
`AI_PIPELINE.md`, `tasks/STATUS.md`, `tasks/README.md`, skills).

## Done when

- [ ] `tmp/orchestration-test/concept-brief.md` covers every section above, with
      license + provenance for each asset/font and a real pipeline-mapping
      section (not generic).
- [ ] No changes outside `tmp/orchestration-test/` and this task file
      (verify with `git status`).
- [ ] T0102 moved to `review`.

## Open questions

## Log

- orchestration: used
  objective: Produce a grounded concept + reference brief (tmp/orchestration-test/concept-brief.md) for a new small idle game ("Sporelight"), backed by a real map of this repo's pipeline and a verified CC0/CC-BY asset + font shortlist.
  allowed files: tmp/orchestration-test/**, tasks/active/T0102-concept-reference-spike-for-a-new-small-game-orc.md
  tool-use guard: verify exact repo paths with rg --files/Test-Path before reads; use Select-Object -Skip/-First for line windows; keep evidence commands read-only
  expected output: concept-brief.md covering concept/loop/first-screen, currencies/progression, art direction + 3 refs, CC0/CC-BY asset + font shortlist with license+provenance, real pipeline mapping, open questions
  evidence command: git status (boundaries) ; node tools/ai.mjs status --agents (delegation telemetry) ; node tools/taskboard/cli.mjs validate
  stop condition: brief covers every required section with per-item license+provenance and a non-generic pipeline map; changes confined to tmp/orchestration-test/ + this task file
  independent reviewer: lead (human) at review — advisory brief, no runtime/code change to gate
- Fanned out 3 read-only Explore subagents in parallel: P1 codebase-map state+automation (state/**, tools/state_codegen/**, src/game_storage.*, DevAPI), P2 codebase-map asset+visual gate (tools/**, src/**, gamedesign/**), R Opus asset/license research (web). Integrated solo into the brief.
- Brief delivered: tmp/orchestration-test/concept-brief.md. Read-only/advisory; no changes outside tmp/orchestration-test/ + this file.
