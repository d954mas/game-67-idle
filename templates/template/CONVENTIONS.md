# Foundation conventions

- `main.c` only sequences the shell phases (`src/shell/shell.h`): platform
  begin, tick, present, platform end. Each phase owns its file; a game grows a
  phase file, never `main.c`. No phase contains game rules or presentation
  details that a feature or renderer could own.
- `World` owns transient game data. State that must survive a restart belongs
  in a schema fragment under `state/`.
- A feature owns one coherent rule set. `game_features.c` calls its lifecycle
  phases in a stable order.
- Rule updates happen before presentation. Renderer and UI modules read state;
  they do not mutate gameplay state.
- Add an asset only when a feature uses it, with its provenance in the asset
  pipeline. The blank foundation has no game assets.
- Use engine public APIs before adding custom platform, save, rendering, or UI
  infrastructure.
- Skip drawing while the graphics context is lost. A feature with persistent
  GPU resources restores them on the engine's `context_restored` signal before
  drawing. Web recovery is acknowledged only after presenting a restored frame.

Optional integration examples live in `ai_studio/feature_testing/consumer`.
