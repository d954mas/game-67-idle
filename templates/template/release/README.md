# Release scaffold

The template keeps game-owned web build, package, verification, and CI tooling
so a new game can ship after it adds gameplay. The blank foundation itself has
no release assets.

Run `node tools/game.mjs test --template-proof`,
`node tools/game.mjs playable --target itch`, or
`node tools/game.mjs verify --target itch --template-proof`.

`verify` builds the selected target, packages it, reopens the ZIP, and checks
the browser artifact. Use the target's platform feature only when the game
requires its runtime behavior.
