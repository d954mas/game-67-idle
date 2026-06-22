# Evidence — A7 (composition pass) [+ A6 note]

## Sprint contract
Make the room read as a lived-in place, not a grey box, and confirm the art
direction holds across the day/night cycle. Cut any noisy juice (none needed).

## Named acceptance checks
- [x] Back-wall window (frame + 4-pane mullions + sky pane) reads as a real
      window; the pane uses ll_sky_color so it tracks time of day.
- [x] Framed wall poster on the sunlit wall adds character.
- [x] Detail only on the focused lot (far lots stay clean + fogged).
- [x] Grade consistency verified noon vs evening: scene dims coherently, warm/
      cool split holds, window shows dusk sky at 19:32 (see evidence).
- [x] Silhouette/readability: sims read against the floor (contact shadows +
      plumbobs + need bars); juice stays tasteful (nothing to cut).
- [x] Build green; gameplay smoke green; ai.mjs validate green.

## A6 (gameplay depth) — intentionally light
Gameplay systems (needs/AI, build/buy, careers w/ 5-level ladder + promotions,
skills, relationships, multi-lot neighborhood) are already built and verified
(M1-M3, tasks T0106/T0109/T0110). The night's goal is the art/polish/juice
transformation, so A6 was kept to readability tuning folded into A7/A3 rather
than new mechanics. Not parked-as-failure; deliberately scoped out.

## Evidence
- `16-A7-noon.png` — window + poster, midday grade.
- `17-A7-evening.png` — same scene at 19:32: coherent dusk grade, dusk window.

## Verdict
PASS — reads as a real lived-in room; art direction holds across the day cycle.
