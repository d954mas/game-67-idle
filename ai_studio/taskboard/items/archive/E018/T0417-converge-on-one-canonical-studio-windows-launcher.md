---
id: T0417
title: Converge on one canonical Studio Windows launcher
status: done
project: P001
epic: E018
priority: P1
tags: [windows, shell, launcher, integration]
created: 2026-07-13
updated: 2026-07-13
quality: {"checks":[{"id":"QTECH_001","outcome":"pass","evidence":"QTECH_001=Authorized Windows launcher integration 2/2 in 9.2s; PID lifecycle 2/2; shell domain pass; two independent final risk reviews PASS."}]}
---

## What

Make the Windows agent launch route singular and reliable.

## Done when

- [x] `start_site_windows.ps1` is the only documented detached Windows agent
      launcher; duplicate `.cmd` and detached Node lifecycle wrappers are gone.
- [x] `server.mjs` remains the foreground cross-platform HTTP implementation.
- [x] One Windows integration smoke starts on a free port, proves HTTP/PID/log
      state, and stops the recorded process; unit runs do not launch it.
- [x] Server routes/lifecycle keep direct Node tests; they do not assert exact
      PowerShell script text. The PowerShell smoke runs only on Windows
      integration/CI or when explicitly requested.
- [x] Documentation and Architecture Map name the same single agent entry and
      foreground server boundary.
- [x] No routine WSL route is added.
- [x] The canonical agent command invokes the PowerShell launcher directly,
      never through WSL.

## Open questions

- None blocking; prefer the fewest files after callers are inventoried.

## Log

- 2026-07-13: Lead selected one Windows agent entry that must work reliably.
- 2026-07-13: Read-only launcher audit confirmed the PowerShell route already
  owns health/PID/log startup and found no active product dependency on the
  `.cmd` or detached Node wrappers. Before live smoke, fix the current
  unconditional `taskkill` path: a stale/reused PID must be verified as the
  matching `node .../studio_shell/server.mjs <port>` process or left untouched.
  Also validate ports, require a live matching PID after health, and clean
  failed runner/PID state. No WSL route or new health endpoint is needed.
- 2026-07-13: TDD removed the unconditional PID kill, added exact live
  `node .../studio_shell/server.mjs <port>` ownership verification, port bounds,
  stale/failed state cleanup, and own-PID-only server shutdown cleanup. The
  `.cmd` and detached Node wrappers were deleted; PowerShell is the one Windows
  agent entry and `server.mjs` remains the foreground implementation.
- 2026-07-13: Verification: server PID lifecycle 2/2; normal shell domain pass
  in 0.62s without integration launch; explicit Windows integration 2/2 in
  11.9s covering invalid port, start, reuse, restart, foreign occupied port,
  stale foreign PID, logs, PID, HTTP, and cleanup. Diff check clean and no WSL
  call was used. Two independent risk-tier reviews remain before final close.
- 2026-07-13: HIGH review found that the fixed runner filename allowed an old
  runner's `finally` to delete a newer launch, and that reopening a PID during
  stop weakened identity safety and left Chat descendants outside cleanup.
  The launcher now uses a unique runner per launch, removes only that runner,
  cleans legacy/orphan runner state only after ownership is absent, retains the
  verified `System.Diagnostics.Process` handle through `taskkill /T /F`, and
  disposes it only after the verified process tree exits. The Windows smoke now
  performs two consecutive restarts, proves unique-runner isolation, stale
  legacy/orphan cleanup, foreign PID survival, failure-log preservation, and
  complete success-log cleanup.
- 2026-07-13: Independent risk review reproduced a HIGH restart race missed by
  the first green smoke: the old fixed-name runner could delete a newly written
  runner. It also found one stale Taskboard skill command, non-tree hard kill,
  verify/reopen PID identity risk, and success-log leakage. T0417 stays open;
  unique runner identity, retained verified process handle/tree cleanup,
  repeated restart, reference repair, and clean-success teardown are required
  before both reviewers repeat their checks.
- 2026-07-13: Re-review confirmed the direct host-process Windows smoke passes
  2/2 in 8.3s; only managed-sandbox `taskkill` is denied. The verified process-
  tree kill remains intact, and the README plus Taskboard skill now state the
  shortest operational contract: invoke direct PowerShell with host-process
  permission outside the managed sandbox, never through WSL.
- 2026-07-13: Final close: root-authorized Windows smoke 2/2 in 9.2s, PID
  lifecycle 2/2, shell domain and skill validation pass, diff check clean, and
  both independent risk re-reviews PASS with no blocker.
- 2026-07-13: Quality: QTECH_001=pass; evidence: QTECH_001=Authorized Windows launcher integration 2/2 in 9.2s; PID lifecycle 2/2; shell domain pass; two independent final risk reviews PASS.
