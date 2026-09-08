# Platform SDK and default playtime verification

## Scope

SDK 2.0 separates portal transport, operation identity, save reconciliation,
game-owned conflict decisions, and release target identity. Game-state 4.7
adds persisted active playtime. The template and all six private games call
the common monotonic counter from their existing gameplay lifecycle.

The optional `game-state/game_save_cloud` runtime owns reusable save orchestration;
`systems/sys_cloud_save` only binds the SDK transport. The game retains explicit
startup and safe-point calls, its progress policy, and world/UI rebinding. Save
configuration installs migrations and the composed validator once; automatic and
manual decisions share `game_save_choice_t`.

## QTECH_001: executable contracts

- Native template application builds. Nine focused CTest suites pass: save,
  blocked-save recovery, template composition, save policy, cloud integration,
  sync coordinator, platform SDK, platform storage, and platform hooks.
  Save regressions cover legacy zero, new-game reset, monotonic pause boundaries,
  backward clocks, invalid import rollback, NTGS reload, and exact int64
  saturation. The JSON and text-only shells share the same integer counter;
  callers provide only the game's active gate.
- The isolated coordinator/cloud integration suites pass 33 cases, including
  remote-first and empty-first startup, idempotent start, live-state rollback,
  unchanged-document policy cadence, and safe-point policy revalidation.
- SDK adapter, storage, WASM bridge, target descriptor, runtime profile,
  build, package, game command, and portal evidence Node suites pass.
- The permanent `tests/platform_sdk_web.test.mjs` fixture compiles the real C
  SDK to WASM and exercises early lifecycle replay, stale ad/read callbacks,
  overlay visibility, and storage acknowledgment in Chrome.
- Native application builds pass for four private game consumers.
- Two older private consumers include the counter and compile the shared save
  sources, but full builds remain blocked by existing rendering API drift:
  `NT_BLEND_MODE_ALPHA`, material `ready`/shader fields, and `nt_batch_key`.
  Their full application behavior is therefore not verified.
- The full game-state feature runner passes its Python suites with normal
  temporary paths, but Windows denies execution/cleanup of its generated
  `test_game_save_writer.exe` in the user Temp directory (`WinError 5`). The
  writer, text codec, and generated C fixtures execute successfully with a
  workspace temporary directory; that override makes the external-provenance
  Python fixture inapplicable because its temporary game is then inside Studio.
  Neither combined runner invocation is reported as an overall pass.
- The repository-wide feature validator remains blocked by an unrelated
  leaderboard README contract: `README router is missing '## Purpose'`.

## Browser playtime

The final local WASM release recorded 2049 ms of active play, 2066 ms after two
seconds in Settings (17 ms at the frame boundary), and 4121 ms after reload
and another active interval. Browser console errors: zero. The scenario waits
for runtime readiness and sends the player gesture required by the existing
platform lifecycle. Evidence: `tmp/playtime-release-smoke/report.json`.
Hidden-tab exclusion is covered by monotonic gate tests and adapter review;
this scenario does not claim a real browser visibility-transition test.

## Browser automatic selection

The final local release passes all three scenarios: stronger remote progress
is adopted automatically; stronger local progress is retained and uploaded;
contradictory milestones leave the Settings choice available. Every scenario
reports zero console errors. The browser runner seeds storage before the new
runtime starts and preserves an explicit language choice, avoiding pagehide
flush and locale-adoption interference with its assertions. Evidence:
`tmp/final-release-smokes/report.json` and `tmp/cloud-policy-release-smoke/`.

The local and Yandex release artifacts are checked against current sources,
selected compile profile, and compiled WASM fingerprint. The Yandex artifact
also passes the full release artifact validator.

## QCLR_002: conflict fallback

The local release was exercised in Chrome at desktop and 390×844 viewports.
The Settings fallback uses two full-width stacked localized actions. Both
manual local and remote branches were exercised without browser errors;
remote adoption updates loaded state and language at the frame boundary.
The final mobile captures after two and six seconds retain both conflict
buttons through local autosaves; the canvas, CSS size, and window are all
390×844. Root visually inspected the settled six-second capture. The earlier
immediate-resize capture was replaced because it used a stale framebuffer.
Screenshots and scenario reports are in the workspace's ignored
`tmp/cloud-conflict-release-smoke/` and `tmp/cloud-policy-release-smoke/`
evidence directories. Core regressions preserve an unresolved conflict when
local playtime changes while still requiring a fresh read after KEEP_LOCAL.

## Limits

Local browser tests use controlled portal doubles. They do not establish
live account synchronization or portal certification. Portal-specific real
account smoke tests remain release work. Portal storage provides no common
atomic compare-and-swap guarantee for simultaneous device writes.
