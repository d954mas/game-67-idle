# Game Tools

The build, test, package and release CLI every game runs. It used to be copied
into each game's `tools/`, and every copy drifted; it now lives here once and a
game keeps two thin entrypoints:

- `tools/game.mjs` passes the game directory, and the game's own extra commands,
  to `game_cli.mjs`.
- `tools/build_web.mjs` passes the game directory to `build_web.mjs`, so the
  documented `node tools/build_web.mjs --preset ...` keeps working.

Both locate the Studio with `tools/lib/studio_root.mjs`, which stays in the game
because it is what finds this module. The CLI then builds and proves against the Studio it
was loaded from only: a `GAME_REPO_ROOT` that names another Studio is an error,
and both CMake trees get `-DGAME_REPO_ROOT` set to it.

Games already compile `features/` and the engine from the Studio checkout they
sit in, so loading the CLI from the same checkout adds no new coupling.

## Contents

- `game_cli.mjs`: `doctor`, `build`, `run`, `test`, `playable`, `package`,
  `portal-check`, `verify`; test tiers come from CTest labels. `test` and
  `verify` run the Node suite beside the native build and CTest, keep test output in `build/logs/game-test.log` and print one summary
  line, or the failing step's tail and the log path; `--verbose` streams it all.
  `clean` removes build variants and keeps `build/native-debug`; `clean --all`
  removes the whole `build/`.
- `build_web.mjs`: the web build plan and runtime build fingerprint.
- `package_web.mjs`, `package_web_smoke.mjs`, `minify_web_release.mjs`,
  `inject_asset_bytes.mjs`: the deterministic portal package and its proof.
- `lib/release_package.mjs` publishes the ZIP, manifest and exact WASM symbol
  map for both standard games and games with custom site validation. A game may
  declare client filenames, pack manifest location and symbol paths in
  `release/web-package.json`; omitted configuration uses `game.*` and
  `assets/game.ntpack`. A custom runtime still supplies its own site and
  loader validator to the CLI; ZIP construction, manifest and publication stay
  in Studio.
- `portal_evidence.mjs`: release evidence for a packaged artifact. Run it from
  the game root.
- `serve_web.mjs`: a loopback server for a built artifact. Run it from the game
  root, or through `node tools/game.mjs run`.
- `lib/runtime_build.mjs`: runtime source and build records. For a game without
  copied tooling the record includes a `tooling` input: the files here that
  change artifact bytes (`build_web`, `inject_asset_bytes`, `minify_web_release`).
- `lib/game_asset_packs.mjs`: validates game-owned deferred pack declarations,
  stages required packs and extends the package allowlist. Its source is also
  part of the runtime tooling fingerprint.
- `lib/copied_tooling.mjs`: the one test for a game that still carries copied
  tooling (`tools/package_web.mjs` present).

ZIP writing comes from `ai_studio/core_harness/tool_lib/zip_store.mjs`.

`live_games.mjs` runs every live (non-frozen) game's `game.mjs test` against
this checkout after checking its dependency record (`version_drift`), and fails
if any game drifts or fails. Run it before merging an engine move or a shared
feature change.

## Game-specific commands

A game adds commands in its shim; they are tried before the shared ones:

```js
process.exitCode = await main(process.argv.slice(2), {
  gameDir: GAME_DIR,
  commands: { dev: async (argv, { gameDir }) => "dev loop stopped" },
});
```

## Game hooks

The shim passes, beside `gameDir` and `commands`, only what the game owns:

- `doctor`: wraps `doctorGame`; `doctorGame({ ..., ciWorkflow: false })` for a
  game that keeps verification local and ships no CI workflow.
- `testTiers`: CTest tiers beyond `core|slow|taste` (PLW's `net`).
- `nativeTest`: usually `runNativeTests(gameDir, options, { settle })`, where
  `settle(runStep, env)` configures `build/native-debug` the game's way, and
  `gameTarget` names a playable CMake target other than `game`.
- A game with its own web layout: `build`, `artifactDir`, `validateArtifact`,
  `package`, `smoke` and `portalArtifact: { args, replacesFeatureInspect? }`.
  portal-check runs the feature's `artifact_tools inspect` and then the game's
  check; the inspect is dropped only with a stated reason, printed on every run.
- `build_web.mjs` takes `configureArgs: (args) => ({ host, web })`, extra `-D`
  options for the asset host tree and the web tree of one build.

A build may record `remote_endpoints.json` (`{ "allowedRemoteUrls": [...] }`)
beside its `bin/`: the packaged smoke then admits only those endpoints, failed
as if the player were offline. A `game.js.symbols` map the link leaves in `bin/`
moves one level up with a digest of its linked WASM. The shared release build
asks Emscripten to emit the map; packaging requires it, checks the digest and
keeps the map beside the ZIP, including with `--no-build`. A
`version` in `game.json` names the artifact `<id>-<target>-v<version>-<hash>`.
`THIRD_PARTY_NOTICES.txt` may ride in any web root. `wasm-devapi-perf` is the
optimised DevAPI build for performance measurements.

## Portal builds with and without assert text

Use `node tools/game.mjs package --target <portal> --assert-text` for a portal playtest, then `node tools/game.mjs package --target <portal>` for the final release. The assert-text release uses `wasm-release-asserts`: Release optimization and minification, `NT_ASSERT_MODE=2`, and a shared platform-sdk assert reporter that writes the failed expression, file, and line into the engine crash log before trapping. The final release uses `wasm-release` with `NT_ASSERT_MODE=1` and no assert text. An assert-text archive has `-asserts-` in its filename; both its ZIP metadata and compiled WASM witness identify the preset. Separate build directories and runtime fingerprints keep the two variants apart. `--assert-text` also works with `build`, `run`, `playable`, `portal-check`, and `verify`. Only the human lead starts Poki playtests.

Games using a copied `cmake/GameOptions.cmake` must allow `wasm-release-asserts` as a runtime profile. The shared `platform_sdk_crash()` CMake helper installs the assert reporter automatically in FULL web builds; games that do not use that helper still get FULL assertions on stderr but do not get the assertion text in the crash report. Capture starts after the game calls `nt_crash_init`.

The Studio template and live games support this mode. Frozen games retain their copied build tools until they are reopened and migrated.

## Deferred game packs

A game may declare required shipping packs in `assets/runtime-packs.json`:

```json
{
  "schema": "ai_studio.game.asset_packs.v1",
  "packs": ["assets/forest.ntpack", "assets/ruins.ntpack"]
}
```

The native `game_asset_packs` target must produce each file in `bin/assets/`.
The web builder copies every declared pack and fails when one is missing or
empty. Declarations allow at most 64 unique, flat, lower-case pack names;
`assets/game.ntpack`, traversal, wildcards and symbolic links are forbidden.
The game owns when to fetch and mount these packs; declaring them does not
preload them. Portal manifests stay game-independent.

The builder keeps `game-asset-packs.json` beside web `bin/`, outside the shipped
artifact. This bounded record lets rebuilds remove only formerly declared packs
that are no longer needed. Target-optional packs such as music retain their
existing behavior. Games without a declaration retain the existing file set.

Packaging requires every declared pack, records the declaration in
`release.json.assetPacks`, and rechecks the same exact allowlist and nonempty
payloads after reopening the ZIP. Nonempty declarations are also recorded as
`runtime-build.json.assetPacks`, inside the canonical fingerprint carried by the
compiled WASM witness. Packaging compares this witnessed list with the source
declaration; reopening compares it with `release.json.assetPacks`, so removing a
pack and rewriting ordinary ZIP/sidecar hashes cannot erase the requirement.
The declaration copy and pack bytes also participate in the release metadata
and artifact hashes. The source declaration and staging
record are not shipped as separate files.

## Older games

Games created before this module still carry their own copies in `tools/`.
Runtime automation (`preview_game.mjs`, native capture fingerprints) uses a
game's own copy when it has one and this module otherwise. Moving such a game to
the shim is a port: replace its `tools/` copies with the two entrypoints and keep
only what is really game-specific as `commands`.

## Tests

```powershell
node --test --test-concurrency=4 ai_studio/game_tools/tests/*.test.mjs
```

They run in the Studio `template-release` domain, not in each game's `test`.
