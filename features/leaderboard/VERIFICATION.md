# Leaderboard integration verification

The review fixes cover the shared facade, HTTP and portal backends, platform
SDK leaderboard bridge, manifest validation, and a game consumer's screen-open
path. The independent lap-time consumer lives in `example/`.

## Verified behavior

- Opening the game screen requests a page. A request waits during SDK startup;
  a ready backend without reading does not leave the UI loading.
- Failed reads and writes retry with separate bounded budgets. A successful
  operation cannot hide the other operation's exhausted error. Manual refresh
  restarts the budgets; rate limits back off and login-refused reads resume on
  an auth change. HTTP retains ownership of its polling cadence.
- HTTP polling receives current metadata even when an unchanged accepted score
  does not need submission. Relaunch does not erase the player's skin/level.
- HTTP ranking supports both sort orders, stable ties, zero-valued rows and
  count-of-better interpolation supplied by the corresponding service.
- A previous day's submission cannot acknowledge a current-day score.
- Yandex neighbourhood trimming preserves the player and nearby rows.
- Web callbacks from a previous listener generation cannot dispatch results or
  stage rows into the new leaderboard session.
- Manifest validation rejects unsupported mixtures of backend families,
  duplicate portal IDs, invalid IDs and multiple CrazyGames boards.
- The generated lap-time example compiles as an independent consumer and
  exercises ascending score coalescing, explicit refresh, views and shutdown.

The regression tests were run failing before their fixes. Two independent
reviewers checked the async changes; their findings were fixed and rechecked.

## Repeatable checks

From the Studio root:

```sh
node --test features/leaderboard/tests/leaderboards.test.mjs features/platform-sdk/tests/platform_sdk.test.mjs
node templates/template/tools/game.mjs test --only test_leaderboard --only test_leaderboard_portal --only test_leaderboard_http --only test_leaderboard_core --only test_leaderboard_example
```

Recorded result: 76 JavaScript tests passed; all five freshly built native
CTest targets passed. `test_leaderboard_example` is also registered in the
template. Its CTest label is `core`.

## Verification limits

The broader game core run passed 118 of 120 targets. The failures were in
`test_rewarded_offers` and `test_platform_sdk` advertising cases, alongside
concurrent changes to those SDK paths. They are not claimed as passing.

The complete Yandex release build subsequently passed after the game source
list included the SDK storage/cloud implementations. Browser execution against
the official Yandex local dev proxy verified reads, score submission and
reload persistence. Game-owned commands, artifact identity, screenshots and
limits are documented in
the consuming game's private release verification report.
The debug configuration's existing memory conflict (`MAXIMUM_MEMORY cannot
be less than INITIAL_MEMORY`) was not changed. No engine files were changed.

Production vendor execution and portal publication are unverified. The game
currently disables its login UI, so guest-to-account login remains unverified.
The example test proves module integration, not
visual acceptance or vendor availability. A new-game generation attempt was
stopped by the tool's clean-template requirement; concurrent template changes
were preserved and the example was tested through its own generated manifest
and executable instead.

Quality: QTECH_001=pass for the named module regressions and example;
QTECH_001=pass for Yandex release build and official local SDK runtime;
QTECH_001=unverified for production portal acceptance.
