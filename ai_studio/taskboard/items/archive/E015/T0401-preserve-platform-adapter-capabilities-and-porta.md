---
id: T0401
title: Preserve platform adapter capabilities and portal evidence boundaries
status: done
project: P001
epic: E015
priority: P1
tags: [platform, adapters, portal, evidence]
created: 2026-07-10
updated: 2026-07-13
---

## What

Preserve the existing target/SDK adapter architecture, expose runtime
capabilities honestly, and keep local mock/contract proof separate from real
portal certification evidence.

## Done when

- [x] Publish target and SDK adapter remain separate choices and only the
      selected adapter is built into a package.
- [x] Runtime capabilities such as `externalLinksAllowed` are exposed through
      the existing platform boundary; gameplay does not infer portal policy.
- [x] Existing platform-sdk and mock simulator surfaces are reused and their
      local behavior has focused contract tests.
- [x] T0400 package inspection rejects target/adapter mismatch and produces the
      exact artifact used for portal smoke.
- [x] Reports label local mock, local SDK contract, public inspector,
      credentialed portal smoke, and production certification as distinct
      evidence levels; one never substitutes for another.
- [x] Missing credentials or portal access yields an explicit unverified
      result, never fabricated success.
- [x] Platform-specific evidence stays with the game/release record rather
      than becoming a global Studio claim.

## Open questions

None.

## Log

- 2026-07-10: Split from oversized T0361 during plan review.
- 2026-07-10: Execute after T0400 produces the final inspected package.
- 2026-07-13: Execution checkpoint after T0400 commit 23d87d8e3 and green Windows/Linux run 29242585709. Scope is a copied game-owned offline portal evidence reporter bound to the exact verified ZIP plus manifest, five honest evidence levels, local output only, focused tests, and short taxonomy docs; no SDK/runtime/adapter changes, credentials, network/browser portal claims, benchmark loops, E017, or engine edits.
- 2026-07-13: TDD RED began with the missing reporter module and then proved missing Studio registration, missing no-clobber publisher export, and a transient outside write in the injected symlink race. GREEN adds a copied offline reporter that reuses T0400 `verifyWebPackage`, binds exact ZIP and sidecar identity/target/adapter/name/size/SHA-256, and writes deterministic game-local evidence with an atomic hard-link from a trusted canonical-root temp.
- 2026-07-13: Evidence levels remain exact and non-substitutable: only `local-sdk-contract` is `pass`; `local-mock`, `public-inspector`, `credentialed-portal-smoke`, and `production-certification` are explicit `unverified` rows with their next required evidence. The reporter reads no credential/environment value, performs no network/browser call, and the CLI says evidence was recorded rather than claiming portal success.
- 2026-07-13: Focused evidence: implementation suite 95/95 in 50.8 seconds; lead reporter/Studio/platform-sdk recheck 47/47 in under one second; copied standalone-game CLI, exact hashes, deterministic retry, corrupt/mismatch/out-of-root/symlink rejection, injected parent swap with zero outside mutation, and concurrent conflicting no-clobber publication all pass. The actual T0401 path set routes only fast owners and does not select full, native, web, audio, CMake, or WASM work.
- 2026-07-13: Two independent read-only reviews converged after fixes: architecture/security/ownership 0 HIGH, 0 MEDIUM, 0 actionable LOW; tests/process/performance/CI 0 actionable. Quality: QTECH_001=pass; no benchmark loop, portal credential/access, SDK runtime/adapter change, E017 file, or engine edit was introduced.
- 2026-07-13: Closure: game-owned offline portal evidence is bound to the exact T0400 ZIP and sidecar, five evidence levels remain honest and distinct, copied-game and failure/concurrency boundaries pass, and both independent reviews report zero actionable findings.
- 2026-07-13: Quality: QTECH_001=pass; evidence: Focused implementation 95/95; lead reporter/Studio/platform-sdk 47/47; architecture and test/process reviews 0 actionable; no CMake/WASM, benchmark, credential, network, E017, or engine mutation.
