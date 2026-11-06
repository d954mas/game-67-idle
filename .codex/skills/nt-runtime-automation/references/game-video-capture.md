# Game Video Capture

Load this when recording game video or changing the capture workflow.

For routine game video, start in the game root and use:

```text
capture live
capture shot <id>
```

Read the game-owned `capture/catalog.json` before choosing an id. The shared
`capture_game.py` route uses `game.record.start` and `game.record.stop` from
`features/game-recording`, combining engine framebuffer capture with audio-core's
actual post-mix stereo samples. Use legacy `record_game.py` only for explicitly
requested desktop/OBS work; it is not a fallback for missing native integration.

Use Windows native DevAPI builds and the approved host execution path. In Codex,
launch the game-owned capture command with `sandbox_permissions: "require_escalated"`
when the managed command sandbox cannot access the interactive game's window,
audio device or build metadata. Production and web builds exclude this native
integration. Discover the live command contract first, require a nonzero
framebuffer, and unlock audio through ordinary player input before recording.

`live` records continuous play. `shot` runs the catalog's authored scenario on
the live wall clock; setup frame waits are host-paced. Do not claim deterministic
or frame-exact simulation from a realtime recording. Do not substitute raw
engine `record.*` commands: those do not attach the game's audio mixer.

Treat each take as a draft. Check the recorder receipt and fully decode the
saved media: codec, dimensions, cadence, audio stream and sample count. Inspect
distinct frames and audio level; static or muted takes need an appropriate
scenario expectation. FPS metadata alone does not prove smooth motion. Check
an observable audiovisual event when proving sync. After capture-workflow
changes, exercise mute, pause, restart and shutdown.

A shot becomes a true master only when recording validation, scenario completion,
and the applicable universal safe-area gate all pass. An incomplete safe-area
policy remains guidance and keeps the take in `draft/`.
