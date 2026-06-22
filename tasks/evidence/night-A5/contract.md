# Evidence — A5 (camera life)

## Sprint contract
Commit the authored fixed diorama angle (ll_art.h LL_CAM_* tokens) and add a
subtle eased "breathing" drift so the view feels alive without fighting player
control or causing motion sickness.

## Named acceptance checks
- [x] Default framing uses ll_art.h LL_CAM_PITCH/YAW/DIST (one authored angle).
- [x] Breathing drift is an ADDITIVE offset on top of the stored camera angle
      (player arrows/right-drag still win; drift rides on top).
- [x] Drift is gentle: ~+/-1.1 deg yaw (~30s period), +/-0.7 deg pitch, +/-0.3u
      distance — slow eased sine, no snaps. Disabled in overview/map mode.
- [x] Build green; gameplay unaffected (render-only); validate green.

## Evidence
- `tasks/evidence/night-A5/15-A5-camera.png` — authored diorama framing.

## Verdict
PASS — cleaner cinematic framing + a living, non-intrusive drift.
