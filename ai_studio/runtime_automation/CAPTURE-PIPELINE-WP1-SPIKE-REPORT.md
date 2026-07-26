# Capture Pipeline WP1 Spike Report

Status: **measurements complete / cleanup verified / WP1 gate failed**

Measured on 2026-07-26 on the current Windows host and interactive desktop.
This report separates tool presence, RPC acceptance, stream topology, decoded
content, and recovery. A pass in an earlier layer is not treated as evidence for
a later one.

The redacted machine-readable evidence index is
[`CAPTURE-PIPELINE-WP1-EVIDENCE.json`](CAPTURE-PIPELINE-WP1-EVIDENCE.json).
Private-consumer paths, identity, scene ids, and action ids remain only in
ignored local evidence.

Operational cleanup is recorded separately in
[`CAPTURE-PIPELINE-WP1-CLEANUP-INCIDENT.md`](CAPTURE-PIPELINE-WP1-CLEANUP-INCIDENT.md).
The administrator cleanup helper later measured zero remaining rules.

## Test target

An anonymized private-consumer native DevAPI build was launched from its game
root with fresh state, autosave disabled, clean capture presentation, and a
requested `640x360` client. Its controlled window was uniquely resolved as a
visible top-level `GLFW30` HWND in the same Windows session and at the same
medium integrity level as OBS.

The game-owned deterministic capture-scene endpoint family was discovered
before use. One versioned presentation scene was loaded with a fixed seed and a
production-owned action was triggered during the OBS take. Production
settings-open edges were also exercised to request a known UI audio cue.

## Tool discovery

| Component | Measured result |
| --- | --- |
| OBS Studio | `30.1.2`, copied into a temporary portable tree |
| OBS WebSocket | `5.4.2`, RPC version 1, authenticated |
| OBS request discovery | 144 requests; recording, source, screenshot, stats, video-settings, and record-directory requests present |
| OBS input discovery | `game_capture`, `window_capture`, `monitor_capture`, `wasapi_process_output_capture`, and device sources present |
| FFmpeg | `N-122685-g9bfa1635ae-20260209` |
| FFmpeg input devices | `gdigrab`, `dshow`, `openal`, `vfwcap`, `lavfi` |

No mutation of the user's installed OBS profile or scene collection was
observed. WebSocket authentication was required. This is not a managed
isolation pass: the spike used a fixed port, implemented no global lock or Job
Object, and OBS WebSocket 5.4.2 bound to all IPv4 interfaces even with
`--websocket_ipv4_only`. An exact-program inbound firewall block reduced the
exposure during the attempt, but its administrator-level cleanup could not be
verified from the final automation context.

## FFmpeg results

### Exact HWND, graceful stop: pass for video only

`gdigrab` captured the exact HWND at 30 fps and FFmpeg was stopped through its
stdin `q` protocol.

| Check | Measured result |
| --- | --- |
| Output | ignored private-consumer staging artifact; hash in the redacted evidence index |
| Container/codec | Matroska / H.264 |
| Geometry | `640x360` |
| Rate | `30/1` average frame rate |
| Frames | 67 |
| Duration | 2.266 s |
| Size | 19,107 bytes |
| Full decode | pass |
| Extracted-frame health | pass: 2,899 unique colors, 58 buckets, luma range 128.3, stdev 21.0 |

This proves exact-window video capture and graceful finalization on this host.
It does not prove minimize behavior, occlusion independence, or audio.

### Desktop region: fail

The exact desktop-region attempt failed with Win32 error `5` ("access denied").
The non-DPI-aware geometry caller also measured a `426x240` logical client for
the requested `640x360` window at 150% display scale. A region adapter would
therefore need both a DPI-aware geometry boundary and an explicit
desktop-capture privacy/privilege preflight. It is not a safe fallback here.

### Forced interruption: fail

Terminating FFmpeg instead of using its graceful stdin protocol left a
584-byte Matroska header. `ffprobe` reported end-of-file and found no recoverable
streams. The adapter cannot claim crash-safe takes from the container choice
alone.

### Audio capability: unavailable

DirectShow enumeration found two video devices and no audio-only device.
Device display names are host-local and omitted from the committed report.
`gdigrab` supplies video, not application audio. Therefore the measured FFmpeg
adapter must reject `audio=game`; it must not infer that policy from `dshow`
presence.

## OBS results

### Control plane: pass

The isolated OBS instance authenticated successfully and reported the required
recording/source/stats requests. Runtime video settings were changed and read
back as `640x360`, 30/1 fps. Global desktop and microphone sources were muted
before the take so they could not create a false `audio=game` pass.

OBS accepted source creation, source settings, screenshots, record start/stop,
and returned output paths. A graceful take produced:

- Matroska with H.264 video and AAC LC audio;
- `640x360`, 30 fps, 8.133 s;
- one stereo 48 kHz audio stream;
- full-file decode success;
- zero OBS-reported render and output skipped frames during the measured take.

These facts prove the control plane and container topology only.

### Exact-window pixels: fail

OBS WebSocket reported the source as `videoActive=true` and
`videoShowing=true`, but both the source screenshot and extracted recording
frame were uniform black. The first attempt also exposed a spike bug:
`mode=capture_specific_window` is not OBS's Game Capture setting contract.
After correcting it to OBS 30.1.2's `capture_mode=window`, the source still did
not hook the game window.

The OBS source property list omitted the game even though independent Win32
enumeration found one visible, enabled, ownerless `GLFW30` top-level window and
FFmpeg captured the same HWND. Title, class, executable, priority modes, launch
order, Windows session, and integrity level were checked. No production-safe
exact-window OBS binding was demonstrated.

An RPC active/showing flag is therefore insufficient. The adapter requires a
decoded pixel-health gate before recording can become `recording`.

### Application audio: fail

OBS parsed the requested window descriptor into the expected title, class, and
executable, but its process-loopback source logged:

`[VIRTUAL_AUDIO_DEVICE_PROCESS_LOOPBACK] Failed to find window`

The AAC stream contained 772,096 decoded samples in one measured take, but
`volumedetect` reported both mean and maximum at `-91.0 dB`: encoded digital
silence. Repeated production UI-cue requests did not change this result because
the application-audio source never initialized.

An AAC stream at 48 kHz is therefore not an `audio=game` pass. The validator
must require measured activity when the scenario declares audio activity.

### Lifecycle readiness: fail

On isolated OBS restarts, the WebSocket socket could accept authentication
before OBS was ready: `GetVersion` returned request status 207. A later run
accepted `StartRecord`, but `GetRecordStatus.outputActive` never became true.

The managed adapter needs a bounded readiness state machine:

1. socket reachable;
2. authenticated/identified;
3. `GetVersion` and required-request discovery succeed;
4. frontend/profile/scene ready;
5. `StartRecord` accepted;
6. `outputActive=true` observed before the scenario starts.

Any timeout must fail the take and preserve diagnostics.

### Window Capture and performance follow-up: capability pass, lifecycle fail

The bounded retry used OBS `window_capture` with its integrated
`capture_audio=true` process-loopback path, first against the controlled WP0
audiovisual fixture and then against the anonymized GLFW consumer. WebSocket
was disabled for these CLI-driven runs.

Both mid-take videos decoded with healthy pixels at `640x360`, 30 fps. The
controlled tone measured `-37.9 dB` mean / `-13.3 dB` maximum. Repeated real
game UI cues measured `-51.8 dB` mean / `-7.4 dB` maximum. This supersedes the
earlier Game Capture/process-source capability failure: OBS can capture this
consumer's healthy window video and active per-process audio through Window
Capture.

The retry still failed the production lifecycle gate:

- initial black preroll existed before Windows Graphics Capture attached;
- on the real-game run, OBS 30.1.2 exited with `0xC0000005` while a WASAPI
  callback raced shutdown;
- the crash crossed `audio_output_get_planes`, `obs_source_output_audio`, and
  `WASAPISource::ProcessCaptureData`;
- the resulting MKV remained decodable, but the adapter exit was abnormal.

The measured performance comparison is recorded in
[`CAPTURE-PIPELINE-RECORDER-BENCHMARK.md`](CAPTURE-PIPELINE-RECORDER-BENCHMARK.md).
On the light native scene, mean whole-GPU utilization was approximately 0.5%
with FFmpeg and 12.9% with OBS. Game p95 frame time was 7.78 ms with FFmpeg and
9.33 ms with OBS; measured 1%-low FPS was 114.7 and 82.3 respectively. Both
remained inside a 16.67 ms budget, but OBS consumed materially more headroom.

## Capability matrix

| Capability | OBS 30.1.2 | FFmpeg build |
| --- | --- | --- |
| Tool/version discovery | pass | pass |
| Isolated managed configuration | fail: incomplete | not applicable |
| Exact HWND/window video with healthy pixels | pass via Window Capture; Game Capture failed | pass |
| `640x360` / 30 fps contract | topology pass | pass |
| Graceful finalized MKV | partial: controlled run passed; game shutdown crashed after a valid file | pass |
| Recoverable forced interruption | not demonstrated | fail |
| Application-only audio | pass via Window Capture | unavailable |
| Audio activity validation | pass via controlled tone and real game cue | unavailable |
| Dropped-frame diagnostics | pass | not implemented |
| Minimize/occlusion contract | not demonstrated | not demonstrated |
| Full WP1 quality gate | **fail** | **fail** |

## Decision

- OBS production adapter: **not selected**; Window Capture is a proven
  audiovisual compatibility path, but 30.1.2 failed performance/lifecycle
  gates.
- FFmpeg production adapter: **not selected**; it is the leading measured
  video-only candidate but cannot satisfy `audio=game` alone.
- Automatic fallback or backend selection: **must remain disabled**.
- `audio=game`: the production CLI **must still refuse** until one combined
  adapter passes the full gate, even though experimental OBS Window Capture
  proved the host capability and the later Studio process-loopback primitive
  proved game-audio routing/isolation.
- The existing DevAPI frame prototype remains experimental/deprecated because
  of its known paired capture/step wrong-tick defect. Only separately verified
  silent-frame operations may be reused; it is not general acceptance evidence
  and does not satisfy live application-audio recording.

WP2/WP3 backend implementation remains blocked by the measured gate. WP1b has
now proved finite PCM process-tree routing, controlled isolation, and real-game
activity; its hardened finite helper also verifies process creation-time
identity, reports continuity, and atomically publishes validated WAV. The
initial combined MKV was rejected for under-frame video and a non-game system
surface. The follow-up restored the benchmark's exact-HWND input in place of
desktop-region capture and corrected CFR timestamps; it now passes video/audio
topology, but the current automation run returns nearly black exact-HWND pixels
for an unverified reason, so the pixel gate rejects it.
See
[`CAPTURE-PIPELINE-WP1B-PROCESS-LOOPBACK-REPORT.md`](CAPTURE-PIPELINE-WP1B-PROCESS-LOOPBACK-REPORT.md).
The next bounded order is:

1. run the corrected exact-HWND path on the unlocked interactive desktop and
   require a healthy preflight frame before scenario time starts;
2. add timestamped streaming PCM and require controlled flash/impulse
   synchronization, drift, gap, and real-game activity validation;
3. repeat mute/volume, graceful stop, forced interruption, restart, ownership,
   and cleanup gates on the combined path;
4. optionally re-spike OBS using a pinned current stable build—OBS 32.0.3
   changed shutdown handling—without assuming that the 30.1.2 crash persists or
   disappeared;
5. select a production adapter only after one full gate passes.

No retry may weaken pixel, activity, isolation, readiness, recovery, or
performance validators.

## Primary references

- FFmpeg device documentation:
  <https://ffmpeg.org/ffmpeg-devices.html>
- OBS portable mode:
  <https://obsproject.com/kb/portable-mode>
- OBS Studio 30.1.2 Game Capture source contract:
  <https://github.com/obsproject/obs-studio/blob/30.1.2/plugins/win-capture/game-capture.c>
- Microsoft ApplicationLoopback sample:
  <https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/>
