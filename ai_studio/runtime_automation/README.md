# Runtime Automation

AI Studio module for local runtime preview and proof automation.

## Role

Runtime Automation owns game/runtime interaction helpers used by agents during
native playable work:

- DevAPI TCP client and minimal CLI.
- Native debug launch/reuse helpers.
- Catalog-selected non-proof web preview through game-owned build plans.
- Engine framebuffer screenshots, explicit window screenshots, and OBS game recording.
- PNG encode/decode utilities used by capture and health checks.
- Pixel-health checks for blank or flat screenshots.
- UI readability zoom audit for screenshot evidence.
- Live-state screenshot coverage and acceptance-matrix helper.
- QCLR_002 responsive viewport capture and portal reserved-zone check for any game.
- Game-owned window contract capture: all declared windows in portrait and
  landscape, with scene coverage and scroll-policy checks.

This module does not own engine DevAPI implementation, game-specific commands,
game state schemas, quality rules, or task state. The engine owns DevAPI
runtime behavior; games own the commands they expose; `ai_studio/quality/`
owns acceptance rules; `ai_studio/taskboard/` owns durable work state.

Native launch helpers wait eight seconds for DevAPI by default. A scenario may
pass `running_game(startup_timeout=30.0)` for slower shader or asset startup;
the responsive viewport CLI exposes the same setting as `--startup-timeout`.
This does not change the per-request timeout after connection.

## Commands

```powershell
node ai_studio/dev_environment/python_run.mjs -m unittest discover -s ai_studio/runtime_automation -p "*_test.py"
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/devapi_cli.py 17890 endpoints
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/iterate.py --json
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/iterate.py 17890 --reuse --json
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/ui_readability.py tmp/captures/screenshot.png
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/pixel_health.py tmp/captures/screenshot.png
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/capture_window.py --process-id <pid> --output tmp/captures/window.png
node ai_studio/runtime_automation/web_local_mock_probe.mjs --url http://127.0.0.1:8092/ --cdp http://127.0.0.1:9222 --out games/<game-id>/.ai_studio/evidence/local-mock/<observation>.json
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/window_layout_capture.py --game <game-id>
```

The game-owned contract format and proof limits are in
[window_layout_contract.md](window_layout_contract.md).

DevAPI screenshots use the engine's `capture.frame` and `capture.region`
commands. Capture errors propagate to the caller. Use `capture_window.py`
explicitly when the required evidence includes the operating-system window.

## Fast web preview

Use web preview for browser-specific work; ordinary gameplay iteration should
use the game's documented native path. Run web preview from the Studio root
with a canonical catalog game ID:

```powershell
node ai_studio/runtime_automation/preview_game.mjs --game <game-id> --preset wasm-devapi-debug
```

Use `wasm-debug` when DevAPI is unnecessary. The adapter uses the game's exported
build-plan API, preserving its own build steps and artifact layout. Catalog and
physical-path checks run before importing the game builder. Unsupported builder
APIs fail explicitly. Plans were checked across eleven catalog games and three
local derivative checkouts; the CLI selects canonical catalog mounts.

Preview skips the full source-tree fingerprints before and after compilation.
It clears the compiled fingerprint, removes an older proof record before the
build and writes `ai_studio.runtime_build.preview.v1` with `proof: "none"`.
Only debug presets accept this mode. Release packaging rejects this record;
native capture and formal release checks retain their source freshness checks.
The next ordinary build restores proof generation in the same build directory.

Keep the game's local server running and refresh after incremental builds.
Run only tests for the changed contract while editing. Use ordinary builds and
the owning module's checks at integration, capture or release boundaries; the
complete Studio gate belongs at CI/acceptance boundaries. Preview build time
still includes actual CMake/compiler work and varies with changed inputs.

The current starter and shared feature consumer also accept
`node tools/build_web.mjs --preset wasm-debug --preview` from their own roots.

## Gameplay recording

Run one command from a game root:

```powershell
.\capture.cmd live
.\capture.cmd shot intro
```

`live` opens a normal controllable game and writes one MP4. `shot` launches a
fresh unsaved game, then applies the catalog setup and event schedule. REC
playback is realtime at 1×. Both print the final `tmp/captures/<take-id>/edit.mp4`
path. The fresh state, setup, and event schedule are deterministic; rendering,
OBS encoding, and the MP4 are not byte- or frame-identical between takes. There
is no montage, retiming, x4 mode, or preroll in the result.

### Native capture provenance

Before either command opens the game window, capture rebuilds the current game's
canonical native CMake target: `build/devapi-debug`, target `game`, and its
platform artifact `bin/game.exe` on Windows or `bin/game` on POSIX. The cache
must declare its exact `CMAKE_CACHEFILE_DIR`, the selected game as
`CMAKE_HOME_DIRECTORY`, `GAME_DEVAPI_ENABLED=ON`, `CMAKE_GENERATOR=Ninja`, and
`CMAKE_BUILD_TYPE=Debug`. When `GAME_REPO_ROOT` is present, it must resolve to
the Studio root. A missing, foreign, or incompatible cache fails with the
configure command instead of accepting a previously built executable.

A game whose client is a different target may declare it in the catalog's
`executable` as `build/<dir>/bin/<target>` (`.exe` on Windows); capture then
rebuilds `build/<dir>`, target `<target>` (the file stem), and launches that
artifact. The path must stay inside the game's `build/` tree at exactly that
depth, resolve through no link or junction, and be a regular file after the
rebuild. Its cache keeps the same contract except the build type, which may be
any explicit `CMAKE_BUILD_TYPE` and is recorded in `nativeBuild.cacheContract`.
`--executable` follows the same rules. `launch` (`fresh_state`,
`autosave_enabled`, `args`) applies to either artifact.

The preflight calls the selected game's `tools/lib/runtime_build.mjs` when the
game still carries copied tooling, and `ai_studio/game_tools/lib/runtime_build.mjs`
otherwise, as `createRuntimeSourceRecord({gameDir, studioRoot})` through a fixed Node script.
Legacy modules retain their `createRuntimeBuildRecord` entrypoint. Source-only
records do not invent a web compile profile for a native build. Physical game
and module paths must stay inside their owning roots before the script imports
game tooling.
It records the resulting `sourceFingerprint`, executable SHA-256 and byte size
as `nativeBuild` in the capture manifest. It checks the source record before and
after the rebuild, then checks the source record, executable, and critical cache
contract again after REC. The critical cache values must remain stable across
those checkpoints. Any observed change invalidates the capture proof; rerun
capture after the edit settles. Full Node and CMake output is stored in the
take's `.preflight/` directory while failure messages contain only a bounded
tail. These checkpoints detect changes that persist to an observation point;
they cannot attest an external writer that swaps and restores a cache entirely
between observations.

`nativeBuild.proofKind` is `native-rebuilt`. It proves that this local native
capture came from a rebuilt configured target. It does not witness a web build,
portal package, or release artifact.

Each game owns `capture/catalog.json`:

```json
{
  "version": 1,
  "executable": "build/devapi-debug/bin/game.exe",
  "defaults": {"size": "1920x1080", "fps": 30, "ticks_per_frame": 2},
  "live": {"seconds": 15},
  "shots": {
    "intro": {
      "seconds": 3,
      "warmup_ticks": 2,
      "setup": [{"method": "game.scene.stage_intro"}],
      "events": [{"frame": 30, "method": "input.key", "params": {"key": "W"}}]
    }
  }
}
```

`fps` is the MP4 frame rate; `ticks_per_frame` selects the simulation rate
in Hz: `fps * ticks_per_frame` before REC. It is not an exact tick count for
each encoded frame. During REC the game runs normally;
`events` are scheduled at their output-frame wall times and `frame` is an
output-frame index. `setup` and `warmup_ticks` run before REC. A shot can
override `preset`, `size`, `fps`, `ticks_per_frame`, or `max_freeze_seconds`
(default `2.0`) from defaults. Its `seconds * fps` must be a whole number.
### Windows host-execution boundary

OBS-backed capture depends on the active Windows console user's per-user
`CaptureService`. On a managed host, launch `capture live` and `capture shot`
through the approved interactive host execution broker on the first attempt. In
Codex this means running the game-owned capture command with
`sandbox_permissions: "require_escalated"`. A process started as an isolated
sandbox account can be in the same console session yet still fail WGC with
`CreateForWindow (0x80070424)` and encode uniform black frames. That error is a
Windows capture-service failure; on a managed host, first keep the canonical WGC
backend and rerun the unchanged command as the active interactive user instead
of cycling OBS or adding an alternate backend. If the command already has that
identity, inspect the user's `CaptureService` availability.

## Recorder maintenance

`record_game.py` is the shared OBS backend used by `capture.cmd`. It validates
frame count, dimensions, FPS, video health, audio format, and writes technical
metadata beside the final MP4. Normal game work uses only `capture.cmd`; do not
build per-video record or montage scripts.

Shot capture uses an isolated, authenticated OBS WebSocket and a disabled-by-
default full-frame magenta marker source. The marker covers setup, then OBS
acknowledges its removal and renders the configured post-ack frame barrier
before the audio gate and timeline begin. After OBS finalizes the source,
`obs_marker.py` decodes that marker run and trims at the first decoded game-frame
PTS after it. Missing, incomplete, or ambiguous marker evidence fails the take;
the recorder never falls back to packet timestamps or a fixed encoder delay.
The manifest stores the marker PTS/frame receipt, control barrier receipts, and
marker-disable-to-audio-gate delay so A/V timing remains inspectable.

## Responsive viewport evidence

`responsive_viewports.py` captures QCLR_002 evidence for any live game: one
launch per viewport (`--window-size`), an engine screenshot, `ui.tree` bounds,
a contact sheet and `summary.json`.

```powershell
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/responsive_viewports.py --game <game-id> --portal poki
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/responsive_viewports.py --game <game-id> --viewport 640x360 --viewport phone=390x844
node ai_studio/dev_environment/python_run.mjs ai_studio/runtime_automation/responsive_viewports.py --exe <path/to/game.exe> --cwd <launch dir>
```

- `--game <id>` resolves the game through the workspace catalog
  (`ai_studio/workspace/games.mjs list --include-private`) and launches what
  its `capture/catalog.json` names: `executable` (default
  `build/devapi-debug/bin/game.exe`) and `launch.fresh_state`,
  `launch.autosave_enabled`, `launch.args` -- the same block
  `capture_game.py` reads. A game whose CLI rejects `--fresh-state` or
  `--disable-autosave` (the PLW clients, which also need `--solo`) declares
  that in its catalog; there is no per-game code here. It launches in the game
  root and writes to `<game root>/build/quality/qclr_002_responsive/`.
- `--exe` overrides the executable; `--cwd` the launch directory (default: the
  game root, else the current directory). `--fresh-state/--no-fresh-state`,
  `--autosave/--no-autosave` and repeatable `--launch-arg=--no-vsync` override
  or extend the catalog launch block. `--port` pins the DevAPI port (default: a
  free port per launch).
- `--viewport NAME=WxH` (repeatable) replaces the default 4:3, 16:9 and
  19.5:9 landscape/portrait matrix; `--landscape-only` filters it.
- `--portal <target>` checks visible `ui.tree` leaves against the target's
  `metadata.reserved_zones` (`portal_zones.py`; the Poki Pill) and exits 1 on
  overlap. Leaves at least 90% of the window wide are counted as backdrops
  (root shell, scrims) and skipped; the text and controls over them are leaves
  of their own and still checked. `--zone-ignore <id-glob>` (repeatable)
  exempts any other decorative leaf.
- `--scenario MODULE:FUNCTION` or `PATH.py:FUNCTION` runs
  `prepare(game, viewport)` after warmup and before capture, to reach a menu or
  game moment. Relative paths and modules also resolve against the game root,
  so hooks stay in the game (`--scenario devapi/scenarios.py:open_shop`).
- `--audit` adds the pixel-health audit (off by default: quiet valid UI can fail
  its generic thresholds).

A bot can drive the same matrix in-process: `run_matrix(factory, viewports,
out_dir, prepare=..., portal=...)` with `devapi_client.running_game` as the
factory.

## Headless Chrome profiles

Every headless Chrome run gets its `--user-data-dir` from `chrome_profile.mjs`
or `chrome_profile.py`. Profiles live under `<temp>/ai-studio-chrome/`; creating
one also sweeps profiles older than a day, so a run that crashed before cleanup
does not leak for good.

```js
import { createChromeProfile } from "<studio>/ai_studio/runtime_automation/chrome_profile.mjs";
const profile = createChromeProfile("shots");
const chrome = spawn(chromePath, ["--headless=new", `--user-data-dir=${profile.dir}`, ...]);
try { /* drive the page */ } finally { await profile.dispose(chrome); }
```

```python
from chrome_profile import chrome_profile, remove_profile
with chrome_profile("shots") as profile:
    proc = subprocess.Popen([chrome, "--headless=new", f"--user-data-dir={profile}", ...])
    try: ...  # drive the page
    finally: remove_profile(profile, proc)
```

`dispose` / `remove_profile` stop Chrome and wait for the exit before deleting:
Windows refuses to delete files a live process holds, and `kill()` does not wait.

Browser GPU-process profiling (trace, sampler, stack folding):
[`browser_gpu_profiling.md`](browser_gpu_profiling.md).

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

Normal `iterate.py` mode owns the independent feature-consumer proof loop. It prepares
`ai_studio/feature_testing/consumer/build/devapi-debug` with `GAME_DEVAPI_ENABLED=ON` when the
cached Ninja configuration is absent or incompatible, then invokes the
canonical `cmake --build ... --target game` command on every run. An existing
executable is never accepted as freshness evidence; CMake/Ninja decides whether
the requested build is a true no-op.

The shared client's short loopback probes and retry waits share one monotonic
connection deadline, including the prelaunch occupied-port check. Successful
connections receive their separate request timeout; a quick connection budget
does not shorten a later capture or command response deadline.

The helper launches a fresh process on an ephemeral port by default. A socket
connection is not readiness: the launch phase ends only after `endpoints`
succeeds and advertises the consumer-owned `game.iteration.proof` command. That
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
sorted nonignored untracked paths and bytes under the consumer, features, and
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
client helpers. `iterate.py` intentionally drives the independent feature
consumer executable under `ai_studio/feature_testing/consumer/build/devapi-debug/bin/`;
it does not select an arbitrary game's executable.
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

## Build input hashing

`ai_studio/game_tools/lib/runtime_build.mjs` (or an older game's copied
`tools/lib/runtime_build.mjs`) selects inputs and creates build records. Integrated games use `runtime_hash_tree.cjs` for synchronous streaming
hashing; standalone copies keep their game-owned implementation. Native capture
validates source proof through `native_capture_fingerprint.mjs`. Fingerprinting
uses the installed Node.js runtime and must not launch an ad-hoc fingerprint
executable.

```powershell
node --test ai_studio/game_tools/tests/runtime_hash_tree.test.mjs ai_studio/runtime_automation/native_capture_fingerprint.test.mjs
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
