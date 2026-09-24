# Audio Core

## Purpose

`audio-core` is the reusable in-place L1 playback module consumed by templates
and games. It owns handles, pools, buses, mix state, playback policy, and the
private native/web backend seam. The game owns cue/music catalogs,
codec-neutral BLOB IDs, source assets and provenance, pack registration,
persisted settings, platform lifecycle composition, and user-gesture wiring.

## Public surface

The only game-facing header is
`include/features/audio/audio.h`. It exposes lifecycle and status, loading by a
ready `NT_ASSET_BLOB` hash (decoded whole, or streamed), generation-checked
clip/voice handles, playback and stop queries, live per-voice gain and pitch,
MUSIC/SFX buses, mix controls, pause/enable state, and browser
user-gesture unlock. Its fixed limits are 64 clips and 32 voices.
Backend types, file paths, codecs, and JavaScript handles stay private.

## Streamed clips

`audio_clip_load` decodes a clip whole: right for short cues, which then start
with no work. A long track is opened with `audio_clip_stream` instead. It
decodes nothing when it opens; each voice decodes the encoded blob a little at a
time while it plays. The clip holds a copy of the encoded bytes (about 0.5 MB
for a 90 s mono MP3 at 32 kHz) where a decoded one holds the PCM (33 MB for the
same track at 48 kHz stereo). The clip shares the handle pool and every clip
and voice call. A looping voice wraps sample-exact: the loop runs in the decoder
at the source rate, ahead of the resampler, and the decoder trims the encoder
delay and padding that the MP3's Xing/LAME header declares. Pitch does not apply
to a streamed voice. A streamed voice held at gain 0 for 2 s parks: it stops
decoding but still counts as playing, and when its gain rises it goes on from
the sample it paused on. A muted bed therefore costs nothing, and it comes back
where it left off rather than where it would have been.

- Native: each voice is an `ma_sound` over its own `ma_decoder`, so miniaudio
  decodes on the audio thread. The main thread only opens the decoder when the
  voice starts.
- Web: miniaudio's MP3 decoder, built into the wasm with no device, threads
  or WAV support, decodes on the main thread in 2048-frame chunks (64 ms at
  32 kHz). A web stream is MP3 only; a WAV opened with `audio_clip_stream`
  fails there, while `audio_clip_load` still takes WAV through the browser.
  Each chunk becomes an `AudioBuffer` at the track's own rate and channel
  count, and the browser resamples it. Chunks play back to back on the
  track's own timeline (origin + frames / track rate), so rounding never adds
  up, and each carries one guard frame, the next chunk's first, so the
  browser's interpolation across a join reads the real next sample. A stream
  starts, or restarts after a gap, twice the context's output latency ahead
  of the clock and at least 50 ms ahead, and a restart fades in over 8 ms.
  A parked voice keeps its unplayed chunks and schedules them again when it
  resumes.

Web robustness, because a main-thread decoder only runs when the main thread
does:

- Lookahead: 3 s per stream voice, grown ×1.5 after each gap up to 4 s, so a
  3 s main-thread stall plays through. Stop and gain act on the voice's gain
  node, so the lookahead adds no control latency. At 32 kHz mono that is
  384 KB of PCM per playing voice, 512 KB at the cap. Effects are unaffected.
- Fill: once full, a voice decodes at most two chunks per update. After a
  start or a gap, an audible voice fills to its lookahead within 4 ms of
  main-thread time per update for all voices together: in -O3 wasm the
  first update schedules 3 s in 2.6 ms, so a load right after the music
  starts does not starve it.
- A timer every 150 ms feeds the streams when the game loop has not called
  for 200 ms and the context runs, so music continues while frames stop but
  the page stays visible (an offscreen or throttled frame).
- Context recovery: `onstatechange` resumes a context the browser or OS
  suspended or interrupted, unless the game is hidden, paused, disabled or
  never unlocked, and retries every 2 s and on `focus` and `pageshow`. A
  context that closes or fails is replaced: effect voices end, and stream
  voices are scheduled again on the new context from where they were.

Why this web path, measured on a game's music tracks (32 kHz mono MP3,
48 kHz context):

- `decodeAudioData` of the whole track, the path before streaming, decodes off
  the main thread but keeps 16.4 MB of PCM per 90 s track.
- The chosen path, in -O3 wasm with three beds of which one is audible and
  two parked, costs 0.008 ms per 60 Hz frame on average and 0.21 ms at most
  once filled; the first update after a start fills 3 s in 2.6 ms. It keeps
  384 KB of PCM per playing voice. Played back in Chrome, it matches
  Chrome's own resampling of the whole track as one buffer to 1.7e-5 across
  every join and the loop seam, with no drift over a minute. The decoder is
  the one the native path uses.
- `MediaElementAudioSourceNode` over a Blob URL keeps no PCM, but an
  `<audio>` loop is not sample-accurate: it gaps or clicks at the seam, and the
  MP3 delay handling depends on the browser.
- An `AudioWorklet` ring buffer is sample-exact as well, but it needs a worklet
  module (a Blob URL that a portal CSP may refuse) and a message channel. It
  gains nothing over scheduled buffers at this lookahead.
- `decodeAudioData` on MP3 segments keeps the decode off the main thread. But
  each segment is primed and resampled on its own, so the joins click unless
  the frames overlap and are trimmed. That trimming depends on the browser's
  decoder.

## Validation

Run the focused native, resource, web-library, and template-catalog checks in
`INSTALL.md`, then build and test the consumer's real native and Emscripten
targets. Source presence or the version string alone is not compatibility
evidence.

The real-browser proof uses an opt-in Release artifact so ordinary release
exports stay unchanged:

```powershell
cmake ... -DCMAKE_BUILD_TYPE=Release -DGAME_AUDIO_BROWSER_SMOKE=ON
$env:AUDIO_SMOKE_HEADED = "1"
$env:AUDIO_SMOKE_BROWSER_CHANNEL = "chrome"
node features/audio-core/tests/browser_smoke.mjs <artifact-bin>
```

Set `PLAYWRIGHT_MODULE` when Playwright is supplied by an agent runtime rather
than installed in the workspace. The smoke fails on decode errors, missing
gesture unlock, missing SFX/music source transitions, lifecycle failures, or
any console, page, or request error.

## Compatibility

A consumer records the exact version it validated. `feature.json.version` is mandatory SemVer:

- PATCH: compatible fixes, tests, or documentation;
- MINOR: backward-compatible additions to the public contract;
- MAJOR: breaking public API, metadata, or observable behavior changes.

The native adapter is pinned to miniaudio 0.11.25. The web adapter uses
WebAudio. Vendored version, license, source, and integrity hashes are recorded
in `vendor/miniaudio/README.md`.

## Extension points

Add game-specific catalogs, assets, settings, and lifecycle wiring in the game,
not in this module. A backend may be replaced behind `src/audio_backend.h`
without changing the public header. If a game needs incompatible playback
semantics, copy the module into that game and own the fork; do not add a
speculative shared switch.

See `INSTALL.md` for wiring, validation, and removal.

## Version history

- `1.2.0`: `audio_clip_stream` plays long tracks without decoding them whole
  (native: decoded on the audio thread; web: chunks scheduled from a wasm
  MP3 decoder). Native pooled voices now honour `audio_voice_set_pitch`; they
  were created with miniaudio's pitch stage disabled, so the call did nothing.
  Each play starts at pitch 1. Destroying a clip stops its voices.
- `1.1.1`: a phone's first tap unlocks the sound.
- `1.1.0`: live gain and pitch per voice.
