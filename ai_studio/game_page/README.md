# Game Page

Read-only Studio Shell surface for one game: overview, builds, pack contents,
balance, canvases, state schemas, and captures in one place.

## Role

The page aggregates what the studio already knows about a game and links into
the owning tools (Items Workbench, Canvas, Asset Viewer, Taskboard). It never
mutates game data and never launches builds or games.

## Owned Here

- `ops.mjs` — read-only aggregation over workspace mounts and game folders.
- `api.mjs` — HTTP adapter mounted by Studio Shell on `/api/game-page/`.
- `site/` — the browser page served at `/game/<game-id>`.

## Routes (served by Studio Shell)

- `/game/<game-id>` — the page.
- `/game-file/<game-id>/<relative-path>` — read-only file access confined to
  that game's root (design docs, previews).
- `/api/game-page/games` — all games, private included; the home page list.
- `/api/game-page/overview?game=<id>` — one game's header data.

## Boundary

Game data parsing that has an owning module stays there (items via the Items
Workbench ops, canvases via the canvas stores). This module only reads folder
facts nobody else owns: build configs, pack files, capture files, schemas.
