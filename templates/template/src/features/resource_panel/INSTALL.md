# Resource Panel Install

The reference template already compiles this pointer feature. A new game gets
the complete folder through `games/new_game.mjs` and owns that copy.

For an existing game, copy this folder, add `resource_panel.c` to its target,
include this directory, and call `resource_panel_ui` from game composition.
Verify the game UI tests and `node features/validate_contracts.mjs`.

To uninstall, remove the composition call, source/include wiring, and this
folder. No state migration or asset removal is required.
