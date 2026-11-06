---
name: nt-design-knowledge
description: "Maintain cited game-design sources and reusable knowledge; route game-specific findings into that game's private design knowledge."
---

# NT Design Knowledge

Maintain the requested design evidence or knowledge in its owning location.

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

## Approach And Evidence

Use existing notes and accepted evidence when they cover the request. Consult
`ai_studio/game_design/knowledge/README.md` when ownership is unclear. Load only
references needed for the current routing or evidence decision.

Keep reusable principles in `ai_studio/game_design/knowledge_base/knowledge/`
and game-specific findings in `games/<game-id>/design/knowledge/`. When external
evidence supports a new conclusion, record its source before promoting it.
For a focused correction, update the affected notes and links; update indexes
when discoverability changes. Verify citations support the claims and report gaps.

## Rules

- Do not put current-game facts, screenshots, accepted GDD facts, status, or
  playtest results in reusable knowledge.
- Do not use search snippets, thumbnails, memory, or store copy as mechanics
  proof.
- Claim "grounded in refs" only with durable deconstruction evidence.
- If evidence is incomplete, state the gap instead of presenting it as ready.
- Keep work status in Taskboard, not in design data files.
