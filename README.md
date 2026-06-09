# Game 67 Idle

Small Neotolis Engine smoke-test game project. The engine is connected as a git
submodule at `external/neotolis-engine`.

## Controls

- Left mouse drag: rotate the object.
- Mouse wheel: zoom.
- `A` / `D`: previous / next test shape.
- `W`: switch solid, wire, and solid+wire mode.
- `R`: reset rotation.
- `Esc`: quit native build.

## Setup

```powershell
git submodule update --init --recursive
cmake --preset native-debug
cmake --build --preset native-debug --target game_67_idle
```

Native executable:

```text
build/game_67_idle/native-debug/game_67_idle.exe
```

## WASM

Activate Emscripten first, then configure through `emcmake`. The `wasm-debug`
preset uses `MinSizeRel` plus full engine asserts so local web builds stay fast
enough for iteration.

```powershell
emcmake.bat cmake --preset wasm-debug
cmake --build --preset wasm-debug --target game_67_idle
powershell -ExecutionPolicy Bypass -File scripts/serve-web.ps1 build/game_67_idle/wasm-debug 8080
```

Open `http://localhost:8080/index.html`.

## Pack Builder

The pack task verifies that the game can use the engine offline builder while
the engine is a submodule:

```powershell
cmake --build --preset native-debug --target game_67_idle_pack
```

Output:

```text
build/game_67_idle/game_67_idle.ntpack
src/generated/game_67_idle_assets.h
```

The runtime Basis Universal transcoder is disabled by default for this smoke
scene (`GAME_ENABLE_BASISU_RUNTIME=OFF`) so web links stay small. Turn it on
when the game starts loading Basis-compressed textures.

## VS Code

Use `.vscode/tasks.json` and `.vscode/launch.json`:

- `Build: native debug`
- `Run: native debug`
- `Pack: build game pack debug`
- `Pack: build game pack release`
- `Build: wasm debug`
- `Web: serve debug`
- `Release: native PC`
- `Release: web`
- `Web: serve release`
- `Release: all`

Run and Debug configurations:

- `Native Debug (PC)`
- `Native Release (PC)`
- `Web Debug (Chrome)`
- `Web Release (Chrome)`
- `Build Pack Debug`
- `Build Pack Release`

CMake Tools also has explicit build presets:

- `game-native-debug`
- `game-native-release`
- `pack-native-debug`
- `pack-native-release`
- `game-wasm-debug`
- `game-wasm-release`
