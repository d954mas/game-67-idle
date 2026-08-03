# GSD Debug Knowledge Base

Resolved debug sessions. Used by `gsd-debugger` to surface known-pattern hypotheses at the start of new investigations.

---

## obs-window-capture-black — OBS Window Capture encoded uniform black frames
- **Date:** 2026-08-03
- **Error patterns:** OBS Window Capture, uniform black frame, unique=1, luma_range=0.0, stdev=0.0, CreateForWindow 0x80070424
- **Root cause:** OBS WGC was launched as the managed sandbox account while the active desktop and available per-user CaptureService belonged to the interactive console account; win-capture matched the right window but `CreateForWindow` failed with `0x80070424`, producing encoded black frames.
- **Fix:** Run desktop-dependent OBS capture through the approved host execution broker so it inherits the active console user's session token; the original recorder, WGC source configuration, and engine remain unchanged.
- **Files changed:** none
---
