# Project State

Updated: 2026-06-10
Current HEAD at update: `319eb45 Add studio GDD process gates to skill`

## Current Target

Native desktop is the active development and validation target.

Do not run WASM/web unless the user explicitly asks for it or the task is specifically about web/WASM behavior.

## Current Playable Loop

Current implemented native loop:

`Tap -> upgrades -> jobs -> PIC -> BIK -> STND -> SHOP +90 -> 11/67`

Known gameplay checkpoint:

- `36a3ca0 Build native gameplay loop and DevAPI QA`
- Full native smoke and framebuffer capture passed in that cycle.

Latest tooling checkpoint:

- `40294d9 Capture native DevAPI launch logs`
- Native DevAPI runs now write stdout/stderr launch logs under `build/logs/`.

## How To Run And Validate

Build native:

```powershell
cmake --build --preset game-native-debug --target game_67_idle
```

Native smoke:

```powershell
py -3.12 tools\devapi\smoke_test.py <port>
```

Native state roundtrip:

```powershell
py -3.12 tools\devapi\scenarios\state_roundtrip.py <port>
```

Native capture:

```powershell
py -3.12 tools\devapi\capture_demo.py <port> build\captures\<name>.png --full-loop
```

Launch logs:

```text
build/logs/native_devapi_<port>_*.log
```

DevAPI scripts that use `tools/devapi/devapi_client.py::running_game()` print the launch log path at startup and print the tail automatically on failures.

## Evidence Rules

- For gameplay/UI work, prefer native smoke plus framebuffer capture.
- A screenshot file existing is not enough; use the existing capture path that runs pixel-health.
- On failure, read the launch log tail before diagnosing from screenshots/state alone.

## Current Known Issues

- Reset/audio buttons still look like debug controls.
- First 30 seconds need stronger guidance and reward readability.
- SHOP/stand payoff exists but still uses simple drawn primitives, not final juicy assets.
- Web/WASM is not currently validated.
- Some older tests still contain exact coin assumptions; prefer delta/wait-until checks in new tests.

## Next Priorities

1. Improve first-30-seconds guidance and next-action clarity.
2. Replace dev-like reset/audio buttons with toy-like readable UI controls.
3. Add stronger reward feedback and polished stand/customer assets.
4. Keep native smoke/capture loop fast and reliable.
5. Defer WASM/web validation until core gameplay is ready or the user explicitly asks.

## Repo Traps

- Do not edit `external/neotolis-engine` unless explicitly asked.
- Do not use `git add -A`; this repo has submodule/LFS friction. Use path-limited staging.
- Prefer `git status --short --ignore-submodules=all`.
- Plain broad `git diff --stat` may touch submodule/LFS filters; prefer path-limited diffs.
- Pack building is explicit; do not wire pack generation into every normal build.

## Related Issues

- Universal runtime log buffer/export: https://github.com/d954mas/game-67-idle/issues/1
