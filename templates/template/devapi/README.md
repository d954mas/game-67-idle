# Template DevAPI Bots

This folder is copied into every new game. Put game-owned runtime bots,
smoke tests, and scenario scripts here.

## Two-build convention (human vs agent)

- `build/native-debug` — the HUMAN build (VS Code tasks): `GAME_DEVAPI_ENABLED=OFF`,
  plain window title. No automation surface — the lead plays this one by hand.
- `build/devapi-debug` — the AGENT build: configure with `-DGAME_DEVAPI_ENABLED=ON`;
  window title gets an ` [AI]` suffix so both windows are distinguishable side by side.
  **Configure it with Ninja and clang**, the way the gate build is configured:

  ```
  cmake -S . -B build/devapi-debug -G Ninja \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Debug -DGAME_DEVAPI_ENABLED=ON
  ```

  The Visual Studio generator cannot build this tree: MSVC rejects the engine's
  `/Wextra` and dies on `nt_crc32`. The failure is quiet in the worst way — the
  directory keeps whatever `game.exe` was linked last, so a bot run against it
  measures a build nobody made. If a before/after pair comes back suspiciously
  identical, check the binary's timestamp before trusting the numbers.
  A directory already configured with the wrong generator has to be deleted and
  configured again; CMake will not switch one in place.
- The preset name encodes the split on purpose: engine libs land in
  `build/engine/<preset>`, so one shared preset would let the two builds
  overwrite each other's `nt_input` (with/without inject symbols).
- Agent scripts must point at `build/devapi-debug/bin/game.exe`, never at the
  human build.

The shared transport/client code stays in `ai_studio/runtime_automation/`.
Game scripts import that client, then add semantic game actions here. Do not
move reusable Python helpers into the skill bundle, and do not duplicate engine
commands in this folder.

## Smoke Bot

Run after building a native Debug template:

```powershell
node ai_studio/dev_environment/python_run.mjs templates/template/devapi/smoke_bot.py --exe templates/template/build/devapi-debug/bin/game.exe
```

Or through CMake when the build directory was configured with
`GAME_DEVAPI_ENABLED=ON`:

```powershell
cmake --build templates/template/build/devapi-debug --target devapi_smoke
```

The bot:

1. starts the game with `--devapi`;
2. discovers live commands through `endpoints`;
3. checks command metadata with `command.describe`;
4. waits for `ui.tree`;
5. verifies the stable `settings/gear` UI id is visible;
6. toggles the engine render gate with `render.set_enabled`;
7. captures a PNG proof image;
8. reads `game.state.schema` and `game.state.get` from the in-place
   `game-state` feature;
9. reads the DevAPI-only `game.iteration.proof` leaf-C/generated-schema fixture
   pair used by the shared trustworthy iteration helper.

Use it as the pattern for real game bots:

- keep low-level launch, frame, input, and capture calls in the shared client;
- put game-specific actions in this folder;
- use stable UI ids from `ui.tree`, not labels or array indexes;
- follow observe -> act -> `frame.wait` -> observe;
- use `game.state.get` as the default raw-state assertion point, or a semantic
  `game.action.*` command when the scenario checks gameplay rules;
- write screenshots or JSON summaries to ignored `tmp/` paths.

## Responsive Viewport Evidence

Use this when a QCLR_002 responsive-viewport check needs screenshots:

```powershell
cmake --build templates/template/build/devapi-debug --target quality_responsive
```

That default target captures the first screen in:

- 4:3 landscape + portrait;
- 16:9 landscape + portrait;
- tall-phone 19.5:9 portrait + landscape.

For a specific game state, call the helper from a bot or pass a scenario hook:

```powershell
node ai_studio/dev_environment/python_run.mjs templates/template/devapi/responsive_viewports.py `
  --exe templates/template/build/devapi-debug/bin/game.exe `
  --scenario games/my-game/devapi/scenarios.py:prepare_upgrade_menu
```

The hook is called as `prepare(game, viewport)` after launch and warmup but
before `ui.tree` and screenshot capture. Use it to open menus, trigger combat,
advance tutorial state, or otherwise reach the moment that needs QCLR_002
evidence.

From a game bot, the same helper can be used directly:

```python
from pathlib import Path
from responsive_viewports import DEFAULT_VIEWPORTS, run_matrix
from devapi_client import running_game

def prepare_upgrade_menu(game, viewport):
    game.result("time.step", {"frames": 5})
    game.click_ui("settings/gear", observe=None)
    game.wait_frames(2)
    return {"state": "upgrade_menu", "viewport": viewport.window_size}

run_matrix(
    lambda viewport: running_game(window_size=viewport.window_size),
    DEFAULT_VIEWPORTS,
    Path("tmp/quality/qclr_002_upgrade_menu"),
    prepare=prepare_upgrade_menu,
)
```

Pixel-health audit is opt-in for this helper (`--audit`), because QCLR_002 needs
layout evidence and some valid UI states are visually quiet enough to fail a
generic contrast/variance threshold.

## Tests

```powershell
node ai_studio/dev_environment/python_run.mjs -m unittest discover -s templates/template/devapi -p "*_test.py"
```
