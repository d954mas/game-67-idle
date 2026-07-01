# Games

Working games live here as `games/<game-id>/`.

There can be multiple games at the same time. Each game is an independent copy
created from a template or another explicit source folder, then customized in
place.

Register active game asset roots in
`games/games.json`. This lets Asset Viewer and asset tools show game-local
assets alongside template and library sources.

`games/games.json` is not a record of which template a game came from. Template
choice is only used at creation time because the template is copied.

Reusable feature packs can be copied from `features/`, but after copying the
game owns and may customize its local feature code, assets, and state.
