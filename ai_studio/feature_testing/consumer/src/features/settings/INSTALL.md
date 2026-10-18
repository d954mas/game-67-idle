# Settings Install

The integration test consumer compiles this reference. Copy this folder and
`state/settings.schema.json` explicitly when a game needs the settings example.

For an existing game, copy this folder and the settings state fragment, add the
sources to its target, register the fragment, and dispatch
`settings_draw_launcher` / `settings_draw_panel` from the game-owned scene
catalog inside
game composition. The copied feature requires the generated settings fragment,
`game_save`, the game scene catalog, engine `nt_ui`, and the game UI theme;
wire those include/link dependencies explicitly. Verify settings/save tests and
`node features/validate_contracts.mjs`.

To uninstall, remove composition and build wiring, then remove persisted fields
through the owning game's normal state-migration policy.
