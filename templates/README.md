# Templates

Reusable game starting points live here as `templates/<template-id>/`.

There can be multiple templates. Mount each usable template in
`ai_studio/workspace/catalog.json` and give it a `template.json` so AI Studio surfaces and
`games/new_game.mjs --template <template-id>` can find it.
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

Use `--visibility public` when the game may be registered in tracked Studio
files such as the tracked workspace catalog, parent Taskboard projects, and generated
VS Code entries. Use `--visibility private` for a nested private repository
under `games/<game-id>/`; that path keeps game-local taskboard, canvas, evidence,
and workspace state inside the game and only writes local ignored Studio mount
metadata.

The template includes a `design/` scaffold with `concept.md`, `gdd.md`,
`knowledge/`, and starter structured data. It also includes a game-owned
`.ai_studio/` scaffold with `workspace.json`, `taskboard/items/`,
`canvas/projects/`, and `evidence/`. A new game gets its private GDD,
knowledge base, local task/canvas/evidence roots, and asset workspace contract
by copying this template.

Optional reusable feature packs live in `features/`. Copy a feature into a
template when every game from that template should inherit it, or into a game
when only that game needs the feature.

Current registered template list:

- `ai_studio/workspace/catalog.json`
- `.vscode/tasks.json` and `.vscode/launch.json` are generated from this catalog.
