# Resource Panel Install

The integration test consumer compiles this reference. Copy the folder and
wire its declared dependencies explicitly when a game needs the widget.

For an existing game, copy this folder, add `resource_panel.c` to its target,
include this directory, and call `resource_panel_ui` from game composition.
Verify the game UI tests and `node features/validate_contracts.mjs`.

To uninstall, remove the composition call, source/include wiring, and this
folder. No state migration or asset removal is required.
