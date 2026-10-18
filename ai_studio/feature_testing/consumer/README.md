# Feature integration consumer

This fixture belongs to [Feature Testing](../README.md). Run shared-feature
checks through `node ai_studio/feature_testing/run.mjs` from the Studio root.

The consumer supplies schemas, generated content, UI/settings references,
platform adapters and registered sample assets used by integration tests.
Its sample executable is excluded from the default build.

The folder is outside the game-template catalog. Its internal `template.json`
is fixture input for toolchain tests and does not make it a game starter.
Games copy selected reference code only when needed, following its INSTALL
contract. They do not inherit this consumer through `games/new_game.mjs`.

Native tests live in `tests/` and are registered by `cmake/GameTests.cmake`.
Reusable implementations compile from their `features/` owners. Source assets
retain their existing license, provenance, origin and hashes.
