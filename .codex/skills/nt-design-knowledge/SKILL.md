---
name: nt-design-knowledge
description: "Use when adding, reviewing, reorganizing, or promoting game-design research sources and design knowledge: source notes, article/video/reference packet parsing, reusable `ai_studio/game_design/knowledge_base/knowledge/` rules, private `games/<game-id>/design/knowledge/` routing, source hygiene review, knowledge index/log updates, or reference-backed design claims."
---

# NT Design Knowledge

Use this skill to route design sources and reusable game-design knowledge.

Canonical workflow docs live in `ai_studio/game_design/knowledge/`.

## Load Only What Applies

- `ai_studio/game_design/knowledge/source_routing.md`: where source notes,
  reusable knowledge, private game knowledge, tasks, and temp artifacts belong.
- `ai_studio/game_design/knowledge/source_intake_promotion.md`: how to parse
  sources and promote conclusions without losing evidence labels.
- `ai_studio/game_design/knowledge/reference_work_review.md`: how to handle a
  named reference that guides gameplay, UI, economy, balance, art, or
  implementation decisions.
- `ai_studio/game_design/knowledge/templates.md`: load only when creating or
  substantially rewriting source notes, knowledge pages, project notes, or
  review reports.

## Default Workflow

1. Read `ai_studio/game_design/knowledge/README.md`.
2. Route before editing: reusable source shelf, reusable knowledge, private game
   knowledge, task state, or temp scratch.
3. Add or update source notes before conclusions when external evidence matters.
4. Keep reusable principles in `ai_studio/game_design/knowledge_base/knowledge/`; current-game facts
   stay in `games/<game-id>/design/knowledge/` or other focused files under
   `games/<game-id>/design/`.
5. Link conclusions back to source notes, update index/log files when
   discoverability changes, and state evidence gaps.

## Rules

- Do not put current-game facts, screenshots, accepted GDD facts, status, or
  playtest results in reusable knowledge.
- Do not use search snippets, thumbnails, memory, or store copy as mechanics
  proof.
- Claim "grounded in refs" only with durable deconstruction evidence.
- If evidence is incomplete, state the gap instead of presenting it as ready.
- Keep work status in Taskboard, not in design data files.
