# Game-local AI Studio Workspace

This folder stores game-owned Studio data that should travel with the game
instead of the parent Studio workspace.

- `workspace.json` describes the game-local store roots.
- `taskboard/items/` contains game-local task cards.
- `evidence/` contains local validation evidence and is ignored by the parent
  repository by default.

Canvas working projects live in the shared external Canvas root and carry their
owning game in each Canvas `project.json` as `ownership.kind/gameId`.
