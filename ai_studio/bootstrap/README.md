# Bootstrap

AI Studio bootstrap owns project/game creation and portable AI Studio export.

## Role

- `new_game.mjs`: copy `template/` into a new game folder and register that
  game's asset root through Asset Storage.
- `export_base.mjs`: copy the reusable AI Studio base into another project.
- `template_paths.mjs`: document the template copy model and reusable/seed/game-only
  path groups.
- `TEMPLATE.md`: human explanation of the template copy model.

Bootstrap may call Asset Storage helpers, but it does not own asset manifests,
license policy, previews, search, or viewer behavior.

## Commands

```powershell
node ai_studio/bootstrap/new_game.mjs --id <game-id>
node ai_studio/bootstrap/export_base.mjs --target C:\projects\new-game
node --test ai_studio/bootstrap/new_game.test.mjs ai_studio/bootstrap/export_base.test.mjs
```

## Boundary

Keep compatibility wrappers out unless a real external workflow still needs
them. Current repo docs and tests should point to `ai_studio/bootstrap/`.
