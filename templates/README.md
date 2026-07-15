# Templates

Reusable game starting points live here as `templates/<template-id>/`.

There can be multiple templates. Give each usable template a `template.json` so
AI Studio surfaces and `games/new_game.mjs --template <template-id>` can find it
by scanning `templates/<template-id>`.
Each template also owns `game-dependencies.json`, the explicit engine/feature
seed used to create exact game-owned dependency records.
Use `node templates/new_template.mjs --id <template-id>` when creating
a new reusable template; it registers the template and refreshes VS Code
build/run entries.

Creating a game copies the selected template into `games/<game-id>/`. After
that copy, the game owns its files; later template edits do not automatically
change existing games.

New games can be created from a template as either public/tracked or private:

```powershell
node games/new_game.mjs --id <game-id> --template <template-id> --visibility public
node games/new_game.mjs --id <game-id> --template <template-id> --visibility private
```

Use `--visibility public` for a tracked game with parent Taskboard and generated
VS Code entries. Use `--visibility private` for a nested private repository
under `games/private/<game-id>/`; that ignored path keeps game-local taskboard,
canvas, evidence, and workspace state inside the game.

The template includes a `design/` scaffold with `concept.md`, `gdd.md`,
`knowledge/`, and starter structured data. It also includes a game-owned
`.ai_studio/` scaffold with `workspace.json`, `taskboard/items/`,
`canvas/projects/`, and `evidence/`. A new game gets its private GDD,
knowledge base, local task/canvas/evidence roots, and asset workspace contract
by copying this template.

Feature ownership rules live in `features/README.md`. Reusable `*-core` modules
compile in place from `features/`; template-owned feature implementations copy
with the template and become game-owned. Follow each module's `INSTALL.md`
rather than assuming every feature is copied.

`.vscode/tasks.json` and `.vscode/launch.json` are generated from scanned public
template and game folders.
