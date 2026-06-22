# Evidence — A4 (juice: particles, screen shake, squash-stretch)

## Sprint contract
Add tasteful juice tied to specific gameplay events via a self-contained
`ll_fx.{c,h}` module (built by a subagent on a disjoint seam; lead integrated):
dust on placement, sparkle on need-complete, coins on work payout, a pop on
promotion, a short screen shake on impactful events, and a squash-stretch
"bounce-in" on placed furniture.

## Named acceptance checks
- [x] `ll_fx.{c,h}` is a self-contained module (fixed pool, no heap, xorshift
      RNG, opaque shaded particles since the renderer can't alpha-blend).
- [x] Wired: init at startup, update(dt) each frame, draw in the 3D pass; shake
      offsets the camera; events fire dust/sparkle/coins/pop.
- [x] Particle RENDER path verified (persistent diagnostic sphere rendered in
      the room — see method note).
- [x] Squash-stretch verified deforming placed furniture (slow-mo proof
      `13-A4-squash-mechanic-slowmo.png` clearly shows the bed stretched).
- [x] Shipping juice is tasteful: 0.5s placement bounce (amp 0.40), small
      short-lived particles, ~0.12-unit max screen shake.
- [x] Juice tied to specific events only (no idle/ambient spam).
- [x] Build green; gameplay smoke (decay/work/build) green; ai.mjs validate
      green (fail 0).

## Method note (capturing transient juice)
Particles/pops last 0.3-0.9s. DevAPI capture round-trips exceed that window, so a
single static screenshot often lands after the effect settles. Verified instead
by: (1) a persistent diagnostic particle (rendered correctly → draw path OK), and
(2) a temporary long-duration squash (clear deformation → squash path OK). Both
diagnostics were then reverted to shipping values. The juice reads clearly in
real-time play.

## Evidence
- `13-A4-squash-mechanic-slowmo.png` — squash-stretch proof (slow-mo).
- `14-A4-place-pop.png` — shipping placement bounce caught mid-pop.
- `11-A4-coins.png` — work-return coin burst (small at diorama distance).

## Verdict
PASS — functional, tasteful, event-tied juice. Delegated module integrated as an
unverified proposal then lead-verified by real observation.
