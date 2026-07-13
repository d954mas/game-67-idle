---
id: T0396
title: Quarantine skeletal animation as an incomplete experimental extension
status: done
project: P001
epic: E015
priority: P1
tags: [skeletal, experimental, cleanup]
created: 2026-07-10
updated: 2026-07-11
---

## What

Move the current skeletal-animation proof under an explicitly experimental
boundary and prevent its incomplete APIs from being mistaken for a production
reusable feature. Preserve evidence; do not complete the renderer in this task.

## Done when

- [x] All eight tracked skeletal files are inventoried and moved behind the
      accepted `extensions/experimental/` ownership with link/build parity.
- [x] The extension is excluded from default template, package, feature, and
      CI discovery and requires an explicit experimental opt-in.
- [x] README/contract state the actual implementation: Ozz sampling plus CPU
      mesh proof, no completed renderer/GPU-buffer contract, no production
      performance claim, and no game-ready lifecycle guarantee.
- [x] Reproducible defect evidence covers non-finite/huge `wrap_time`,
      pre-sample matrix access, missing skeleton-to-mesh joint-order mapping,
      C++ exceptions crossing the C ABI, mesh-before-instance lifetime/UAF,
      non-finite weights/matrices, and the exported silent no-op draw path.
- [x] Any public operation that is not implemented fails explicitly or is
      removed from the claimed contract; no silent success/no-op remains.
- [x] CMake changes are target-local, do not globally mutate compile/runtime
      flags, and register retained probes as explicit experimental checks.
- [x] Existing probes and docs stop referencing stale T0007/current-production
      status, duplicated commands, or hardcoded asset assumptions as universal.
- [x] No engine change, GPU-skinning project, or completion of skeletal runtime
      is introduced.

## Open questions

None.
## Log

- 2026-07-10: Split from T0358 after 14/14 residual file review found severe
  correctness/lifecycle gaps beyond the previously known wrap-time defect.
- 2026-07-10: Runtime completion/fixes are explicitly future work outside E015; quarantine scope is fully decided.
- 2026-07-11: Checkpoint: inventoried the eight tracked skeletal-extension files; starting explicit experimental ownership quarantine without renderer completion or engine changes.
- 2026-07-11: Evidence: all eight original payload files now exist only under
  `extensions/experimental/skeletal_animation`; the quarantine check validates
  the exact inventory, old-path absence, default feature/template/game/package/
  workspace/CI exclusion, explicit opt-in, target-local flags, defect ledger,
  and removed draw API.
- 2026-07-11: Evidence: no-opt Ninja configure is rejected by the experimental
  guard; opt-in Ninja configure succeeds against a minimal explicit Ozz seam,
  registers only experimental checks, and the quarantine CTest passes 1/1.
  Repository-owned `.ozz` fixtures do not exist, so no Ozz runtime success is
  claimed.
- 2026-07-11: Evidence: Clang syntax checks pass for both C probes. A direct
  native build/run of the CPU mesh source and contract probe passes its 11
  existing failure paths and reproduces four safe known defects: NaN weights,
  infinite inverse-bind data, positional joint order, and NaN pose propagation.
  Pre-sample Ozz evidence is in the fixture-gated probe; hang/UAF/exception
  cases have bounded subprocess recipes and are never default tests.
- 2026-07-11: Evidence: doc-reference, strict Architecture Map
  (`mapped=347`, `scanned=780`, all error counts zero), Taskboard, quarantine,
  and cached diff validations pass; searches find no stale production/T0007
  claims or default discovery route.
- 2026-07-11: Review convergence: cycle 1 fixed MSVC/Ozz ABI parity, executable
  defect evidence, exact inventory/discovery coverage, and unused Ozz build
  cost. Cycle 2 corrected the remaining lifecycle wording. Final independent
  architecture and process reviews report 0 HIGH and 0 actionable findings.
- 2026-07-11: Quality: QTECH_001=pass; evidence: explicit quarantine boundary,
  exact move inventory, no-opt and opt-in configure proofs, native defect probe,
  experimental CTest, strict repository validation, and two clean independent
  reviews.
- 2026-07-11: Closed after two review cycles: 0 HIGH, 0 actionable; experimental quarantine and validation proofs pass.
- 2026-07-11: Wave 1 integration correction: the new top-level experimental
  quarantine is explicitly represented in Architecture Map ownership and
  tracked shallow coverage; 22/22 map tests and strict 352/784 validation pass.
