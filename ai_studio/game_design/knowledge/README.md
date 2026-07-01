# Design Knowledge Workflow

Workflow docs for routing design sources and reusable game-design knowledge.

## Role

This group tells agents where design evidence belongs and how to promote source
material without mixing reusable knowledge with current-game facts.

It owns workflow, not content:

- reusable design knowledge content: `ai_studio/game_design/knowledge_base/knowledge/`;
- reusable source notes: `ai_studio/game_design/knowledge_base/sources/`;
- current-game design docs, private knowledge, references, screenshots,
  playtest notes, and GDD facts: `games/<game-id>/design/`;
- work state: Taskboard;
- temporary extraction and rejected material: `tmp/`.

## Files

- `source_routing.md`: routing rules between source shelf, reusable knowledge,
  private game knowledge, game design folders, tasks, and temp scratch.
- `source_intake_promotion.md`: source quality, evidence labels, and promotion
  workflow.
- `reference_work_review.md`: reference-driven design work and review.
- `templates.md`: source note, knowledge page, project note, and source review
  report templates.

Use `nt-design-knowledge` as the skill surface for this workflow.
