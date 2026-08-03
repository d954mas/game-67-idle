---
status: resolved
trigger: "Так получается запись не работает? давай разбираться и чинить"
created: 2026-08-03
updated: 2026-08-03T20:59:00+05:00
---

# Symptoms

- expected: `capture shot character-actions` records the visible game window and process audio, then publishes validated draft artifacts.
- actual: OBS Window Capture preflight returns a uniform black 720x1280 frame and aborts before REC.
- errors: `OBS window source stayed unhealthy` with unique=1, luma_range=0.0, stdev=0.0.
- timeline: successful validated H.264/AAC capture exists from 2026-07-28; current failure reproduced on 2026-08-03.
- reproduction: from the game root run `.\capture.cmd shot character-actions`.

# Current Focus

- hypothesis: resolved — WGC fails when OBS inherits the managed sandbox token instead of the active console user's token and per-user CaptureService.
- test: self-verification passed and the user confirmed the published edit.mp4 in the real desktop workflow.
- expecting: satisfied — the draft shows the character-actions scenario rather than black frames and contains the intended process audio.
- fault_tree:
  - OR generated source contract mismatch: live title/class/executable descriptor or capture_method no longer matches the actual HWND.
  - OR initialization race: OBS binds before the game swapchain/window is capture-ready and never recovers.
  - OR disposable runtime/profile defect: staged OBS/plugin/config contents are incomplete or stale.
  - OR host graphics/WGC failure: OS, driver, GPU routing, overlay, or protection state makes WGC return a black texture.
  - OR game presentation change: the HWND remains visible but its render/swapchain properties no longer support the pinned WGC path.
  - OR health-probe defect: the scene is live but screenshot polling samples the wrong scene/source.
- next_action: completed — reusable host-execution instructions and fail-fast WGC service diagnostics were added to Runtime Automation.
- reasoning_checkpoint:
    hypothesis: "OBS records black because WGC CreateForWindow executes under the managed sandbox account, which lacks the active console user's per-user CaptureService, so CreateForWindow returns 0x80070424."
    confirming_evidence:
      - "OBS matched the exact GLFW game descriptor, selected WGC, then logged CreateForWindow 0x80070424; that HRESULT decodes to missing service."
      - "GDI captured the same HWND nonblack, while unchanged WGC capture passed on attempt 1 and published a draft when launched through the active-user host broker."
    falsification_test: "Run the unchanged capture command under the active console identity; continued 0x80070424/black output would disprove the user-token mechanism. It instead passed."
    fix_rationale: "Desktop capture must run through the approved host broker so game and OBS inherit the active console token and can access that user's CaptureService; no backend or engine change is warranted."
    blind_spots: "The completed OBS process exited before its child PID owner was queried directly; ownership is inferred from the verified active-user host shell and normal Windows child-token inheritance."
- reasoning_checkpoint:
- tdd_checkpoint:

# Evidence

- timestamp: 2026-08-03T20:12:26+05:00
  observation: six official WGC preflight attempts produced uniform black health PNGs; no draft capture was published.
- timestamp: 2026-08-03T20:04:05+05:00
  observation: engine-native PNG capture and game-specific lever/tram/drag scenario passed, proving the game rendered nonblank frames in the same session.
- timestamp: 2026-08-03T20:20:00+05:00
  observation: no .planning/debug/knowledge-base.md entry was available to supply a known-pattern candidate; common-pattern scan prioritizes Environment/Config because a formerly working external capture path now fails uniformly while native rendering remains healthy.
- timestamp: 2026-08-03T20:25:00+05:00
  observation: the active private game's capture.cmd delegates unchanged to ai_studio/runtime_automation/capture_workflow.py, whose documented single path is disposable OBS Window Capture via WGC plus process-loopback audio.
- timestamp: 2026-08-03T20:25:00+05:00
  observation: catalog shot character-actions still targets build/devapi-debug/bin/game.exe with preset social and a deterministic 4.55-second scenario, so shot selection itself contains no alternate capture backend or window override.
- timestamp: 2026-08-03T20:30:00+05:00
  observation: root git history shows no recorder or capture_workflow changes after the successful 2026-07-28 takes; the newest relevant commit predates them at 2026-07-28 10:10 +05:00, eliminating a post-success source-code regression in those shared files.
- timestamp: 2026-08-03T20:30:00+05:00
  observation: a failed take retained a 5,634-byte uniform health PNG and a 200,704-byte retry recording, while earlier successful takes published ~2.81 MB recording.mkv files and nonuniform 166-170 KB representative frames.
- timestamp: 2026-08-03T20:36:00+05:00
  observation: record_take waits for one stable HWND, resolves its live title/class/executable into an exact OBS descriptor, pins window_capture method=2 (WGC), brings that HWND forward, and samples the OBS recording at 1.0, 2.5, and 4.0 seconds before rejecting it.
- timestamp: 2026-08-03T20:36:00+05:00
  observation: every disposable OBS session, including its generated Recorder.json and OBS logs, is unconditionally deleted in record_take finally; failed public takes therefore preserve the black media symptom but not the source-attachment diagnostics needed to distinguish descriptor/plugin/WGC failures.
- timestamp: 2026-08-03T20:41:00+05:00
  observation: a parallel preserved-session probe reported OBS WGC CreateForWindow failure 0x80070424 in the disposable OBS session and an apparent current-user vs active-console-user mismatch; this is now the primary falsifiable hypothesis pending direct verification.
- timestamp: 2026-08-03T20:48:00+05:00
  observation: direct log inspection confirms OBS 30.1.2 loads D3D11, win-capture.dll, NVENC, the exact private-game source descriptor, selects WGC, then fails at CreateForWindow with 0x80070424 before recording starts; the resulting MKV still contains 981 H.264 frames but they are uniformly black.
- timestamp: 2026-08-03T20:48:00+05:00
  observation: 0x80070424 maps to Win32 1060, "The specified service does not exist as an installed service". The command used a managed sandbox token while the desktop belonged to the active console account; both were in the same console session, isolating the difference to the user/security token rather than a disconnected desktop session.
- timestamp: 2026-08-03T20:52:00+05:00
  observation: service inspection shows the CaptureService template and one per-user instance for the interactive account, while the capture process used the distinct sandbox account; an independent GDI capture of the exact same GLFW HWND is nonblack, ruling out an invisible/blank game window and isolating failure to WGC's per-user broker path.
- timestamp: 2026-08-03T20:54:00+05:00
  observation: an approved host identity probe runs as the active console account, providing the one-variable counterfactual needed to test the per-user WGC broker mechanism with the original capture command unchanged.
- timestamp: 2026-08-03T20:55:00+05:00
  observation: the exact unchanged .\\capture.cmd shot character-actions command, launched through the active-user host broker, passed WGC preflight on attempt 1, completed REC for 4.55 seconds, exited 0, and published a validated draft.
- timestamp: 2026-08-03T20:57:00+05:00
  observation: published capture.json reports scenarioStatus=completed, sourceStartAttempts=1, preflight lumaStdev=9.88 with 552 colors, final lumaStdev=10.075 with 711 colors, active audio peaking at -7.5 dB, and valid 4.549-second 1080x1920 H.264/AAC output; independent ffprobe confirms 272 video frames and 48 kHz stereo audio.

# Eliminated

- hypothesis: the generated descriptor targets the wrong window.
  evidence: OBS logs the exact game executable and reaches WGC CreateForWindow; its explicit HRESULT is a capture-runtime initialization failure, not a window search miss.
  timestamp: 2026-08-03T20:48:00+05:00
- hypothesis: the screenshot health probe samples the wrong scene or rejects a healthy source.
  evidence: OBS itself logs CreateForWindow 0x80070424 before recording starts, independently explaining the uniform source frames.
  timestamp: 2026-08-03T20:48:00+05:00
- hypothesis: a recorder-code or OBS-install version update after 2026-07-28 regressed WGC configuration.
  evidence: recorder history is unchanged after the successful take, and the installed OBS executable is still 30.1.2 with its original 2024 timestamp.
  timestamp: 2026-08-03T20:48:00+05:00

# Resolution

- root_cause: OBS WGC was launched by the managed command sandbox under a different user token from the active desktop and its per-user CaptureService; win-capture matched the right window but CreateForWindow failed with 0x80070424, producing encoded black frames.
- fix: run desktop-dependent OBS capture through the approved host execution broker so it inherits the active console user/session token; keep the original WGC backend and add reusable launch instructions plus service-failure diagnostics.
- verification: the unchanged reproduction command passed through the active-user host broker and published a technically valid, nonblack, audible draft on the first OBS source attempt; the user then confirmed successful playback in the real desktop workflow.
- files_changed:
  - .codex/skills/nt-runtime-automation/SKILL.md
  - .codex/skills/nt-runtime-automation/references/runtime-workflow-rules.md
  - ai_studio/runtime_automation/README.md
  - ai_studio/runtime_automation/record_game.py
  - ai_studio/runtime_automation/record_game_test.py
