# Publish Target Manifest Contract

T0339 defines publish targets separately from SDK adapters:

| Target | SDK adapter | Build dir |
| --- | --- | --- |
| `local` | `mock` | `build/wasm-release/bin` |
| `itch` | `mock` | `build/wasm-release-itch/bin` |
| `poki` | `poki` | `build/wasm-release-poki/bin` |
| `yandex` | `yandex` | `build/wasm-release-yandex/bin` |
| `playgama` | `playgama` | `build/wasm-release-playgama/bin` |

`local` is a development target, not a portal manifest. The portal manifest
files live in `features/platform-sdk/publish-targets/`:

- `itch.json`
- `poki.json`
- `yandex.json`
- `playgama.json`

Each manifest declares:

- `required_files`: files expected in the upload root;
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
```

The script passes `-DGAME_PUBLISH_TARGET=<target>` to CMake. CMake computes the
SDK adapter and copies only that adapter to `platform-sdk-adapter.js`.

## Validate

Run the manifest's validation command against the built `bin` directory:

```powershell
node features/platform-sdk/scripts/artifact_tools.mjs inspect --target itch --artifact templates/template/build/wasm-release-itch/bin
node features/platform-sdk/scripts/artifact_tools.mjs inspect --target poki --artifact templates/template/build/wasm-release-poki/bin
```

The inspection fails if production artifacts contain debug button labels,
`debug_test`, or SDK markers from adapters not selected by the target.
