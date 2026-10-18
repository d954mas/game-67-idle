# Game features

`game_features.c` is the composition root for game-owned features. Its explicit
phases make the boundary explicit:

`init -> update -> update_root -> react -> record -> draw_world -> draw_ui -> shutdown`

Keep game rules in feature modules and keep drawing in renderer or UI modules.
Register a persistent feature's schema fragment from `main.c`; keep
feature-local transient data in `World` or inside the feature.

The blank template contains no optional feature implementation. Copy an
integration reference from `ai_studio/feature_testing/consumer` when a game needs one, then
own that copy in the game.
