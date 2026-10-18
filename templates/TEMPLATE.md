# Game starter contract

The default `templates/template` starts with an empty screen. It provides an
engine-backed application lifecycle, input boundary, game-owned state, save/load,
a pack/build path, and places to add logic and presentation. It contains no
sample gameplay, currencies, progression, HUD, settings screen, or sample world.

## Separate ownership

- `template`: the lean default for a new game.
- `ai_studio/feature_testing/`: the independent shared-feature test harness.
  Its consumer owns integration fixtures and optional reference examples.

New games are independent copies. Updating the template does not rewrite an
existing game's state, assets, dependency records, or gameplay.

## Architecture

`src/main.c` owns initialization, frame order, and teardown. Keep gameplay
decisions out of it.

`src/world/world.h` holds game-owned runtime state. Systems own transitions;
rendering reads the result. Persistent state is declared under `state/` and
generated through `features/game-state`; saves and migrations use that module's
public contract.

`src/game_input.*` is the input boundary. Keep platform coordinates and device
events there. Simulation and UI use Y-up coordinates.

`src/features/game_features.*` provides explicit lifecycle phases:
init, update, react, record, draw-world, draw-UI, shutdown. Add direct calls for
installed capabilities. Call order is visible; no automatic registry or plugin
scheduler is required.

`src/render/` owns presentation. The default renderer clears the framebuffer;
it does not spawn entities or manufacture UI. Add real game presentation only
when the game's design needs it. User-visible text uses engine text rendering
and real fonts.

## Optional capabilities

Look in `features/README.md` before implementing a capability. Use engine public
APIs before custom primitives. A shared invariant implementation compiles in
place from `features/`; a reference expected to be repainted is copied and then
owned by its game. Game composition, balance, seed content and save history stay
with the game.

Settings and resource-panel references live under
`ai_studio/feature_testing/consumer/src/features/`. Their INSTALL files state dependencies;
copying the UI alone does not install localization, schemas, theme, or scene
bindings.

Feature maturity does not force default installation. Each template's
`game-dependencies.json` names only the modules its runtime actually compiles,
with exact matching versions. `specified` modules cannot be runtime
dependencies.

## Creation and ownership

Use an explicit visibility choice:

```powershell
node games/new_game.mjs --id <game-id> --template template --visibility private
```

Public games use `--visibility public`. Private game identities, design and
evidence remain in their nested private repositories. Keep heavy authoring files
in the external synchronized workfile workspace; import only shipping assets
with license, provenance, origin and integrity.

The copied `design/` and `.ai_studio/` scaffolds give the game its own design,
knowledge, task/canvas/evidence routing. They do not prescribe a gameplay genre.

## Proof

Run the selected consumer's `tools/game.mjs` from that template or game folder.
The tool owns compiler-output capture and the core/slow/taste test tiers.

Prove the blank starter with a real launch and screenshot: no sample entities,
HUD or unsolicited menus. A uniform clear frame is the expected output. Prove
state/save and creation/build contracts with focused tests.

Shared-feature integration coverage belongs in `ai_studio/feature_testing/` or the feature's
own test consumer. Removing a module from default startup must not silently
remove that module's test coverage.

Release and portal behavior use the owning platform/package contracts and are
verified separately from the blank-screen claim.
