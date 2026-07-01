# Source Routing

Load this reference before adding, reviewing, reorganizing, or promoting
game-design research sources and reusable knowledge.

## Load First

1. Use the already-loaded root rules.
2. Read `ai_studio/game_design/knowledge_base/README.md`, `ai_studio/game_design/knowledge_base/knowledge/README.md`,
   `ai_studio/game_design/knowledge_base/sources/README.md`, and `ai_studio/game_design/knowledge_base/knowledge/index.md`.
3. If references guide gameplay, UI, economy, balance, or final art, also read
   `ai_studio/game_design/knowledge_base/knowledge/reference_deconstruction.md`.
4. For templates, read `templates.md` only when creating or substantially
   rewriting source/knowledge files.

## Routing

- `ai_studio/game_design/knowledge_base/sources/`: raw or near-raw source notes for reusable
  cross-project knowledge. Keep facts close to the original material.
- `ai_studio/game_design/knowledge_base/knowledge/`: reusable principles, checklists, methods, and
  validation rules. Do not put current-game facts here.
- `games/<game-id>/design/knowledge/`: private game knowledge base. Put
  accepted game-specific conclusions, reference lessons, playtest findings,
  build observations, and implementation-facing facts here.
- `games/<game-id>/design/knowledge/sources/`: source material that supports one
  game, one GDD, one balance file, one art direction, or one current
  implementation decision.
- `games/<game-id>/design/`: project-specific GDDs, examples, reference studies,
  evidence, screenshots, playtest notes, UI flow, balance, and content data.
- `ai_studio/taskboard/items/`: work status and deferred tasks.
- `tmp/`: temporary extraction, screenshots, transcripts, rejected material,
  scratch scripts, and audit logs.

If a note names the active game, local balance numbers, screenshots, current
implementation status, accepted GDD facts, or playtest results, keep it in the
game design folder unless it has been rewritten as a reusable rule.
