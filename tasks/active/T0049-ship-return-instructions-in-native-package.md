---
id: T0049
title: Ship return instructions in native package
status: review
epic: ""
priority: P1
tags: [release, packaging, child-test, handoff, qa]
created: 2026-06-12
updated: 2026-06-12
---

## What

Ship a visible `RETURN_INSTRUCTIONS.txt` in the native release package before
child-test export happens, so an adult tester knows exactly what file to return
after `EXPORT_CHILD_TEST_RESULTS.bat` succeeds.

## Done when

- [x] `RETURN_INSTRUCTIONS.txt` is generated into the staged package.
- [x] Package manifest/checksums/zip/self-check include the file.
- [x] Package smoke validates the file and its required text.
- [x] Release audit passes automated gates with only real manual child-test
      acceptance missing.
- [x] Task/status files point to the validation evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after package handoff review found that
  `RETURN_INSTRUCTIONS.txt` existed only inside the exported return zip, not in
  the original package visible to testers before export.
- 2026-06-13: Added generated package-level `RETURN_INSTRUCTIONS.txt`; package
  manifest, checksums, zip, and self-check now include it. Export now copies
  the package instruction file into `child_test_results_for_return.zip`.
- 2026-06-13: Rebuilt package with `node tools/package_native_release.mjs`.
  Output: `67-world.exe` 775680 bytes, `assets/world67_art.ntpack` 20995020
  bytes, zip 21806246 bytes.
- 2026-06-13: Evidence passed:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9358 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`.
  Smoke now checks `RETURN_INSTRUCTIONS.txt`, sees 19 self-check files, proves
  the return zip still includes `RETURN_INSTRUCTIONS.txt`, and leaves no
  generated child-test reports or return zip in the staged package.
- 2026-06-13: Final audit passed:
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v19_return_instructions_package.json`.
  Result: `automated_gates_passed=true`, `release_ready=false`; blocker remains
  real manual child-test/user acceptance only.
