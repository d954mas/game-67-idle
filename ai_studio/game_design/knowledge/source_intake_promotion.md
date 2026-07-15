# Source Intake And Promotion

Load this reference when parsing articles, videos, reference packets, store
pages, gameplay footage, talks, or other game-design research sources.

## Source Intake

For every non-trivial source, record:

- title, author/publisher if known, link or local path, and checked date;
- source quality: official/store/trailer, gameplay footage, walkthrough/guide,
  review/community, deconstruction/analysis, academic/industry talk,
  user-provided, or unknown;
- what the source proves and what it does not prove;
- evidence labels for claims: `observed`, `secondary`, `inferred`, or
  `unknown`;
- exact timestamps, frame numbers, screenshot paths, sections, or quotes when a
  claim depends on visual/gameplay evidence.

Do not use search snippets, thumbnails, memory, or store copy as mechanics
proof. If browsing or source access is missing, write the evidence gap plainly.

## Promotion Workflow

1. Decide scope before editing: shared reusable knowledge, private game
   knowledge, raw source shelf, or task/status.
2. Add or update the source note first when external evidence matters.
3. Extract only reusable conclusions into `ai_studio/game_design/knowledge_base/knowledge/`, or
   game-specific conclusions into `games/<game-id>/design/knowledge/`.
4. Link the conclusion back to its source note when the source matters.
5. Update the knowledge README only when entry-point guidance changes.
6. Do not create a new knowledge page if a focused edit to an existing page is
   clearer.
