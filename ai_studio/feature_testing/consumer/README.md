# Feature integration consumer

This fixture belongs to [Feature Testing](../README.md). Run shared-feature
checks through `node ai_studio/feature_testing/run.mjs` from the Studio root.

The consumer supplies schemas, generated content, UI/settings references,
platform adapters and registered sample assets used by integration tests.
Its sample executable is excluded from the default build.

The folder is outside the game-template catalog. Its internal `template.json`
is fixture input for toolchain tests and does not make it a game starter.
Games copy selected reference code only when needed, following its INSTALL
contract. They do not inherit this consumer through `games/new_game.mjs`.

Native tests live in `tests/` and are registered by `cmake/GameTests.cmake`.
Reusable implementations compile from their `features/` owners. Source assets
retain their existing license, provenance, origin and hashes.

The audio reference builds short cues into `assets/game.ntpack` and the sample
MP3 into `assets/music.ntpack`, with separate generated ID headers. Music is
optional: `shell_platform_frame_end` permits its request only after a complete,
playable frame has been presented. An input gesture unlocks audio but does not
start the download. Muted music waits until enabled. The resource loader owns
transient retries; after a terminal failure, turning music off and back on
remounts its pack. A missing or slow music pack never blocks the core pack or
cues. Music uses `audio_clip_stream`; its pack stays mounted until audio has
stopped during shutdown. Playback selection remains game-owned through
`game_audio_play_music`.

For a focused audio integration check that does not build the sample UI:

```powershell
node --input-type=module -e "import {runNativeTests} from './ai_studio/game_tools/game_cli.mjs'; runNativeTests(process.cwd()+'/ai_studio/feature_testing/consumer',{selection:{mode:'only',names:['test_game_audio']}},{gameTarget:'test_game_audio'});"
```

Run this from the Studio root. The test checks the actual built pack boundary,
the first-frame request gate, mute and retry behavior, and shutdown ordering.

## Native audiovisual recording

The sample host integrates [game-recording](../../../features/game-recording/README.md)
in Windows DevAPI builds. Unlock audio through normal input before using
`game.record.start` and `game.record.stop`; check `valid` and the saved media.
Production and web builds omit the capture code. The native DevAPI sample host
and `test_game_recording` build and pass together through `runNativeTests` with
`gameTarget: "game"`; the log is `build/logs/recorder-build.log`.
