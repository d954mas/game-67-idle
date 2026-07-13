# Game-owned release

This folder owns release commands and evidence for the copied game. Build,
package, and verify an itch upload without a Studio registry:

```text
node tools/game.mjs doctor
node tools/game.mjs test
node tools/game.mjs playable --target itch
node tools/game.mjs package --target itch
node tools/game.mjs verify --target itch
```

`package` builds `wasm-release-<target>`, consumes the exact allowlist from the
selected `features/platform-sdk/publish-targets/<target>.json`, writes a
deterministic STORE ZIP, reopens it, and verifies paths/case, CRC, sizes,
hashes, entrypoint, target/adapter, release metadata, required assets, and the
exact dependency record. The compact adjacent manifest is bound to the final
ZIP hash and every reopened entry. Outputs live in ignored
`release/artifacts/`.

Final packages require game-owned `game.json` and `dependencies.json` with
exact engine/feature revisions. DevAPI/debug/source payloads, unexpected or
missing files, adapter drift, and placeholder Playgama configuration fail
before publication.

## CI ownership

`.github/workflows/game-verify.yml` is inactive while nested in the parent
Studio repository. In a standalone game repository it checks out the exact
public Studio revision recorded by the feature dependency rows, initializes
its engine submodule, mounts the game under `games/<game-id>`, and runs the same
Windows/Linux `game verify` contract. The canonical public Studio repository is
declared by `STUDIO_REPOSITORY` in that owned workflow. Credentials and portal
submission remain game-owned.

`--template-proof` and `--skip-tests` exist only so the parent Studio gate can
reuse its already-built, already-unit-tested reference artifact. A real game
release must use plain `game verify` and must not bypass its tests.
