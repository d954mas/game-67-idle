# Game Capture Pipeline Research and Comparative Analysis

Status: source-driven research record for the approved V1 design.

Reviewed: 2026-07-26.

Normative proposal:
[`CAPTURE-PIPELINE-SPEC.md`](CAPTURE-PIPELINE-SPEC.md). Delivery sequence:
[`CAPTURE-PIPELINE-IMPLEMENTATION-PLAN.md`](CAPTURE-PIPELINE-IMPLEMENTATION-PLAN.md).

## 1. Executive conclusion

No reviewed product combines all of these in one contract:

- one-command manual gameplay recording with application audio;
- game-owned authored and seeded presentation scenes;
- aspect-specific framing from one shot;
- immutable masters, separate delivery exports, and provenance;
- optional exact fixed-step frame rendering and semantic/pixel comparison.

The accepted industry pattern is not one universal capture mechanism. It is a
shared workflow over specialized lanes:

1. real-time external capture for authentic gameplay and synchronized sound;
2. authored timeline/queue orchestration for repeatable cinematics;
3. optional offline frame/image rendering when exact sampling or maximum quality
   matters;
4. delivery encoding/remux as a step after the durable master.

The Studio design adopts that split behind one CLI and one job/artifact model.
The first local A/B measurement did not select a production Windows backend:
FFmpeg was materially leaner for exact-window video but exposed no
per-application audio input, while OBS Window Capture recorded healthy video
and real game audio at a substantially higher GPU/headroom cost and crashed
during one automated shutdown. See
[`CAPTURE-PIPELINE-RECORDER-BENCHMARK.md`](CAPTURE-PIPELINE-RECORDER-BENCHMARK.md).
The next primary candidate is FFmpeg video combined with a small Windows
process-loopback PCM helper; OBS remains a separately gated
manual/compatibility candidate. The deterministic PNG lane remains optional and
cannot make ordinary recording silent or block V1.

## 2. Products and accepted approaches

### 2.1 OBS Studio

Official evidence:

- [Game Capture Source](https://obsproject.com/kb/game-capture-source) calls
  Game Capture the most efficient Windows path for DirectX/OpenGL games and
  supports selecting a specific game window.
- [Application Audio Capture Guide](https://obsproject.com/kb/application-audio-capture-guide)
  documents per-application audio on Windows and, from OBS 30.1, audio included
  directly with Window/Game Capture.
- [Standard Recording Output Guide](https://obsproject.com/kb/standard-recording-output-guide)
  recommends MKV for recovery after an ungraceful stop, supports selected audio
  tracks, and documents later remux to MP4.
- [Multiple Audio Track Recording Guide](https://obsproject.com/kb/multiple-audio-track-recording-guide)
  recommends a compatibility mix plus separated tracks for editing.
- [obs-websocket](https://github.com/obsproject/obs-websocket) is bundled with
  OBS 28+, uses authenticated WebSocket control, and documents command-line
  port/password overrides.
- [OBS Portable Mode](https://obsproject.com/kb/portable-mode) requires a
  separate portable application tree, so “portable” is not interchangeable
  with pointing a normal installed OBS at an arbitrary game-local directory.

What OBS establishes:

- application/game audio capture is an external-tool problem already solved on
  current Windows;
- crash-resilient recording followed by remux is normal;
- game/system/microphone sources and multi-track masters are normal;
- a supported automation seam exists without a game or engine video API.

What OBS does not provide for Studio:

- game-owned semantic controls/actions or seeded stage contracts;
- shot validation against the live game;
- normalized jobs/takes and source/build provenance;
- deterministic input, semantic, or pixel claims;
- safe isolation automatically—Studio must prove that it does not overwrite a
  user's unmanaged OBS profile or scene collection.

Decision:

- OBS Window Capture is a proven capability path for `audio=game`, not the
  selected production adapter;
- any automated OBS path must use a pinned current stable build, authenticated
  local control, and pass the measured isolation/lifecycle/performance gates;
- retain OBS as a manual/compatibility candidate while the lean combined
  FFmpeg/process-loopback path is spiked.

### 2.2 FFmpeg

Official evidence:

- [FFmpeg Devices Documentation](https://ffmpeg.org/ffmpeg-devices.html)
  documents `gdigrab` capture of the desktop, a fixed region, a window title, or
  an HWND.
- The same documentation says DirectShow supports audio/video devices, can list
  them with `-list_devices true`, and may open audio and video together to
  improve synchronization.
- Current FFmpeg source exposes a Windows Graphics Capture-backed
  [`gfxcapture` source](https://ffmpeg.org/doxygen/trunk/vsrc__gfxcapture_8c_source.html)
  with exact HWND selection. Microsoft's
  [`CreateForWindow`](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow)
  contract targets one HWND, while its
  [screen-capture guidance](https://learn.microsoft.com/en-us/windows/apps/develop/media-authoring-processing/screen-capture)
  identifies each captured frame's `SystemRelativeTime` as QPC time suitable
  for media synchronization.

What FFmpeg establishes:

- direct CLI-driven window/region capture and named device capture are stable
  low-level primitives;
- source dimensions, frame rate, devices, codec, container, and mapping can be
  made explicit and auditable;
- post-processing, probing, remuxing, and delivery encoding fit one toolchain.

The detected 2026 nightly build does expose `gfxcapture`, but feature discovery
is not an acceptance pass. On this host the elevated probe resolved the exact
window and received one D3D11 frame before stalling, while the non-elevated
sandbox failed capture setup with HRESULT `0x80070424`. It remains a valuable
future replacement for occlusion-sensitive GDI, but is not the selected V1
primitive without a pinned build and the full lifecycle/content gate.

Critical Windows limitation:

`gdigrab` is video capture and `dshow` is device capture. Their presence does
not prove that a given FFmpeg build can isolate one game's application audio or
capture system loopback. That requires a concrete discovered loopback/device or
another adapter. A recorder that sees `dshow` and silently assumes
`audio=game` would be lying.

Decision:

- keep FFmpeg as the required HWND/region plus explicit-device adapter and the
  common probe/remux/export tool;
- serialize `gdigrab` HWND values canonically as `0x` for stable evidence
  (FFmpeg also accepts decimal), preflight the exact source, reject unhealthy
  pixels, and never fall back to the whole desktop;
- capability-gate application/system audio;
- never downgrade a requested audio policy.

### 2.2.1 Windows application-loopback helper

Official evidence:

- [Microsoft's ApplicationLoopback sample](https://learn.microsoft.com/en-us/samples/microsoft/windows-classic-samples/applicationloopbackaudio-sample/)
  uses `ActivateAudioInterfaceAsync` to capture only a specified process and its
  child processes on Windows 10 build 20348 or later.

What it establishes:

- process-only audio is a Windows platform primitive, not an OBS-only feature;
- the capture is independent of a particular physical output endpoint;
- silence is an honest result when the target process has no active render
  stream.

Decision:

- spike a repository-owned, narrowly scoped helper that emits timestamped PCM
  for the owned game process tree;
- feed that audio into the same FFmpeg master take as exact-HWND video;
- require content-sync, process ownership, graceful stop, forced interruption,
  and no-audio activity validation before selecting it.

### 2.3 Xbox Game Bar

Official evidence:

- [Microsoft's Xbox Game Bar recording guide](https://support.microsoft.com/en-US/accessibility/windows/use-a-screen-reader-to-record-your-screen-with-xbox-game-bar)
  documents recording a game/app with `Win+Alt+R`, including game, app, system,
  and optional microphone audio, saved as MP4.

What Game Bar establishes:

- the expected daily UX is extremely small: enable once, start/stop hotkey,
  optional microphone, predictable capture folder;
- users expect sound in ordinary gameplay clips by default.

Why it is a competitor/reference rather than the Studio backend:

- the documented workflow exposes no supported job/provenance automation seam;
- it records directly to a delivery-oriented MP4;
- it does not drive authored game stages, target framing, queues, or validation.

Decision:

- copy the simplicity—defaults, countdown/visible state, stop hotkey, one output
  path—not the opaque artifact lifecycle.

### 2.4 Unity Recorder

Official evidence:

- [Unity Recorder 5.1](https://docs.unity.cn/Packages/com.unity.recorder%405.1/manual/index.html)
  captures gameplay/cinematics as video, GIF, image sequences, audio as separate
  WAV, accumulated subframes, and command-line batches; it is Editor Play-mode
  tooling, not a standalone-player recorder.
- [Unity Recorder Timeline workflow](https://docs.unity.cn/Packages/com.unity.recorder%404.0/manual/RecordingTimelineTrack.html)
  uses Recorder Tracks and Clips to trigger independent recordings over
  timeline intervals and uses multiple tracks for simultaneous data types.
- [Unity Audio Recorder](https://docs.unity.cn/Packages/com.unity.recorder%404.0/manual/RecorderAudio.html)
  records mono/stereo WAV and routes the audio signal to the Recorder rather
  than the normal output during that recording.

What Unity establishes:

- recording source, time interval, output properties, and format are separate;
- a manual recording session and authored timeline-triggered recording can use
  the same recorder concepts;
- multiple outputs/recorders and command-line job batches are normal.

Difference from the Studio proposal:

- Unity owns the engine/editor/audio path; Studio intentionally keeps ordinary
  video and audio outside Neotolis;
- Studio requires external real-time recording from a production-like base
  recording build and a separate authored build for game-owned stage contracts;
- Studio preserves a recoverable audiovisual master and provenance rather than
  treating every output as one Recorder file.

Decision:

- keep shot choreography independent from target/profile/export;
- let the same backend kernel serve manual and authored sessions;
- do not copy the Editor-only or engine-audio dependency.

### 2.5 Unreal Movie Render Queue

Official evidence:

- [Movie Render Pipeline](https://dev.epicgames.com/documentation/unreal-engine/movie-render-pipeline-in-unreal-engine)
  defines Movie Render Queue as a batch system of jobs plus per-job settings and
  reusable queue/config assets.
- [Render Settings and Formats](https://dev.epicgames.com/documentation/en-us/unreal-engine/cinematic-render-settings-and-formats-in-unreal-engine)
  separates reusable Master presets from per-shot overrides and supports a
  third-party command-line encoder such as FFmpeg.
- [Command-line MRQ rendering](https://dev.epicgames.com/documentation/unreal-engine/using-command-line-rendering-with-move-render-queue-in-unreal-engine)
  supports a sequence plus config, a saved queue, or a custom Python executor.

What Unreal establishes:

- sequence/shot, render configuration, job, queue, and export are separate
  production concepts;
- master settings plus narrow shot overrides scale better than duplicating
  complete configurations;
- local/separate-process execution and programmable queues are normal.

Difference from the Studio proposal:

- MRQ is primarily a high-quality cinematic renderer, not the simplest path to
  record authentic player input with application audio;
- Studio's real-time lane remains first-class and its public workflow hides
  internal jobs unless batch control is needed.

Decision:

- retain the internal shot → target/profile → job → take model;
- expose the simpler shot → target → take → export workflow;
- keep queues as generated data rather than Python scripts.

### 2.6 Godot Movie Maker

Official evidence:

- [Godot: Creating movies](https://docs.godotengine.org/en/stable/tutorials/animation/creating_movies.html)
  documents fixed-FPS movie writing that can run slower than real time, PNG
  image sequence plus WAV, AVI, command-line launch, and extensible
  `MovieWriter` implementations.

What Godot establishes:

- exact output cadence does not require real-time execution;
- lossless image/audio sources can be kept before later encoding;
- a dedicated offline writer is distinct from ordinary screen recording.

Difference from the Studio proposal:

- Neotolis does not currently own an equivalent movie/audio writer;
- exact render-target and presented-frame contracts have proven gaps;
- V1 sound is solved immediately through external real-time capture instead of
  waiting for deterministic offline PCM.

Decision:

- keep `offline_frames` as a separately gated optional backend;
- never make it the hidden prerequisite for manual or scripted audiovisual
  recording.

## 3. Capability comparison

Legend: **Yes** means first-class in the reviewed official workflow; **Partial**
means possible with restrictions or extra setup; **No** means not a documented
primary capability.

| Capability | OBS | Xbox Game Bar | FFmpeg CLI | Unity Recorder | Unreal MRQ | Godot Movie Maker | Studio V1 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| One-step manual gameplay clip | Yes | Yes | Partial | Partial | No | No | Yes |
| Selected game/application audio | Yes, Windows | Yes | Partial, device-dependent | Engine/editor audio | Not primary | Offline WAV | Yes via capable adapter |
| Named microphone / separate tracks | Yes | Partial | Yes, device-dependent | Separate WAV | Config-dependent | Offline WAV | Yes |
| Authored timeline/shot | OBS scenes only | No | No | Yes | Yes | Game-owned | Yes, game-owned |
| Seeded semantic game controls | No | No | No | Game-specific | Game-specific | Game-specific | Yes, explicit contract |
| Portrait/landscape framing variants | Canvas/manual | No | Filters/manual | Configurable | Shot presets | Viewport/game | Yes, target-resolved |
| Queue/presets | Partial | No | Shell scripts | CLI/session | Yes | CLI | Yes |
| Crash-resilient master then remux | Yes | Not documented | Yes if configured | Output-dependent | Output-dependent | Image/WAV source | Yes |
| Programmatic supported control | WebSocket | No documented API | CLI | Scripting/CLI | Python/CLI | CLI/writer API | CLI + adapters |
| Exact slower-than-real-time sampling | No | No | Input-dependent | Yes | Yes | Yes | Optional backend |
| Semantic/pixel reproducibility claims | No | No | No | No generic claim | No generic claim | No generic claim | Scoped and explicit |
| Immutable master plus derived exports | Manual convention | No | Manual convention | No common take model | Pipeline convention | Manual convention | Normative |

## 4. Platform delivery and safe-area policy

Capture targets and delivery presets are deliberately separate.

Official example:

- [Steamworks trailer requirements](https://partner.steamgames.com/doc/store/trailer)
  prefer 16:9 up to 1920x1080, 30/29.97 or 60/59.94 FPS, high bitrate,
  H.264/AAC, and common 44.1/48 kHz audio. Steam also generates derived posters,
  thumbnails, and microtrailers.

Official vertical-social guidance:

- [TikTok Auction In-Feed Ads](https://ads.tiktok.com/help/article/tiktok-auction-in-feed-ads?lang=en-GB)
  recommends 9:16 and says the safe zone changes with video dimensions, caption
  length, additional formats, and LTR/RTL layout; it publishes separate
  downloadable safe-zone files and warns that preview and live UI can differ by
  device;
- [YouTube Shorts ad asset guidance](https://support.google.com/google-ads/answer/16041697?hl=en-GB)
  recommends vertical 9:16 assets, while Google's
  [universal video ad safe-zone guidance](https://support.google.com/google-ads/answer/13547298?hl=en)
  publishes a vertical 1080x1920 safe-zone visual and downloadable transparent
  templates;
- [Meta Reels ads guidance](https://www.facebook.com/business/ads/facebook-instagram-reels-ads)
  recommends 9:16 creative with key messages in the safe zone and provides a
  safe-zone checker/template.

Lesson:

- platform policy changes independently from game choreography;
- common master sizes prevent downstream processing failures;
- upload formats are derived artifacts, not the only capture master;
- platform-named profiles require a source URL and review date.

The sources do not justify a timeless universal pixel inset. TikTok explicitly
documents variant-dependent geometry, while YouTube and Meta route creators to
visual templates/checkers. The accepted approach is therefore one
1080x1920/60 media target with a versioned safe-area policy derived from the
intersection of the selected official masks.

The reviewed official geometry is primarily advertising guidance. It cannot be
silently relabeled as an organic specification. Policy inputs therefore declare
`organic_standard`, `paid_ad`, or `measured_organic`: paid-ad templates feed only
explicit `ad-*` policies, and measured organic UI remains labeled measured.
`universal-social-v1` cannot report a standard-policy pass until the complete
worst-case caption/UI, layout-direction, and platform matrix has eligible
standard-organic evidence. Each of the four surfaces has explicit LTR and RTL
rows with pinned measurable UI/caption bounds; rows may share geometry only
after identical normalized mask hashes are proven. Full canonical source-record
hashes and a total sort make the derived policy independent of import order.

V1 ships 1920x1080, `vertical-social-1080p60` at 1080x1920, 1080x1080, and
1280x720 capture/delivery paths. The same vertical delivery is reused for
TikTok, YouTube Shorts, and Instagram/Facebook Reels. Critical content must fit
the conservative common intersection; non-essential background may fill the
frame. Platform masks and delivery profiles are source-dated, hashed policy
data, not hard-coded behavior, baked overlays, or implicit crops. Paid-ad-only
anchors/cards/add-ons resolve stricter explicit variants rather than changing
the standard publishing policy. The same MP4 must also pass four independently
pinned delivery constraint sets—TikTok, YouTube Shorts, Instagram Reels, and
Facebook Reels—because shared geometry alone does not prove container, codec,
FPS, audio, color, duration, or size compatibility.

## 5. Local prototype assessment

The current prototype includes:

- `capture_scenario.py` and a strict scenario schema;
- large-response and paired capture/step client changes;
- Win32 window resize/parking helpers;
- external `record_screen_ffmpeg.ps1`;
- private consumer evidence for game-owned stage-like endpoints.

Keep:

- fresh process and ephemeral loopback port for accepted scripted runs;
- strict manifests and live descriptor validation;
- seeded game-owned content rather than reel-specific Python branches;
- contact sheets and event-boundary evidence;
- staging plus terminal handoff written last;
- build/source/artifact hashes;
- release exclusion of capture-only surface.

Replace or split:

- video-only FFmpeg recording becomes one backend adapter, not the product;
- paired capture/step logic moves to optional offline work and cannot define
  real-time V1;
- portrait-only viewport assumptions become targets/framing variants;
- output-frame-authored events become simulation-tick-authored events;
- scenario/profile/delivery fields become separate normalized documents;
- one mutable take/delivery directory becomes immutable masters plus immutable
  delivery bundles;
- private-game fallbacks and identifiers never enter shared Studio contracts.

## 6. Decisions adopted from the comparison

1. One public recorder, two execution lanes; users should not build a recorder
   per game or per reel.
2. Real-time audiovisual recording is required V1 and does not depend on a
   Neotolis video/PCM stream.
3. FFmpeg is the leading measured exact-window video/remux tool. The primary
   Windows audiovisual candidate adds a narrow process-loopback helper; OBS is
   a separately gated manual/compatibility candidate.
4. `auto` selection is capability-based and stays disabled until a full adapter
   gate passes. A missing audio capability is an error, never a silent
   downgrade.
5. MKV is the default real-time master; MP4 is a validated derived artifact.
6. A compatibility audio mix is preserved; isolated tracks are retained when
   supported.
7. Shot choreography, framing, target/profile, job, master, and delivery are
   separate identities.
8. The common CLI hides jobs for routine work but preserves normalized snapshots
   for audit and queues.
9. The optional offline frame lane follows fixed-step movie-renderer patterns
   only after its engine contracts are proven.
10. Platform presets are versioned policy data with official provenance, never
    implicit crop/reframe logic.
11. Exact authored real-time ticks require a bounded precompiled plan executed
    inside the game frame loop; per-tick network control is not a production
    clock.
12. Base manual recording uses a production-like native build with no stage or
    DevAPI dependency; only authored recording adds capture-stage-core.
13. Application audio is consumer-capability-gated. Multi-process/helper
    topologies must be declared and probed rather than hidden behind the word
    “game”.
14. TikTok, Shorts, and Reels share one 9:16 delivery. Platform differences are
    versioned validation masks; `universal-social-v1` is their conservative
    intersection, not a second or third render.
15. Organic and paid-ad masks are different policy classes. One MP4 is universal
    only after both the complete eligible standard safe-area matrix and all four
    delivery constraint sets pass.

## 7. Approaches explicitly rejected

- an offline-only V1 that cannot record ordinary gameplay with sound;
- a per-game OBS scene or per-game FFmpeg script;
- claiming FFmpeg DirectShow automatically means application audio;
- direct-to-MP4 as the only master;
- changing Neotolis to expose video or PCM for the real-time path;
- coupling authored event times to output frame numbers;
- calling a real-time recording pixel- or byte-deterministic;
- blocking the universal recorder on the optional offline capture gaps;
- using platform names without a source date or baking UI safe areas into
  choreography.

## 8. Research uncertainties to close by measured spikes

Official documentation establishes capability, not compatibility with this
specific engine/build/machine. WP1 of the implementation plan must measure:

1. OBS isolated configuration, exact game-window binding, application audio,
   multi-track output, dropped-frame status, and forced-interruption recovery;
2. FFmpeg HWND geometry under DPI/multiple monitors, DirectShow
   backend/host-scoped device re-resolution, stop behavior, and A/V
   synchronization;
3. recorder overhead and dropped-frame behavior at 1080p60;
4. A/V drift over a 10-minute take;
5. privacy/redaction of OBS credentials, device names, process paths, and user
   directories.

Failure of the OBS spike does not authorize silent capture. It triggers a
backend decision review: repair the adapter, choose another proven external
adapter, or explicitly reduce supported audio policies before implementation.
