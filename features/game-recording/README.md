# Game Recording

## Purpose

Optional L0 host integration for the engine recorder and audio-core. Windows
native DevAPI builds only. It records the presented framebuffer and the actual
48 kHz stereo mix after bus/master gain. Muting or pausing produces silence;
paused voices do not advance. There is no microphone or desktop audio capture.

The implementation originated as a game-host integration and is compiled in
place. Game fixtures, input, camera, settings, assets and storage remain owned
by each game. No engine sources are changed. See [INSTALL.md](INSTALL.md).

## Public surface

`features/recording/recording.h` exposes `game_recording_register`,
`game_recording_frame` and `game_recording_shutdown`. Host wiring is explicit;
the module owns only the recording lifecycle and DevAPI commands below.

Discover commands first. Unlock audio through normal player input before
`game.record.start` with `{"name":"C:/existing/folder/take.mp4","fps":60}`.
The file path must be writable and its directory must exist. Existing files
are overwritten by the engine. FPS must be an integer from 1 through 120.

Play on the live wall clock, then call `game.record.stop`. Its result contains
`frames`, `audio_frames`, `max_drift_ms`, `tail_delta_ms`, and `valid`. Repeated
stop returns the last result; automatic encoder termination is reported as an
invalid take. A duplicate start is rejected. Unavailable or locked audio is
rejected, so a silent device failure cannot masquerade as working sound.

An audio queue overflow, mixing error, encoder failure, or clock drift above
100 ms invalidates and ends the take. At shutdown the audio producer is stopped,
its final samples are drained and the encoder is finalized before audio/graphics
teardown. Audio production uses a bounded SPSC queue; encoding stays on the main
thread. The device clock is aligned to the first presented frame with PCM.

`valid` is a transport/lifecycle result, not a quality verdict. Verify the saved
file's dimensions, decoded frame cadence, audible signal when expected, and an
observable sound/visual event for synchronization. Container FPS alone does not
prove smooth motion. Short differences between audio/video tail durations can
include the device block, output-frame and AAC padding boundaries.

Do not mix raw engine `record.*` commands with an owned `game.record.*` take.
Raw commands do not connect the game's audio mixer. Web recording, manual-time
simulation capture, streaming and desktop capture are outside this module.
Production builds do not compile the capture queue or recorder host source.

## Validation

`tests/test_game_recording.c` exercises lifecycle failures and timing through
fake engine/audio devices and real command parsing. The audio-core native tests
exercise PCM, muting, pause, wraparound and overflow. A real native game take is
also required: unit tests cannot certify encoder/device output or gameplay feel.

## Compatibility

The exact SemVer in `feature.json` follows these rules: PATCH fixes preserve
command/API contracts, MINOR adds compatible capabilities, and MAJOR changes
or removes existing APIs or their meaning. Windows native DevAPI is the only
supported AV platform; release/web builds use disabled inline host calls.

## Extension points

Games own their launch arguments, playable-readiness checks, audio unlock,
fixtures, camera and controls. Studio owns scenario scheduling and saved-media
validation. New backend support must preserve the device-clock, pause and
failure contracts and ship with real encoded-media proof.
