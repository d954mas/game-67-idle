---
id: T0048
title: Add guided document actions to packaged start menu
status: review
epic: ""
priority: P1
tags: [release, packaging, handoff, child-test, qa]
created: 2026-06-13
updated: 2026-06-13
---

## What

Make the packaged `START_HERE.bat` menu open the two required child-test
documents directly, so an adult tester can follow the package flow without
searching the folder manually.

## Done when

- [x] `START_HERE.bat` exposes menu actions for `PARENT_OBSERVER_GUIDE.md` and
      `CHILD_TEST_ACCEPTANCE.md`.
- [x] Package smoke validates the new menu tokens and executes the document
      actions without launching the game or leaving child-test artifacts.
- [x] The package is rebuilt and release audit still reports automated gates
      passing with only manual child-test acceptance missing.
- [x] Task/status files point to the validation evidence.

## Open questions

None.

## Log

- 2026-06-13: Started after reviewing the manual child-test handoff: the
  package told adults to read the guide and acceptance kit, but `START_HERE.bat`
  did not provide menu actions to open those files directly.
- 2026-06-13: Updated package generation so `START_HERE.bat` adds direct menu
  choices for the parent observer guide and child-test acceptance kit. Updated
  package smoke to validate the new menu tokens and execute both document
  choices.
- 2026-06-13: Rebuilt package with `node tools/package_native_release.mjs`.
  Output: `67-world.exe` 775680 bytes, `assets/world67_art.ntpack` 20995020
  bytes, zip 21805213 bytes.
- 2026-06-13: Evidence passed:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9358 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`.
  Smoke now validates `choice /C 12345678Q`, executes choice `2` and sees
  `# 67 World Parent Observer Guide`, executes choice `3` and sees
  `# 67 World Child-Test Acceptance Kit`, then still validates report creation,
  normal/fresh launch, export bundle, cleanup, zip, self-check, branding, and
  framebuffer proof.
- 2026-06-13: Strengthened release audit to require the new `START_HERE.bat`
  menu tokens, then ran
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v15_guided_start_menu.json`.
  Result: `automated_gates_passed=true`, `release_ready=false`, blocker remains
  real manual child-test/user acceptance only.
- 2026-06-13: Fixed a guided-menu numbering mismatch found during follow-up
  review: the recommended child-test order said step 4 was fresh child-test,
  but the menu had normal play at `[4]` and fresh child-test at `[5]`.
  `START_HERE.bat` generation now puts fresh child-test at `[4]` and normal
  play at `[5]`; package smoke was updated to execute the corrected choices.
- 2026-06-13: Evidence passed after the corrected menu order:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9358 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`.
  Smoke executed choice `4` as fresh child-test and choice `5` as normal play,
  verified both launch the packaged exe, and left no generated child-test
  reports or return zip in the staged package.
- 2026-06-13: Final audit passed after the corrected menu order:
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v16_guided_menu_order.json`.
  Result: `automated_gates_passed=true`, `release_ready=false`; blocker remains
  real manual child-test/user acceptance only.
- 2026-06-13: Fixed a second handoff wording mismatch: the recommended
  child-test order now prints exact menu choices `[1]`, `[2]`, `[3]`, `[4]`,
  `[6]`, `[7]`, and `[8]`, so the report/validate/export steps no longer look
  like menu choices `[5]`, `[6]`, and `[7]` while `[5]` is normal play.
- 2026-06-13: Rebuilt package with `node tools/package_native_release.mjs`.
  Output: `67-world.exe` 775680 bytes, `assets/world67_art.ntpack` 20995020
  bytes, zip 21805220 bytes.
- 2026-06-13: Evidence passed after exact menu-choice labels:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9358 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`.
  Smoke now validates `[4] Start fresh child-test`, `[6] Create report after
  the session`, `[7] Validate the filled report`, and `[8] Export validated
  child-test results zip` in `START_HERE.bat`.
- 2026-06-13: Final audit passed after exact menu-choice labels:
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v17_start_here_order_labels.json`.
  Result: `automated_gates_passed=true`, `release_ready=false`; blocker remains
  real manual child-test/user acceptance only.
- 2026-06-13: Added the same guided child-test menu path to packaged
  `README.txt`, so testers who open the readme instead of `START_HERE.bat`
  still see the exact choices `[1]`, `[2]`, `[3]`, `[4]`, `[6]`, `[7]`, and
  `[8]`. Package smoke now validates these README tokens.
- 2026-06-13: Rebuilt package with `node tools/package_native_release.mjs`.
  Output: `67-world.exe` 775680 bytes, `assets/world67_art.ntpack` 20995020
  bytes, zip 21805524 bytes.
- 2026-06-13: Evidence passed after README guided path:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9358 build/captures/scenarios/package_release_framebuffer_proof_v2_clean_smoke.png`.
  Smoke now includes `package readme lists guided child-test menu path` and
  still validates launch, report, export, cleanup, zip, self-check, branding,
  and framebuffer proof.
- 2026-06-13: Final audit passed after README guided path:
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v18_readme_guided_path.json`.
  Result: `automated_gates_passed=true`, `release_ready=false`; blocker remains
  real manual child-test/user acceptance only.
