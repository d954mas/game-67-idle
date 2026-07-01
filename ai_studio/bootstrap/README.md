# Bootstrap

AI Studio bootstrap owns project/game creation and portable AI Studio export.

## Role

- `new_game.mjs`: copy `templates/<template-id>/` into `games/<game-id>/` and
  register that game's asset root through Asset Storage. Use `--template <id>`
  to pick a registered template by id, or `--from <path>` for an explicit
  folder.
- `new_template.mjs`: copy an existing template into `templates/<template-id>/`
  and register it as a reusable template.
- `vscode_projects.mjs`: regenerate `.vscode/tasks.json` and
  `.vscode/launch.json` from `templates/templates.json` and `games/games.json`.
- `export_base.mjs`: copy the reusable AI Studio base into another project.
- `TEMPLATE.md`: human explanation of the template copy model.

Bootstrap may call Asset Storage helpers, but it does not own asset manifests,
license policy, previews, search, or viewer behavior.

## Commands

```powershell
node ai_studio/bootstrap/new_template.mjs --id <template-id>
node ai_studio/bootstrap/new_game.mjs --id <game-id>
node ai_studio/bootstrap/new_game.mjs --id <game-id> --template <template-id>
node ai_studio/bootstrap/vscode_projects.mjs
node ai_studio/bootstrap/export_base.mjs --target C:\projects\new-game
node --test ai_studio/bootstrap/new_template.test.mjs ai_studio/bootstrap/new_game.test.mjs ai_studio/bootstrap/vscode_projects.test.mjs ai_studio/bootstrap/export_base.test.mjs
```

## Boundary

Keep compatibility wrappers out unless a real external workflow still needs
them. Current repo docs and tests should point to `ai_studio/bootstrap/`.
