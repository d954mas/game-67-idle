# Game-local AI Studio Workspace

This folder is copied into every new game. It stores game-owned Studio data
that should travel with the game instead of the parent Studio repository.

- `workspace.json` describes the game-local store roots.
- `taskboard/items/` is for game-local task cards.
- `evidence/` is for game-local validation evidence and reports.

Canvas working projects stay in the shared external Canvas root. Link them to
the game by setting each Canvas project's `ownership.kind/gameId`.
