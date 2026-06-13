# Game Seed

Clean AI-first game project base for the next concept.

This repository is currently a neutral testbed:

- no active game concept is selected;
- old game-specific design, task history, content, assets, and product files
  are removed;
- reusable AI workflow lives in `AI_PIPELINE.md`, `AGENTS.md`, `tasks/`, and
  `.codex/skills/`;
- reusable design knowledge lives in `gamedesign/knowledge/`;
- game code starts at `src/main.c`;
- universal AI runtime support is kept: DevAPI, screenshot capture, state
  codegen, save/load, migrations, and smoke scenarios.

The engine is connected as a git submodule at `external/neotolis-engine`.

## Build

```powershell
git submodule update --init --recursive
cmake --preset native-debug
cmake --build --preset native-debug
```

Native executable:

```text
build/game_seed/native-debug/game_seed.exe
```

## Run

```powershell
build/game_seed/native-debug/game_seed.exe
```

The placeholder screen is intentionally minimal. Click or press `Space` to
cycle colors. `Esc` quits native builds.

## DevAPI

Native debug builds expose the agent command bus:

```powershell
build/game_seed/native-debug/game_seed.exe --devapi 9123 --fresh-state --disable-autosave
```

Useful checks:

```powershell
py -3.12 tools/devapi/smoke_test.py 9123
py -3.12 tools/devapi/full_probe.py 9123
py -3.12 tools/project_67_world/devapi_scenarios/state_roundtrip.py 9123
```

State schema source: `state/game_state.schema.json`. Generated C files are
derived from it by `tools/state_codegen/generate_state.py`.

## Web

Use this when the task targets web/mobile behavior.

```powershell
cmake --preset wasm-debug
cmake --build --preset wasm-debug
powershell -ExecutionPolicy Bypass -File scripts/serve-web.ps1 build/game_seed/wasm-debug 8080
```

Open `http://localhost:8080/index.html`.
