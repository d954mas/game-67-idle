# platform-sdk

Reusable L1 feature pack for browser platform SDK integration.

This feature separates two concepts that must not be conflated:

- **target platform**: where the build runs or is published;
- **platform SDK**: which SDK adapter is used to talk to that target.

`mock` is a platform SDK adapter, not a target. `local` and `itch` are targets
that use the `mock` SDK adapter because they do not have a required runtime
portal SDK.

## Layer

L1 foundation. It exposes a stable API to game code and does not depend on
game-specific screens, economy, items, progression, or content.

## Target To SDK Map

| Target | Platform SDK | Reason |
| --- | --- | --- |
| `local` | `mock` | Local development and smoke tests need deterministic SDK behavior. |
| `itch` | `mock` | itch.io is a publish target with no mandatory game SDK. |
| `poki` | `poki` | Selected backend calls Poki SDK methods; the C facade owns game policy. |
| `yandex` | `yandex` | Selected backend calls Yandex Games SDK methods; the C facade owns game policy. |
| `playgama` | `playgama` | Playgama Bridge is its own SDK adapter, not the universal wrapper. |

## Contents

```text
features/platform-sdk/
  README.md
  INSTALL.md
  feature.json
  include/features/platform_sdk/platform_sdk.h
  src/platform_sdk.c
  web/
    platform-sdk.js
    platform-sdk-core.js
    adapters/
      mock.js
      poki.js
      yandex.js
      playgama.js
  scripts/
    artifact_tools.mjs
    scorecard.mjs
  publish-targets/
    itch.json
    poki.json
    yandex.json
    playgama.json
  references/
    contract.md
    publish-targets.md
```

The template consumes this as an in-place L1 module: C code owns target/SDK
identity, policy capabilities, lifecycle state, gameplay/input guards, ad-flow
callbacks, and pause/resume listener dispatch. Web builds copy only the selected
JavaScript backend adapter into the artifact as `platform-sdk-adapter.js`.
The template's debug/test controls are C/Clay UI in
`templates/template/src/ui/platform_sdk_debug.c`; they are not staged as a web
JavaScript artifact.

## What It Owns

- The game-facing platform SDK wrapper contract.
- The target-to-SDK mapping.
- Build-time selection rules so release builds include only the selected SDK
  adapter.
- Runtime target/SDK identity and capabilities for policy-sensitive UI, such as
  whether external creator/community links may be shown.
- Runtime input/gameplay flags: `platform_sdk_gameplay_start()` is blocked until
  first player input, and the C facade exposes `platform_sdk_has_input()`,
  `platform_sdk_has_gameplay_started()`, and `platform_sdk_gameplay_active()`.
  First input is only a precondition: active gameplay is still owned by the game
  state, so menus, settings, dialogs, and ad flows must call or remain in
  `platform_sdk_gameplay_stop()`.
- Adapter semantics for lifecycle, pause/resume, interstitials, rewarded ads,
  banners, portal analytics forwarding, save/load, locale, and unsupported
  operations. The C facade owns ad-flow policy and listener dispatch; selected
  JavaScript backends only call portal SDK methods and report outcomes.
- The compact SDK lifecycle/ad-flow event contract, owned by the C facade and
  bridged into `features/game-events` once that L0 pack exists, plus a
  fixture-driven scorecard CLI.
- Publish target manifest contracts for itch, Poki, Yandex Games, and Playgama.

## What It Does Not Own

- The zip builder itself. Use the template's `tools/build_web.sh`; target
  manifests define acceptance above that output.
- Portal account setup.
- Per-game monetization balance and reward design.
- The old JS template under `C:\projects\ai_games`; that path is only a
  reference example.
- Gameplay guards, event emission, debug UI, or reward policy in JavaScript.
  JavaScript is only the selected web SDK backend/portal call layer.

## Commands

```powershell
node --test features/platform-sdk/tests/platform_sdk.test.mjs
node features/platform-sdk/scripts/scorecard.mjs --input features/platform-sdk/tests/fixtures/scorecard-sample.ndjson --pretty
node features/platform-sdk/scripts/artifact_tools.mjs inspect --target itch --artifact templates/template/build/wasm-release-itch/bin
```

## References

- `references/contract.md`: current platform SDK wrapper contract.
- `references/publish-targets.md`: target manifest and validation contract.

## Backdoor

A game with fundamentally different platform semantics may copy the feature
implementation into its own tree and own that fork. Do not add speculative
switches to the shared feature for one-off portal behavior.
