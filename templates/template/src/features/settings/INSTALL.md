# Settings Install

The reference template already compiles this pointer feature. A new game gets
the folder and `state/settings.schema.json` through `games/new_game.mjs`.

For an existing game, copy this folder and the settings state fragment, add the
sources to its target, register the fragment, and call `settings_draw_ui` from
game composition. Verify settings/save tests and
`node features/validate_contracts.mjs`.

To uninstall, remove composition and build wiring, then remove persisted fields
through the owning game's normal state-migration policy.
