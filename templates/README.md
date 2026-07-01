# Templates

Reusable game starting points live here as `templates/<template-id>/`.

There can be multiple templates. Register each usable template in
`templates/templates.json` so AI Studio surfaces and
`ai_studio/bootstrap/new_game.mjs --template <template-id>` can find it.

Creating a game copies the selected template into `games/<game-id>/`. After
that copy, the game owns its files; later template edits do not automatically
change existing games.

Current registered template list:

- `templates/templates.json`
