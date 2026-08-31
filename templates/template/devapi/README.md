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

## Driving the game from a bot

A bot is not a player. It advances a run far faster than anyone plays, it does
not look at most of what it renders, and when it does look it has to see the
same thing twice. Three switches make that possible; a game that drops them
gets bots that are slow, or worse, bots that appear to work and prove nothing.

### `--no-vsync` — always, from an agent shell

A window launched from an agent shell never becomes the foreground one, and with
vsync on it presents at roughly **166 ms a frame**. A run that should take
seconds takes minutes. `smoke_bot.AGENT_SHELL_ARGS` carries this; pass it from
every bot you add.

### `render.set_enabled` — while advancing, not while looking

Drawing is the larger half of a fast-forward tick. Measured in one game: **2.26
ms/tick down to 0.45** with the gate off, because the world draw, the UI pass
and the present all disappear.

```python
with game.rendering_off():
    while stats["cleared"] < target:
        game.fast_forward(300, chunk=300)
        stats = game.result("game.planet.stats")
game.fast_forward(4, chunk=4)   # let the drawn frames catch up before shooting
```

The gate is `nt_app_render_enabled()`, and `frame()` in `src/main.c` honours it
around the draw pass. **Keep that.** A game that draws unconditionally accepts
the devapi command and ignores it, which is the worst kind of broken: the bot
reports success and the run stays slow.

Two consequences to know:

- The UI is immediate mode, so **no widget exists while rendering is off**.
  Anything that clicks or photographs must turn it back on and let a frame pass.
- Smoothed presentation state does not advance either, so give the game a few
  drawn frames before a capture.

### `--time-manual` — whenever the bot photographs

A screenshot is a photograph of an animation phase, and the phase follows the
run's clock. Taking manual time over the devapi is **always too late**: real
frames run while the transport comes up, and they advance the world by a dt
nobody chose. `--time-manual` puts the loop in lockstep before its first frame,
so every tick is one the bot asked for. Use `smoke_bot.CAPTURE_ARGS`.

That alone is not enough. Anything the bot waits on — a pack landing, a scene
arriving — takes a variable number of steps, and those steps are on the clock.
Count them and top up to a fixed total:

```python
spent = 0
def advance():
    global spent
    spent += 2
    game.fast_forward(2, chunk=2)
enter_scene(game, ..., advance=advance)
game.fast_forward(ENTRY_STEPS - spent)     # fixed total, whatever the I/O did
```

### What "the same thing twice" is worth

With the clock pinned, one game's capture reproduces **byte for byte**. Without
it, two runs of the same binary differed across **95%** of the frame — a number
worth knowing, because a capture that noisy cannot prove a render change is
safe, and it cannot serve an art review either.

The noise floor once the clock agrees is about **0.007%** of pixels at a channel
delta of 3: driver jitter. Compare with a tolerance, or by hash if the platform
is stable.

### Proving a render change did not move a pixel

Take the shot, change the code, take it again, compare. To build the old code,
do not create a worktree: a game nested under `games/` resolves the engine
above itself and manual Studio copies are not allowed. Swap the files in place
instead.

```
git checkout <ref> -- src/render CMakeLists.txt
cmake --build build/devapi-debug --target game
<capture>
git checkout HEAD -- src/render CMakeLists.txt
```

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
