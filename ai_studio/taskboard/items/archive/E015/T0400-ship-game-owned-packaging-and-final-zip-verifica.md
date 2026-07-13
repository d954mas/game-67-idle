---
id: T0400
title: Ship game-owned packaging and final ZIP verification
status: done
project: P001
epic: E015
priority: P1
tags: [packaging, game-owned, zip, verification]
created: 2026-07-10
updated: 2026-07-13
---

## What

Copy a Node-based packaging scaffold into each new game, make that game own
its packaging evolution, and validate the actual final ZIP rather than an
intermediate directory.

## Done when

- [x] The reference template carries the packaging scaffold and T0356/new_game
      copies it into the created game; Studio retains no mutable per-game
      packaging registry.
- [x] Packaging uses Node, creates the final target ZIP, reopens that ZIP, and
      validates required files, paths/case, sizes, hashes, entrypoint, selected
      target/adapter, and release metadata.
- [x] DevAPI/debug payloads, source-only files, placeholder ZIP configuration,
      target/adapter mismatch, missing assets, and unexpected files fail early.
- [x] The package emits a compact artifact manifest with hashes and the tested
      engine/feature versions from T0355/T0361.
- [x] The reference template includes a copied game-owned `game verify`
      scaffold; this task invokes the already-landed T0353 verification contract
      to prove that scaffold on Windows/Linux without discovering or running
      arbitrary workspace games.
- [x] Each game owns doctor, tests, playable proof, package validation, CI, and
      release credentials after creation.
- [x] Focused ZIP corruption/mismatch tests and full reference-template
      packaging proof pass.

## Open questions

None.

## Log

- 2026-07-10: Split from oversized T0361 during plan review.
- 2026-07-10: Execute after T0353, T0355, T0356, T0361, and T0397.
- 2026-07-10: Review cycle 2 assigned verification of the newly created scaffold to this task; T0353 is consumed, not reopened.
- 2026-07-13: Execution checkpoint after T0399 commit 53c4ccbd5. All dependencies T0353/T0355/T0356/T0361/T0397 are done. Scope is the copied template game-owned Node package/doctor/verify/CI scaffold, deterministic final ZIP plus sidecar verification, focused tests, and Studio reference-template registration; no arbitrary game discovery, E017, external engine, credential use, or portal evidence.
- 2026-07-13: TDD RED was 0/2 module loads because game.mjs and package_web.mjs did not exist. GREEN adds the copied game-owned doctor/build/run/test/playable/package/verify CLI, release docs, ignored outputs, and a nested CI scaffold whose activation preconditions are explicit.
- 2026-07-13: ZIP/package proof is deterministic STORE only and fail-closed: strict reopen checks CRC, sizes, hashes, path syntax/case/collisions, local/central agreement, hidden gaps, exact itch manifest allowlist, entrypoint, target/adapter, release flags, required assets, forbidden debug/DevAPI/source payloads, and exact self-hashed dependency records. Invalid outputs remain in a temporary sibling and are removed before immutable hash-named finals publish.
- 2026-07-13: Focused evidence: Studio/build/game/package 31/31; new_game/new_template copied-scaffold regression 48/48; actual existing itch web artifact refreshed through platform_sdk_web_assets then `game verify --no-build --template-proof --skip-tests` reopened and passed final ZIP plus sidecar verification. T0353 Windows/Linux command wiring is present but remains unchecked until the committed CI run proves both runners.
- 2026-07-13: Game-owned facade evidence: `game test` passed 17/17 and `game playable --target itch --no-build` passed over the same release artifact. No benchmark loop, arbitrary `games/` discovery, second CMake/web build, mutable Studio game registry, shell packager, portal credential, or E017/engine edit was introduced.
- 2026-07-13: Review cycle 1 combined two independent read-only reviews: 4 unique HIGH and 5 unique MEDIUM, 0 LOW. Fixed native/CTest false greens, exact dependency-source/version/revision/cleanliness proof, standalone game-CI dependency-layout reconstruction, debug/DevAPI/invalid WASM rejection, exact platform wrapper/core/adapter bytes, template-only test bypass, precise release-config parsing, byte-canonical ZIP verification, and transactional ZIP/manifest publication.
- 2026-07-13: Review-fix evidence: focused package tests 15/15; game-owned CLI tests 8/8; combined Studio facade/template copy/build/game/package regressions 91/91 in 53 seconds. The existing itch artifact was reused without a CMake/WASM rebuild and `game verify --target itch --no-build --template-proof --skip-tests --out build/package-proof` reopened and passed `template-itch-38d85723e8a4af4b.zip` plus sidecar.
- 2026-07-13: Quality: QTECH_001=review; evidence: changed packaging, dependency, native-test, copied-CI, and final-ZIP claims have focused executable proof; final independent convergence and committed Windows/Linux T0353 evidence remain open.
- 2026-07-13: Review cycle 2 closed the remaining 1 HIGH and 5 unique MEDIUM findings. The release bootstrap proof now lexes executable inline scripts instead of matching raw HTML strings, ignores comment/string/external-script-body decoys, and proves an executable `game.js` load. Web packaging requires exactly one canonical `features/platform-sdk` dependency; release WASM must contain a non-empty core/export module shape; publication writes the manifest first and ZIP last as the commit marker, tracks rename ownership, and preserves a concurrent writer's valid pair on rollback. The copied-game execution fixture now copies `tools/`, `.github/`, and `release/` from its current game module root while using Studio only for shared dependencies.
- 2026-07-13: Cycle 2 TDD evidence: focused RED was 20/24 with the four grouped regressions failing (bootstrap decoys, empty WASM, platform dependency cardinality, concurrent publication ownership). Focused GREEN is 24/24 for `package_web.test.mjs` plus `game.test.mjs` in 2.4 seconds. No CMake/native/web build, benchmark loop, staging, or commit was run; Windows/Linux CI evidence remains open.
- 2026-07-13: Final convergence fixed the remaining publication, dependency, bootstrap, and loader false greens. The manifest is an idempotent pre-commit object and the ZIP is the sole commit marker; canonical `platform-sdk` is re-proved from sources and after ZIP reopen; executable config/bootstrap parsing ignores string, comment, regex, inert-script, unattached, and explicit dead-branch decoys; the release loader proves one linked live Emscripten chain from `game.wasm` through its instantiate helper to a top-level `createWasm()` invocation before packaging and after reopen.
- 2026-07-13: Final local evidence: focused package/game 26/26; combined Studio facade/CI, game/template copy, web-build, game, and package regressions 96/96 in 39 seconds; syntax checks green; the existing real minified Emscripten artifact passed `game verify --no-build --template-proof --skip-tests` as `template-itch-38d85723e8a4af4b.zip`. No CMake/WASM rebuild, arbitrary game discovery, benchmark loop, credential access, or E017/engine edit was used.
- 2026-07-13: Two independent final read-only reviews (architecture/correctness/ownership and tests/process/performance/context cost) converged after targeted fixes at 0 HIGH, 0 MEDIUM, 0 LOW, and 0 actionable. Quality remains QTECH_001=review only until the committed Windows/Linux full CI proves the copied T0353 scaffold.
- 2026-07-13: Committed Windows/Linux proof: GitHub Actions run 29242585709 passed commit 23d87d8e3 on ubuntu-latest and windows-latest; the full gate built native and itch WASM once per runner, reopened the final ZIP through game verify, and passed all 28 Studio suites.
- 2026-07-13: Quality: QTECH_001=pass; evidence: Focused package/game 26/26; staged combined regressions 96/96; independent final reviews 0 actionable; GitHub Actions 29242585709 green on Ubuntu and Windows.
