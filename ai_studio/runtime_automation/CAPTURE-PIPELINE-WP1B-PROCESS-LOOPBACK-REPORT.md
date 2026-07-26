# Capture Pipeline WP1b Process-Loopback Report

Status: **audio routing and isolation passed / combined adapter gate failed**

Date: 2026-07-26

Evidence:
[`CAPTURE-PIPELINE-WP1B-PROCESS-LOOPBACK-EVIDENCE.json`](CAPTURE-PIPELINE-WP1B-PROCESS-LOOPBACK-EVIDENCE.json).

## What was implemented

The Studio now has a narrow Windows helper under
`capture/native/windows_process_loopback/`. It uses the documented
`AUDIOCLIENT_ACTIVATION_TYPE_PROCESS_LOOPBACK` contract, binds to one PID plus
its child processes, verifies the owned process's Windows creation time, holds
the process handle for the capture lifetime, and writes finite 48 kHz stereo
PCM16 WAV. It has no WIL, Media Foundation, NuGet, OBS, or engine dependency.

The Python boundary validates finite arguments, caps classic RIFF at six hours,
writes to a unique same-directory staging path, requires a qualified non-empty
WAV, and atomically publishes only validated output. Launch, decode, timeout,
helper-exit, and publication failures remain inside stable recorder error
families. Helper reports include data-discontinuity, timestamp, QPC gap, and
device-position diagnostics.
A separate finite-WAV FFmpeg spike builds a video command, losslessly muxes
audio as FLAC into MKV, and rejects missing streams, wrong dimensions/rate, or
insufficient decoded frames. The game-owned probe triggers real semantic UI
actions; no game behavior is hard-coded in the Studio helper.

All helper, FFmpeg, mux, frame-decode, and FFprobe processes now have bounded
deadlines. Audio and video share cancellation across the complete UI-action
block. The private spike applies a basename filter before `Popen`; this is not
an executable identity allowlist. Windows then adds a kill-on-close Job Object
and POSIX a process group. Timeout, cancellation, and injected
post-launch-exception tests prove bounded cleanup for the measured paths, but
canonical executable identity and atomic create-in-job remain production gates.

The implementation follows Microsoft's API documentation and activation
sequence:

- <https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/>
- <https://learn.microsoft.com/en-us/windows/win32/coreaudio/loopback-recording>
- <https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/ApplicationLoopback>

## Passed evidence

| Check | Result |
|---|---|
| Native build | MSVC 19.40 / Windows SDK 10.0.26100, Release x64 |
| Finite silence contract | 48,000 sample frames over 1.00 s, valid stereo PCM16 WAV |
| Controlled active process | 880 Hz target, 143,520 frames over 2.99 s, peak -21.1 dB |
| Process isolation | concurrent 880 Hz target and 440 Hz foreign process; target component exceeded foreign component by 71.21 dB |
| Real game routing | selected game PID, three mechanically verified Settings-open transitions, and measured game-process audio activity during the scenario |
| Real game format/activity | 235,680 frames of 48 kHz stereo PCM16 over 4.91 s, mean -41.0 dB, peak -7.4 dB |
| Timeline diagnostics | zero data discontinuities, timestamp errors, and QPC gaps in controlled and real-game runs |
| Identity/staging contract | stale creation-time identity rejected; final path is untouched on timeout/failure and promoted only after validation |
| Bounded media lifecycle | basename-filtered private spike, shared A/V cancellation, bounded FFmpeg/FFprobe, Job/process-group cleanup; executable identity/atomic assignment not production-qualified |
| Ownership/cleanup | only spawned helper/fixture/game processes were controlled; no survivors after probes |
| Unit contract | 31 focused tests and all 154 Runtime Automation tests pass |

This proves that a Studio-owned process-tree audio primitive is a feasible lean
alternative to OBS's application-audio component on this Windows host. It also answers the earlier
"does the game have sound?" question with runtime evidence: the selected game
process emitted active audio during the verified UI scenario.

The proof does not yet correlate individual action timestamps to individual
waveform intervals or claim that a specific packaged asset produced each
sample. The game-owned probe now records host-monotonic action offsets so a
later versioned correlation analyzer can establish that stronger claim.

The result does **not** establish endpoint mute independence. The endpoint was
not deliberately muted during the real-game proof. Microsoft's process-loopback
source is endpoint-independent, but the product claim remains unverified until
mute and volume-scalar cases are measured and restored safely.

## Combined A/V follow-up

A finite WAV and FFmpeg video were launched within 0.71 ms at the Python
launcher boundary and muxed into H.264/FLAC MKV. That file was structurally
decodable, but it is **not a master candidate**:

- the current elevated automation context exposed a non-game system surface
  even though the engine-native framebuffer was healthy;
- the attempt decoded only 14 frames versus the 135-frame minimum for
  a five-second 30 fps run;
- its nominal `30/1` average rate and generic pixel-health pass were false
  comfort: inspection showed the captured content was not the game;
- an earlier `gdigrab` direct-HWND attempt produced a black OpenGL surface;
- finite WAV post-mux does not prove a shared real-time clock, offset, drift,
  gap handling, interruption recovery, or long-running streaming.

The stricter gate now rejects this false positive. Presence of one video stream
and one audio stream is not enough.

The follow-up found a concrete adapter regression: the successful benchmark
used `gdigrab` with an exact HWND, while the combined spike had switched to
desktop-region capture and selected a non-game surface. The command builder now:

- passes the owned exact HWND as canonical lowercase hexadecimal with `0x` for
  stable evidence (FFmpeg accepts both decimal and `0x` HWND values);
- never falls back to a desktop region;
- rebases video timestamps at the first captured frame and emits declared CFR;
- validates one frame from the same exact-HWND source before audio/scenario
  start and rejects unhealthy pixels immediately;
- retains post-decode frame-count, rate, and pixel-health gates.

With that fix, the same five-second game/audio scenario produced a `720x1280`,
`30/1` H.264/FLAC file with 145 decoded video frames over 4.91 seconds and
active game audio (`-41.0 dB` mean, `-7.4 dB` peak). The topology and frame-rate
gates passed. The take was still correctly rejected because the current Codex
automation run returned a nearly black exact-HWND OpenGL window surface
(8 sampled colors, 2 buckets, 16.0 luma range, 5.5 luma standard deviation).
The engine-native framebuffer remained healthy. The evidence does not establish
why this source was black; desktop/session state is only a hypothesis.

The installed nightly FFmpeg also exposes the newer Windows Graphics
Capture-backed `gfxcapture` filter. It resolved the exact `720x1280` HWND and
received one D3D11 frame in the elevated probe, then stopped receiving frame
events; in the non-elevated sandbox it failed setup with HRESULT `0x80070424`.
It is therefore recorded as an experimental candidate, not a silent fallback.

The earlier interactive benchmark remains valid evidence that FFmpeg can
capture healthy game video on this host. This follow-up does not overturn it;
it shows that the recorder must preflight the exact source and reject unhealthy
pixels instead of assuming that a resolved HWND is a usable video surface.

## Reproducibility

The evidence JSON records the helper binary/source hashes, hashes of both tone
fixtures, the latest real-game WAV/video/rejected master, pinned FFmpeg/FFprobe
build, sanitized commands, host-monotonic action offsets, the `volumedetect`
command, and the single-bin DFT calculation used for isolation. Private
consumer paths and asset identities are intentionally absent.

## Decision

- Accept the native process-loopback helper as a successful **feasibility
  primitive**, not as a production backend.
- Keep FFmpeg as the leading lean video/remux candidate.
- Do not select a combined FFmpeg backend and do not enable `auto`.
- Keep `audio=game` unavailable in the production CLI until the complete
  combined adapter passes.
- Keep OBS as an optional manual/compatibility comparison, not the default.

## Next gate

1. Repeat the corrected exact-HWND command under the conditions of the earlier
   healthy interactive benchmark and diagnose the current black surface;
   continue to require a healthy preflight frame before scenario time starts.
2. Replace finite WAV post-mux with a bounded streaming transport carrying a
   declared audio clock/timestamp contract.
3. Run the controlled flash/impulse fixture and measure initial offset, drift,
   gaps, and stop behavior.
4. Repeat graceful stop, forced interruption, restart, cleanup, mute/volume,
   occlusion/minimize, and 30-second real-game performance tests.
5. Select a backend only after the complete exit gate passes.
