---
id: T0041
title: Harden packaged release visual screenshot proof
status: review
epic: ""
priority: P1
tags: [release, validation, packaging, visual-proof, automation]
created: 2026-06-12
updated: 2026-06-13
---

## What

Package smoke currently proves that the native release exe starts, package
files/checksums/zip are valid, and helper scripts work. It does not yet provide
a strong visual proof that the packaged release rendered gameplay pixels:
Windows GDI/package-window capture can return blank/black frames in the current
environment, and an older "nonblank" package screenshot was visually weak.

Harden the release pipeline so package visual evidence proves actual rendered
67 World gameplay, not just a live process or a nonempty image file.

## Done when

- [x] Package smoke or release audit fails when the packaged visual proof is
      blank, black, near-white, or otherwise not recognizably rendered gameplay.
- [x] The proof comes from the packaged native PC release path, or from a
      clearly documented release-equivalent path if direct packaged capture is
      impossible.
- [x] The chosen capture path is deterministic enough for agents: one command
      produces a screenshot/artifact and machine-readable pass/fail result.
- [x] A visual proof artifact is recorded in `tasks/STATUS.md` and the release
      candidate audit no longer relies on the weak package-window screenshot
      assumption.
- [x] Build/package/smoke/audit/taskboard validation pass.

## Open questions

- Best likely approach: add a release-safe `--capture-framebuffer-once <path>`
  or equivalent native screenshot mode to the game executable, then invoke it
  from package smoke. Confirm feasibility before touching engine code.
- If direct framebuffer capture would require engine-submodule changes, prefer
  a game-local wrapper or documented release-equivalent alternative.

## Log

- 2026-06-13: Captured after T0040 validation exposed that current GDI package
  screenshot capture can produce black frames, while an older package smoke
  screenshot that passed pixel health was still not a strong gameplay render
  proof.
- 2026-06-13: Started implementing a release-safe packaged framebuffer capture
  path: native exe one-shot PPM readback, package smoke PNG conversion, and
  stricter package screenshot audit.
- 2026-06-13: Added native `--capture-framebuffer-once` PPM readback for release
  builds, changed package smoke to convert it to PNG and apply strict visual
  health checks, and changed release-candidate audit to require
  `build/captures/scenarios/package_release_framebuffer_proof_v1.png`.
- 2026-06-13: Validation passed: `py -3.12 -m py_compile tools/devapi/scenarios/package_release_smoke.py tools/release_candidate_audit.py`;
  `cmake --build --preset native-release`; `node tools/package_native_release.mjs`;
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9342 build/captures/scenarios/package_release_framebuffer_proof_v1.png`;
  strict `tools/devapi/pixel_health.py` on the framebuffer proof;
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v3_framebuffer_proof.json`;
  `cmake --build --preset native-debug`; and `py -3.12 tools/devapi/smoke_test.py 9344`.
