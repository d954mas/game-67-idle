# Night Backlog — Little Lives polish (branch night/little-lives-polish)

Durable slice tracker for the overnight run. Source of truth = this file +
`tasks/STATUS.md` + git tags. Evidence lives in `tasks/evidence/night-<slice>/`.
Lives here (next to `night_plan.md`) rather than `tasks/active/` so it stays a
plain tracker, not a taskboard item (keeps `taskboard validate` green).

Orchestrator decision (recorded): the plan's full 12-file monolith split (S0) is
high-risk mechanical churn with **zero visual payoff** for an unattended run, and
the highest-value work (the art payload) is coherence-critical — Cognition's
lesson is that parallel agents break on shared art/lighting decisions. So instead
of splitting the file, the lead authored `ll_art.h` as the frozen art-direction
contract and reworks the renderer **in-place** (lead-owned, coherent), and fans
out only genuinely-independent NEW-file work (juice/particles in `ll_fx.{c,h}`)
to subagents. This honors the plan's deeper principle (disjoint-seam-only
parallelism, lead owns coherence-critical glue, snapshot/verify/commit per slice,
never weaken gates) over its literal "split into 12 files / fan out every wave."

| id | slice | owner | passes | tag |
|----|-------|-------|--------|-----|
| A0 | Author `ll_art.h` frozen art tokens + lighting math (palette, sun dir, ambient, AO, fog, sky bands, grade) | lead | true | night/A0-pre |
| A1 | Render rework #1: directional surface shading (ground/floor/walls) + banded gradient sky + distance fog + warm/cool grade | lead | true | night/A1-pre |
| A2 | Render rework #2: per-face faceted furniture shading + AO contact shadows under objects & sims + lit sims | lead | true | night/A2-pre |
| A3 | HUD polish: palette-matched flat panels, grouped corner HUD, engine-font only, readable at zoom | lead | true | night/A3-pre |
| A4 | Juice — `ll_fx.{c,h}` new module: squash-stretch on place/select, place dust, reward sparkle, screen shake | agent+lead | true | night/A4-pre |
| A5 | Camera life: fixed diorama angle + subtle eased drift/follow (main-loop coupled) | lead | true | night/A5-pre |
| A6 | Gameplay depth pass: tune careers/social readability + any cheap depth (lead judgment) | lead | false | night/A6-pre |
| A7 | Composition + balance pass: silhouette/readability from camera, grade consistency, cut noisy juice | lead | false | night/A7-pre |
| A8 | Hardening: full smoke (named checks), `ai.mjs validate`, taskboard validate; final before/after | lead | false | night/A8-pre |
| A9 | Morning handoff: finalize STATUS.md (shipped/parked + reasons), screenshots, telemetry | lead | false | — |

Priority order: A0 -> A1 -> A2 -> A3 -> A4 -> A5 -> A6 -> A7 -> A8 -> A9.
Visual payload (A1/A2) is the headline; gameplay already works (M1-M3 verified).

Extra tooling added this run (automation aids, not gameplay):
- `game.debug.set_time {minutes,pause}` DevAPI — pin clock for comparable shots.
- `tools/little-lives/ll_capture.py <out> [wait] [mode] [minutes]` — clean capture.
