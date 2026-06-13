---
id: T0029
title: Add Windows release branding metadata
status: review
epic: ""
priority: P1
tags: [release, native, branding, windows, validation]
created: 2026-06-12
updated: 2026-06-13
---

## What

Add release-quality Windows branding to the native PC executable without
editing the engine submodule: embedded app icon, product/version metadata, and
package evidence that the final `67-world.exe` carries the branding.

## Done when

- [x] A durable 67 World `.ico` is generated from accepted runtime/visual assets
      and stored outside temp/build output.
- [x] Native Windows build embeds the icon and VERSIONINFO metadata into
      `game_seed.exe`, then packaging copies it to `67-world.exe`.
- [x] Package manifest/checksums include the branded executable and zip.
- [x] Automated validation proves ProductName/FileDescription/Version metadata
      on the packaged `67-world.exe`.
- [x] Package smoke and child-test readiness still pass after the branding
      change.
- [x] Task/status files point to the evidence and remaining blocker.

## Open questions

None. Use existing accepted 67 badge/character art for the app icon; do not
generate a new art direction in this pass.

## Log

- 2026-06-13: Started after package self-containment reached review. Scope:
  Windows native branding only; no gameplay, engine submodule, or web changes.
- 2026-06-13: Generated durable icon:
  `py -3.12 tools/assets/make_windows_icon.py` produced
  `assets/runtime/67-world/67-world.ico` from the accepted 67 badge art.
- 2026-06-13: Static validation passed:
  `py -3.12 -m py_compile tools/assets/make_windows_icon.py tools/devapi/scenarios/package_release_smoke.py`
  and `node --check tools/package_native_release.mjs`.
- 2026-06-13: Native debug and release builds passed:
  `cmake --build --preset native-debug` and
  `cmake --build --preset native-release`; both compiled
  `src/windows/67_world.rc.res`, embedding icon and VERSIONINFO resources.
- 2026-06-13: Release package regenerated with branding:
  `node tools/package_native_release.mjs` produced
  `build/release/67-world-pc/67-world/67-world.exe` (775168 bytes),
  `build/release/67-world-pc/67-world/assets/world67_art.ntpack`
  (20995020 bytes), and `build/release/67-world-pc/67-world-pc.zip`
  (21773881 bytes).
- 2026-06-13: Package smoke passed:
  `py -3.12 tools/devapi/scenarios/package_release_smoke.py 9292 build/captures/scenarios/package_release_smoke_v4_branding.png`.
  The smoke verified package files, checksums, valid zip, nonblank screenshot,
  Windows VERSIONINFO values (`ProductName=67 World`,
  `FileDescription=67 World native PC game`, `FileVersion=1.0.0.0`,
  `ProductVersion=1.0.0.0`, `OriginalFilename=67-world.exe`), plus
  `ICON`, `GROUP_ICON`, and `VERSIONINFO` resources in the packaged exe.
- 2026-06-13: Pixel health passed for
  `build/captures/scenarios/package_release_smoke_v4_branding.png`.
- 2026-06-13: Child-test readiness passed:
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9294 build/reports/child_test_readiness_v11_branding.json build/captures/scenarios/child_test_readiness_v11_branding`.
  Report result: `automated_review_passed=true`, package ok with manifest,
  checksums and zip, audio backend `winmm-waveout-generated-pcm`,
  `ready_for_manual_child_test=true`, `release_ready=false`.
- 2026-06-13: Pixel health passed for all five v11 readiness screenshots under
  `build/captures/scenarios/child_test_readiness_v11_branding/`.
  Remaining blocker: manual child-test/user acceptance.
