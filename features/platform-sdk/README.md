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
| `poki` | `poki` | Poki SDK owns lifecycle, ads, and platform analytics. |
| `yandex` | `yandex` | Yandex Games SDK owns lifecycle, ads, pause/resume, and player data. |
| `playgama` | `playgama` | Playgama Bridge is its own SDK adapter, not the universal wrapper. |

## Contents

```text
features/platform-sdk/
  README.md
  INSTALL.md
  feature.json
  references/
    contract.md
```

This is currently a contract-first feature pack. Runtime code, tests, and
template wiring should be added in the next implementation slice.

## What It Owns

- The game-facing platform SDK wrapper contract.
- The target-to-SDK mapping.
- Build-time selection rules so release builds include only the selected SDK
  adapter.
- Runtime target/SDK identity and capabilities for policy-sensitive UI, such as
  whether external creator/community links may be shown.
- Adapter semantics for lifecycle, pause/resume, interstitials, rewarded ads,
  banners, analytics, save/load, locale, and unsupported operations.
- Local scorecard event names that later feed commercial analytics.

## What It Does Not Own

- Publish packaging scripts, zip layout, portal metadata, or account setup.
- Per-game monetization balance and reward design.
- The old JS template under `C:\projects\ai_games`; that path is only a
  reference example.

## References

- `references/contract.md`: current platform SDK wrapper contract.

## Backdoor

A game with fundamentally different platform semantics may copy the feature
implementation into its own tree and own that fork. Do not add speculative
switches to the shared feature for one-off portal behavior.
