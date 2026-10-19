# Game Tools

The build, test, package and release CLI every game runs. It used to be copied
into each game's `tools/`, and every copy drifted; it now lives here once and a
game keeps two thin entrypoints:

- `tools/game.mjs` passes the game directory, and the game's own extra commands,
  to `game_cli.mjs`.
- `tools/build_web.mjs` passes the game directory to `build_web.mjs`, so the
  documented `node tools/build_web.mjs --preset ...` keeps working.

Both locate the Studio with `tools/lib/studio_root.mjs`, which stays in the game
because it is what finds this module.

Games already compile `features/` and the engine from the Studio checkout they
sit in, so loading the CLI from the same checkout adds no new coupling.

## Contents

- `game_cli.mjs`: `doctor`, `build`, `run`, `test`, `playable`, `package`,
  `portal-check`, `verify`; test tiers come from CTest labels. `test` and
  `verify` keep test output in `build/logs/game-test.log` and print one summary
  line, or the failing step's tail and the log path; `--verbose` streams it all.
  `clean` removes build variants and keeps `build/native-debug`; `clean --all`
  removes the whole `build/`.
- `build_web.mjs`: the web build plan and runtime build fingerprint.
- `package_web.mjs`, `package_web_smoke.mjs`, `minify_web_release.mjs`,
  `inject_asset_bytes.mjs`: the deterministic portal package and its proof.
- `portal_evidence.mjs`: release evidence for a packaged artifact. Run it from
  the game root.
- `serve_web.mjs`: a loopback server for a built artifact. Run it from the game
  root, or through `node tools/game.mjs run`.
- `lib/runtime_build.mjs`: runtime source and build records. For a game without
  copied tooling the record includes a `tooling` input: the files here that
  change artifact bytes (`build_web`, `inject_asset_bytes`, `minify_web_release`).
- `lib/copied_tooling.mjs`: the one test for a game that still carries copied
  tooling (`tools/package_web.mjs` present).

ZIP writing comes from `ai_studio/core_harness/tool_lib/zip_store.mjs`.

## Game-specific commands

A game adds commands in its shim; they are tried before the shared ones:

```js
process.exitCode = await main(process.argv.slice(2), {
  gameDir: GAME_DIR,
  commands: { dev: async (argv, { gameDir }) => "dev loop stopped" },
});
```

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
