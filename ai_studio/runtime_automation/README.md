# Runtime Automation

AI Studio module for local runtime proof automation.

## Role

Runtime Automation owns game/runtime interaction helpers used by agents during
native playable work:

- DevAPI TCP client and minimal CLI.
- Native debug launch/reuse helpers.
- Framebuffer, window screenshot, and screen recording helpers.
- PNG encode/decode utilities used by capture and health checks.
- Pixel-health checks for blank or flat screenshots.
- UI readability zoom audit for screenshot evidence.
- Live-state screenshot coverage and acceptance-matrix helper.

This module does not own engine DevAPI implementation, game-specific commands,
game state schemas, quality rules, or task state. The engine owns DevAPI
runtime behavior; games own the commands they expose; `ai_studio/quality/`
owns acceptance rules; `ai_studio/taskboard/` owns durable work state.

## Commands

```powershell
node ai_studio/dev_environment/python_run.mjs -m unittest discover -s ai_studio/runtime_automation -p "*_test.py"
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/devapi_cli.py 17890 endpoints
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/iterate.py --json
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/iterate.py 17890 --reuse --json
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/ui_readability.py tmp/captures/screenshot.png
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/pixel_health.py tmp/captures/screenshot.png
node ai_studio/runtime_automation/web_local_mock_probe.mjs --url http://127.0.0.1:8092/ --cdp http://127.0.0.1:9222 --out games/<game-id>/.ai_studio/evidence/local-mock/<observation>.json
```

## Local mock web proof

`web_local_mock_probe.mjs` connects to an isolated Chromium/Chrome page through
raw CDP using Node's built-in `WebSocket`; it has no Puppeteer dependency and
never navigates outside localhost. Start the exact local mock build's static
server, then start a fresh headless browser with a local debugging endpoint,
for example `--headless=new --remote-debugging-port=9222
--user-data-dir=<fresh-temp-dir> --use-angle=swiftshader
--enable-unsafe-swiftshader about:blank`.

The probe creates its own page target, instruments the loading hooks before any
page script runs, and enables console, exception, and network capture before
navigation. It passes only when a `release:true` local mock page receives final
progress and loading-finished through the named C-to-JS bridge functions, hides
the overlay, exposes a non-zero render canvas, stays entirely on loopback, and
reports no errors. One absolute timeout covers target creation, WebSocket/CDP
calls, navigation, polling, and cleanup.

The probe fetches `runtime-build.json` before and after the CDP session and
requires it to match both the page bootstrap and a fingerprint marker published
by the executing C/WASM module. The deterministic output remains a runtime
observation by itself; it becomes release evidence only when the game-owned
portal reporter finds the identical mechanically generated source record and a
compiled witness inside a verified release ZIP. Local and portal targets remain
different payloads: this proves a common source snapshot and real execution,
not byte equality between their target-specific WASM files. A separately
supplied ZIP hash is never treated as that binding.

## Native source-to-runtime proof

Normal `iterate.py` mode owns the reference-template proof loop. It prepares
`templates/template/build/devapi-debug` with `GAME_DEVAPI_ENABLED=ON` when the
cached Ninja configuration is absent or incompatible, then invokes the
canonical `cmake --build ... --target game` command on every run. An existing
executable is never accepted as freshness evidence; CMake/Ninja decides whether
the requested build is a true no-op.

The helper launches a fresh process on an ephemeral port by default. A socket
connection is not readiness: the launch phase ends only after `endpoints`
succeeds and advertises the template-owned `game.iteration.proof` command. That
DevAPI-only command returns a dedicated leaf-C token plus the generated
`test_label_text` schema default. The helper reads both source fixtures before
building and exact-compares both live values, proving that the launched binary
contains current C and codegen/content inputs. The stable JSON result keeps
validation, configure, metadata, codegen, compile, link, launch-to-ready,
semantic-proof, capture-consistency, and total phase records plus process/DevAPI
call and output-byte counts. Each phase carries an explicit status and nullable
wall time. Tool-version discovery is the metadata phase. Codegen and link
performed inside the one canonical build are honestly marked
`included-in-build` with no invented standalone timing; the whole build wall
time is reported under `compile`. The result also records the exact resolved
port and positive PID, command paths actually executed (configure remains null
when skipped), actual tool versions, start/end commit and scoped input identity,
engine gitlink/cleanliness, executable/pack hashes, and rebuilt Ninja outputs
when the log exposes them. The scoped identity comes from Git's binary diff plus
sorted nonignored untracked paths and bytes under the template, features, and
iteration-orchestrator surfaces. It excludes ignored build/tmp products. This
identity is only a race guard; it never decides whether to build. Foreign or
incompatible cache roots use CMake's supported `--fresh` configure instead of
deleting build directories; a missing cache uses a normal configure.

Before and after capture the helper rereads the two source fixtures and the
run-consistency guard. A race blocks capture or marks an already-written capture
`invalidated-race`. Every CLI failure is emitted as bounded structured JSON
with partial phase state and `freshnessClaim:false`. The claim changes to true
only after semantic proof and final capture-consistency checks pass.

The Studio Runtime Automation suite runs the unit contract on Windows and Linux.
Linux CI additionally runs the real functional `iterate.py --no-capture --json`
command; Windows functional no-op/leaf-C/schema smoke evidence is recorded by
T0395 without creating a second performance benchmark.

`--reuse` is strict attach-only interaction with an already-running unchanged
game. It never reads fingerprints or fixtures, configures, builds, launches,
proves, or captures; only `endpoints` readiness is checked. It reports
`freshnessClaim:false`. Use it for repeated interaction, not for a source-edit
iteration claim.

Set `AI_STUDIO_GAME_EXE` when driving a specific game through the low-level
client helpers. `iterate.py` intentionally drives the canonical reference
template executable under `templates/template/build/devapi-debug/bin/`.
`AI_STUDIO_DEVAPI_PORT` or
`NT_DEVAPI_PORT` still pin an explicit port when set.

When `running_game()` launches a new game with no explicit port and no env
override, it now picks a free ephemeral port automatically for that one
launch instead of the fixed `17890`, so concurrent sessions never collide on
the same port (VibeJam had 8-9 concurrent sessions bind-fail and exit
instantly on a shared fixed port, which looked like a hung 5s connect
timeout). CLI tools that attach to an already-running game (`devapi_cli.py`,
`iterate.py`) still default their port argument to `17890`, the conventional
single-session port, when you don't pass one explicitly.

## Player gate for injected input

Wrap any `ui.click`/`input.*` interaction sequence in `DevApiClient.player_gated()`.
The player-input gate's ON->OFF edge clears all real pointer slots; without it, an
injected click can land in a non-primary slot beside a live mouse instead of the
always-free slot 0. The context manager disables the gate on enter and always
re-enables it on exit, even if the body raises:

```python
with game.player_gated():
    game.click_ui("settings/gear")
    game.wait_frames(2)
```

## Skill

Use `nt-runtime-automation` when an agent needs to collect runtime evidence,
drive DevAPI, capture screenshots/recordings, or change this module's helpers.

## Boundaries

Use this module when the agent needs local runtime evidence: state snapshots,
screenshots, visual health checks, readability zooms, or repeatable native
iteration probes.

Game-specific context and startup gates belong inside `games/<game-id>/`.
State-code generation lives inside `features/game-state/`.
