---
id: T0015
title: Research and improve AI art iteration pipeline
status: review
epic: ""
priority: P1
tags: [art,pipeline,skills,research]
created: 2026-06-12
updated: 2026-06-12
---

## What

Research faster AI/game-art iteration practices and convert the lesson into
project rules so generated art becomes reusable runtime assets faster.

## Done when

- [x] External references and local skill/agent practices are summarized.
- [x] A reusable project knowledge doc describes the faster art loop.
- [x] Visual and asset pipeline skills encode the new rules.
- [x] Skill/task validators pass.

## Open questions

None for the current research pass.

## Log

- 2026-06-12: Researched Unity/Godot UI slicing, Unity sprite atlas and
  Addressables content workflows, Scenario's AI art workflow claims, and local
  agent-harness/skill patterns. Added
  `gamedesign/knowledge/ai_art_iteration_pipeline.md`, updated
  `AI_PIPELINE.md`, `AI_PIPELINE_ITERATION_LOG.md`,
  `.codex/skills/game-visual-art-direction/SKILL.md`,
  `.codex/skills/game-asset-pipeline/SKILL.md`, and
  `tools/skills_eval.mjs`.
- 2026-06-12: Validation passed: `node tools/skills_sync.mjs`,
  `node tools/skills_eval.mjs`, and `node tools/taskboard/cli.mjs validate`.
- 2026-06-12: Measured the explicit pack path and used it in native runtime:
  `py -3.12 tools/assets/build_67_world_art.py`,
  `cmake --build --preset native-debug --target build_67_world_packs`, and
  `build/game_seed/native-debug/build_67_world_packs.exe build/game_seed/67-world-packs`.
  Cached pack build reported `Cache: 7 hit / 0 miss` and builder timing
  `0.0s`.
- 2026-06-12: Fixed a DevAPI PPM conversion edge case that caused fallback
  window screenshots during art QA. Added the evidence-source lesson to
  `AI_PIPELINE_ITERATION_LOG.md` and
  `gamedesign/knowledge/ai_art_iteration_pipeline.md`.
- 2026-06-12: Second research pass after user feedback that the art loop was
  still too slow. Compared Agent Skills progressive disclosure, agent harness
  feedback loops, prompt-chain gates, GameUIAgent structured JSON specs,
  SPRITE's YAML-to-engine-assets approach, and SpriteToMesh's hybrid
  learned/algorithmic split. Added
  `gamedesign/knowledge/ai_art_pipeline_research_2026.md`,
  `tools/assets/new_art_job.mjs`, and new skill/eval rules for art jobs,
  candidate batches, shared manifests, pack commands, and native evidence.
  Updated the portable exporter so the scaffold tool travels with future
  projects.
- 2026-06-12: Full pipeline validation exposed a brittle Node -> `py -3.12`
  launcher failure. Hardened `tools/pipeline_validate.mjs` to probe and use a
  working Python runner before state codegen.
