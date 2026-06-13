---
id: T0027
title: Run final child-test readiness review
status: review
epic: ""
priority: P1
tags: [release, review, child-test, audio, validation]
created: 2026-06-12
updated: 2026-06-13
---

## What

Run an automated native PC product-review pass for child-test readiness. The
review should exercise the first playable path, upgrade comprehension, stuck
board recovery, desktop and portrait screenshot evidence, package presence, and
audio cue status. This is a review gate, not a claim that manual child testing
is complete.

## Done when

- [x] A native DevAPI scenario produces a durable child-test readiness report.
- [x] The report covers first loop, FTUE, upgrade comprehension, stuck-board
      recovery, portrait readability evidence, package presence, and audio
      status.
- [x] The report clearly states remaining release blockers instead of claiming
      release readiness prematurely.
- [x] Scenario screenshots and pixel health evidence exist.
- [x] Task/status files point to the report and next blockers.

## Open questions

None.

## Log

- 2026-06-12: Started from the remaining release gate after native mobile
  portrait validation. Engine search found no implemented audio API
  (`nt_audio`, `audio_play`, `AudioClip`, `AudioVoice` absent under
  `external/neotolis-engine/engine`), only persisted volume settings and design
  docs. The review will therefore record audio as a release blocker rather
  than pretending sound is implemented.
- 2026-06-13: Polished native review visuals while running the gate: CTA now
  uses the generated field button slice9 background, portrait board spacing no
  longer pushes the lower fence into the collection drawer, collection label is
  fit/centered, and the raw pink feedback bar was replaced by text feedback.
- 2026-06-13: Evidence passed: `py -3.12 -m py_compile tools/devapi/scenarios/child_test_readiness.py`;
  `cmake --build --preset native-debug`;
  `cmake --build --preset native-release`;
  `node tools/package_native_release.mjs`.
- 2026-06-13: Evidence passed: `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9266 build/reports/child_test_readiness_v5.json build/captures/scenarios/child_test_readiness_v5`.
  Report says `automated_review_passed=true`, package ok, `ready_for_manual_child_test=true`,
  and `release_ready=false` because audio playback is not implemented and
  manual child-test/user acceptance is still required.
- 2026-06-13: Pixel health passed for all v5 screenshots:
  `child_test_desktop_first_loop.png`, `child_test_desktop_upgrade.png`,
  `child_test_desktop_stuck.png`, `child_test_portrait_first_loop.png`, and
  `child_test_portrait_stuck.png`.
- 2026-06-13: Refreshed the readiness gate after optional evidence packaging:
  `py -3.12 tools/devapi/scenarios/child_test_readiness.py 9356 build/reports/child_test_readiness_v20_optional_evidence_package.json build/captures/scenarios/child_test_readiness_v20_optional_evidence_package`
  passed. Report result: `automated_review_passed=true`,
  `ready_for_manual_child_test=true`, `release_ready=false`, package ok, audio
  backend `winmm-waveout-generated-pcm`, desktop cue counts `spawn=3`,
  `merge=1`, `upgrade=2`, `recycle=1`, `blocked=1`, portrait cue counts
  `spawn=2`, `merge=1`.
- 2026-06-13: Pixel health passed for all five v20 screenshots:
  `child_test_desktop_first_loop.png`, `child_test_desktop_upgrade.png`,
  `child_test_desktop_stuck.png`, `child_test_portrait_first_loop.png`, and
  `child_test_portrait_stuck.png`.
- 2026-06-13: Updated release audit to use the v20 child-test readiness report
  and screenshots. Evidence passed:
  `py -3.12 tools/release_candidate_audit.py --output build/reports/release_candidate_audit_v14_current_readiness.json`.
  Result: `automated_gates_passed=true`, `release_ready=false`, blocker remains
  real manual child-test/user acceptance only.
