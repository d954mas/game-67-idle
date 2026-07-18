# Game-owned release

This folder owns release commands and evidence for the copied game. Build,
package, and verify an itch upload without a Studio registry:

```text
node tools/game.mjs doctor
node tools/game.mjs test
node tools/game.mjs playable --target itch
node tools/game.mjs package --target itch
node tools/game.mjs verify --target itch
node tools/portal_evidence.mjs --manifest release/artifacts/<artifact>.manifest.json
node tools/portal_evidence.mjs --manifest release/artifacts/<artifact>.manifest.json --local-mock-observation .ai_studio/evidence/local-mock/<observation>.json
```

`package` builds `wasm-release-<target>`, consumes the exact allowlist from the
selected `features/platform-sdk/publish-targets/<target>.json`, writes a
deterministic STORE ZIP, reopens it, and verifies paths/case, CRC, sizes,
hashes, entrypoint, target/adapter, release metadata, required assets, and the
exact dependency record. `runtime-build.json` is generated from the game and
declared dependency source trees, embedded in every target build, and rechecked
against the HTML bootstrap, a compiled C/WASM marker, release metadata, sidecar,
and reopened ZIP. New packages use release/manifest v2; the verifier retains
read compatibility with pre-fingerprint v1 packages. The
compact adjacent manifest is bound to the final ZIP hash and every reopened entry. Outputs live in ignored
`release/artifacts/`.

`portal_evidence.mjs` does not build, repackage, upload, use credentials, or
contact a portal. It reopens the exact final ZIP through the same package
verifier and writes a deterministic game-owned record to
`.ai_studio/evidence/releases/<full-zip-sha256>/portal-evidence.json`. The local
SDK contract is always marked `pass`. Local mock is marked `pass` only when the
optional game-owned browser observation has the same canonical runtime build
source record as that ZIP and both target-specific WASM payloads carry their
compiled witnesses; otherwise it stays `unverified`. This is source-equivalence
plus executed-build proof, not a claim that local and portal WASM bytes are
identical. Public inspector,
credentialed portal smoke, and production certification remain explicitly
`unverified` until their separate evidence is attached for that exact ZIP.

Run evidence generation with exclusive ownership of the game directory and no
concurrent filesystem mutation. The local tool rejects traversal,
pre-existing symbolic links/junctions, and conflicting report publishers, but
portable Node on Windows and Linux cannot promise protection from a hostile
same-user process replacing owned directory ancestors during the call.

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
