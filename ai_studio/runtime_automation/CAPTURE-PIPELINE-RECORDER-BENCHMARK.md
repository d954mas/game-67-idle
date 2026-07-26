# FFmpeg and OBS Recorder Benchmark

Status: measured decision record; no production backend selected.

Measured on 2026-07-26 on the current Windows host. Private game identity,
window handles, local paths, and action ids are omitted. Raw artifacts remain
ignored and local.

This benchmark follows
[`CAPTURE-PIPELINE-WP1-SPIKE-REPORT.md`](CAPTURE-PIPELINE-WP1-SPIKE-REPORT.md).
It answers two narrower questions:

1. can external recording capture real game audio on the current low-volume
   endpoint; and
2. how much headroom do FFmpeg and OBS consume on the same local workload?

## Method

Two workloads were run:

- a controlled `640x360`, 30 fps audiovisual fixture with a changing frame
  counter, 48 kHz tone, and paired flash/impulse markers;
- an anonymized native game at `640x360`, with its real settings-open SFX
  triggered repeatedly through the game-owned DevAPI.

Each recorder used NVIDIA H.264 NVENC preset `p5`. The game comparison used
CQP 17 for both recorders. Each steady-state sample lasted about 12 seconds.
The game recorded at 30 fps while its normal loop remained uncapped. Built-in
`perf.stats` supplied game frame-time percentiles; `nvidia-smi` supplied coarse
whole-GPU utilization; Win32 process counters supplied CPU and working-set
measurements.

These are short local A/B samples, not universal hardware benchmarks. The game
scene is light, whole-GPU utilization is coarsely sampled, and run-to-run
variance is possible. The useful result is the relative headroom and the
capability/lifecycle difference on this host.

## Controlled audiovisual fixture

| Measurement | No recorder | FFmpeg `gdigrab` | OBS Window Capture |
| --- | ---: | ---: | ---: |
| Mean whole-GPU utilization | 0.45% | 0.50% | 9.68% |
| Mean NVENC utilization | 0% | 0.91% | 2.00% |
| Recorder CPU, share of total machine | — | 0.14% | 0.39% |
| Recorder peak working set | — | 163 MiB | 169 MiB |
| Healthy decoded pixels | — | pass | pass after startup preroll |
| Application audio activity | — | unavailable | pass |
| Recorder-reported lag | — | not exposed | 0.2% render, 0.2% encode |

The OBS output contained H.264 plus stereo AAC at 48 kHz. Audio measured
`-37.9 dB` mean and `-13.3 dB` maximum, so the track was active rather than an
encoded silent stream.

## Native game with real SFX

| Measurement | No recorder | FFmpeg `gdigrab` | OBS Window Capture |
| --- | ---: | ---: | ---: |
| Game frame-time p50 | 4.17 ms | 4.23 ms | 4.94 ms |
| Game frame-time p95 | 4.68 ms | 7.78 ms | 9.33 ms |
| Game frame-time p99 | 7.30 ms | 8.71 ms | 12.16 ms |
| Game 1%-low FPS | 137.1 | 114.7 | 82.3 |
| Frames over 16.67 ms | 0% | 0% | 0% |
| Mean whole-GPU utilization | 0% | 0.50% | 12.91% |
| Mean NVENC utilization | 0% | 0.91% | 1.91% |
| Healthy decoded pixels | — | pass | pass after startup preroll |
| Recorded real game SFX | — | no audio input | pass |
| Stop/finalize | — | clean exit, valid MKV | valid MKV, process crashed during shutdown |

The OBS AAC track measured `-51.8 dB` mean and `-7.4 dB` maximum. This is the
short real game cue, not the controlled tone. It proves that Window Capture's
process-loopback path recorded the game's audio while the current endpoint was
not muted and set to 18%. The benchmark did not toggle endpoint mute or volume,
so it does not prove volume/mute independence. Microsoft's process-loopback
contract is not tied to one physical endpoint, but the production spike must
still measure and declare where endpoint and application volume scalars affect
captured PCM.

Both recorded videos decoded and showed healthy mid-take game frames. OBS
recorded initial black preroll before Windows Graphics Capture attached, which
must be trimmed or excluded by a readiness gate.

OBS 30.1.2 then exited with `0xC0000005` while its WASAPI callback raced
application shutdown. The crash stack crossed
`audio_output_get_planes -> obs_source_output_audio ->
WASAPISource::ProcessCaptureData`. The MKV remained decodable, but a valid file
does not turn an abnormal adapter exit into a lifecycle pass.

## Interpretation

The user's observation that OBS can make a game feel less smooth is credible.
On this light scene OBS roughly doubled p95 frame time from the no-recorder
baseline and reduced measured 1%-low FPS by about 40%. FFmpeg also consumed
headroom, but less: p95 rose by about 66% and 1%-low fell by about 16%. Neither
recorder pushed this particular scene below a 60 fps budget, so the test
demonstrates lost headroom rather than visible stutter on demand. A heavier
scene can turn that lost headroom into actual missed frames.

NVENC alone does not explain the difference. Both paths used NVENC. OBS also
runs a D3D11 compositor, Window Capture, audio processing, scene machinery,
and output lifecycle. FFmpeg's measured path captured one HWND and encoded it
directly.

## Backend decision

- FFmpeg is the leading measured candidate for lean exact-window **video-only**
  recording and remains the common probe/remux/export tool.
- The installed FFmpeg exposes `gdigrab` and DirectShow but no WASAPI or
  per-process audio input. It cannot truthfully satisfy `audio=game` alone.
- OBS Window Capture proves that external per-process game audio is feasible,
  but OBS 30.1.2 is not accepted as the production adapter because of measured
  GPU/headroom cost, startup preroll, isolation gaps, and the shutdown crash.
- OBS remains a useful manual/compatibility backend. If it is reconsidered for
  automation, re-spike the pinned current stable line rather than carrying
  30.1.2 forward. OBS 32.0.3 specifically changed shutdown handling, so the old
  crash result must not be projected onto a newer build without measurement.
- The follow-up Windows application-loopback helper now proves finite 48 kHz
  PCM from the owned game process tree, including 71.21 dB rejection of a
  concurrent foreign tone and real-game activity during three verified UI
  transitions. Microsoft's official
  `ApplicationLoopback` sample demonstrates the required
  `ActivateAudioInterfaceAsync` process-tree capture on Windows 10 build 20348
  or later. The primitive solves application-only routing without the OBS
  compositor. The initial finite-WAV/FFmpeg combination captured only 14 of the
  required 135 frames and exposed a non-game desktop surface. A follow-up found
  that the combined adapter had regressed from the benchmark's exact-HWND input
  to desktop-region capture. The corrected path serializes the exact HWND
  canonically as `0x` for auditability (FFmpeg also accepts decimal), and after
  CFR normalization it produced 145 frames at `30/1` plus active game audio.
  The current automation run still returned nearly black exact-HWND pixels for
  an unverified reason, and the pixel gate rejected the take. Streaming
  synchronization is still untested. See
  [`CAPTURE-PIPELINE-WP1B-PROCESS-LOOPBACK-REPORT.md`](CAPTURE-PIPELINE-WP1B-PROCESS-LOOPBACK-REPORT.md).
- No automatic fallback may weaken a requested audio policy. Until a combined
  path passes synchronization, graceful stop, forced interruption, restart,
  and cleanup gates, `audio=game` remains unavailable in the production CLI.

## Primary references

- [FFmpeg capture-device documentation](https://ffmpeg.org/ffmpeg-devices.html)
- [OBS application-audio capture guide](https://obsproject.com/kb/application-audio-capture-guide)
- [OBS encoding performance troubleshooting](https://obsproject.com/kb/encoding-performance-troubleshooting)
- [OBS hardware encoding](https://obsproject.com/kb/hardware-encoding)
- [Microsoft application-loopback sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/)
- [OBS Studio releases](https://github.com/obsproject/obs-studio/releases)
