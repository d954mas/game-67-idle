# Publish Target Manifest Contract

Publish targets are defined separately from SDK adapters:

| Target | SDK adapter | Build dir |
| --- | --- | --- |
| `local` | `mock` | `build/wasm-release/bin` |
| `itch` | `mock` | `build/wasm-release-itch/bin` |
| `poki` | `poki` | `build/wasm-release-poki/bin` |
| `yandex` | `yandex` | `build/wasm-release-yandex/bin` |
| `playgama` | `playgama` | `build/wasm-release-playgama/bin` |
| `crazygames` | `crazygames` | `build/wasm-release-crazygames/bin` |

`local` is a development target, not a portal manifest. The portal manifest
files live in `features/platform-sdk/publish-targets/`:

- `itch.json`
- `poki.json`
- `yandex.json`
- `playgama.json`
- `crazygames.json`

Each manifest declares:

- `required_files`: staged build files before release composition;
- `packaged_required_files`: files expected in the final upload root;
- `zip_layout`: archive shape;
- `metadata`: portal/account/config metadata that cannot be inferred from the
  build artifact;
- `sdk_policy`: selected adapter, SDK/placeholder policy, forbidden markers,
  and unsupported/no-op behavior;
- `validation_command`: the local artifact inspection command.

## Build

Use the template web builder and target-specific output directory:

```powershell
node tools/build_web.mjs --preset wasm-release --target itch
node tools/build_web.mjs --preset wasm-release --target poki
node tools/build_web.mjs --preset wasm-release --target yandex
node tools/build_web.mjs --preset wasm-release --target playgama
node tools/build_web.mjs --preset wasm-release --target crazygames
```

The script passes `-DGAME_PUBLISH_TARGET=<target>` plus a mechanically generated
runtime source fingerprint to CMake. CMake computes the SDK adapter and stages
only that adapter as `platform-sdk-adapter.js`; release packaging embeds the
source-hash-bound minified SDK bundle into `game.js`, removes the standalone
modules, and minifies generated HTML with its inline CSS/JavaScript. The
builder writes the shared record to `runtime-build.json` for local and portal
targets.
The game also publishes that fingerprint from compiled WASM; packaging rejects
a release whose executable lacks the witness. Because target policy changes the
WASM bytes, local-mock attachment proves the same mechanically hashed source
snapshot was compiled and executed, not that local and portal payloads are
byte-identical.

## Validate

Run the manifest's validation command against the built `bin` directory:

```powershell
node features/platform-sdk/scripts/artifact_tools.mjs inspect --target itch --artifact templates/template/build/wasm-release-itch/bin
node features/platform-sdk/scripts/artifact_tools.mjs inspect --target poki --artifact templates/template/build/wasm-release-poki/bin
```

Inspection accepts either the staged build layout or the manifest-declared
single-JS package layout. It fails if production artifacts contain debug button
labels, `debug_test`, or SDK markers from adapters not selected by the target.

## One command for everything mechanical

A game checks a portal submission with one command rather than three:

```powershell
node tools/game.mjs portal-check --target <portal>
```

It runs the artifact against this manifest, the store folder against the
portal's spec in `ai_studio/store_kit/`, and the portal's own SDK proof where
one exists. A portal with no local proof has no such row, rather than a row that
silently passes.

## Prove the SDK before a draft

Yandex ships a dev server, so "it cannot be tested locally" is never true. The
probe starts that server against a built artifact, drives it in headless
Chrome, and reports what the SDK itself saw: the loading-ready call, the
gameplay start/stop pair, the portal pause and resume events, and whether the
portal language was read while the game was still loading.

```powershell
node features/platform-sdk/scripts/yandex_sdk_probe.mjs --artifact games/<id>/build/wasm-release-yandex/bin
```

It exits non-zero on any failed row and takes `--json <file>` for the full
transcript of calls, listeners and their originating stack frames.

## Release evidence levels

Portal evidence is recorded per game release and never inferred from a lower
level:

| Level | What it proves |
| --- | --- |
| `local-mock` | A game-owned local mock browser run whose runtime build record matches the exact release ZIP. |
| `local-sdk-contract` | The exact final ZIP and sidecar pass the offline package and selected-adapter contract. |
| `public-inspector` | A public portal inspector accepted that exact ZIP SHA-256. |
| `credentialed-portal-smoke` | An authenticated portal smoke run exercised that exact ZIP SHA-256. |
| `production-certification` | The portal issued its production certification for that exact ZIP SHA-256. |

The offline game-owned reporter always records `local-sdk-contract` as `pass`
and can additionally attach a matching local mock observation. It does not read
credentials or make network requests; missing portal access is therefore an
explicit `unverified` result with the next required evidence, not a fabricated
success.
