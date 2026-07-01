# Templates

Reusable game starting points live here as `templates/<template-id>/`.

There can be multiple templates. Register each usable template in
`templates/templates.json` so AI Studio surfaces and
`ai_studio/bootstrap/new_game.mjs --template <template-id>` can find it.

Creating a game copies the selected template into `games/<game-id>/`. After
that copy, the game owns its files; later template edits do not automatically
change existing games.

Optional reusable feature packs live in `features/`. Copy a feature into a
template when every game from that template should inherit it, or into a game
when only that game needs the feature.

Current registered template list:

- `templates/templates.json`
