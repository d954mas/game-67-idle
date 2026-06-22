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
| A6 | Gameplay depth pass: tune careers/social readability + any cheap depth (lead judgment) | lead | folded-A7 | night/A6-pre |
| A7 | Composition + balance pass: silhouette/readability from camera, grade consistency, cut noisy juice | lead | true | night/A7-pre |
| A8 | Hardening: full smoke (named checks), `ai.mjs validate --full`, taskboard validate, skills_eval | lead | true | night/A8-pre |
| A9 | Morning handoff: finalize STATUS.md (shipped/parked + reasons), screenshots, telemetry | lead | true | — |

Priority order: A0 -> A1 -> A2 -> A3 -> A4 -> A5 -> A6 -> A7 -> A8 -> A9.
Visual payload (A1/A2) is the headline; gameplay already works (M1-M3 verified).

Extra tooling added this run (automation aids, not gameplay):
- `game.debug.set_time {minutes,pause}` DevAPI — pin clock for comparable shots.
- `tools/little-lives/ll_capture.py <out> [wait] [mode] [minutes]` — clean capture.

## Final result (morning handoff)

All slices shipped; nothing parked. 8 checkpoint commits on
`night/little-lives-polish` (A0+A1 combined, A2, A3, A4, A5, A7, A8/A9), each
with a `night/<id>-pre` snapshot tag. Gameplay (M1–M3) unchanged + smoke-verified
across the run. Net new/changed runtime: `src/ll_art.h`, `src/ll_fx.{c,h}`,
in-place rework of `src/clean_seed_main.c`; tooling: `game.debug.set_time` DevAPI,
`tools/little-lives/ll_capture*.py`, `init.sh`.

Gates (final): build green; gameplay smoke decay/eat/work/build green;
`node tools/ai.mjs validate --full` green (incl. visual invariant guard);
`node tools/taskboard/cli.mjs validate` green; `node tools/skills_eval.mjs` green.

### Delegation telemetry (`node tools/ai.mjs status --agents`)
- 1 subagent delegated: "Write ll_fx juice module" (general-purpose) — Bash 4,
  Read 3, Grep 2, Write 2 | ~2m | 1 tool-err | ok. Returned ll_fx.{c,h} as an
  unverified proposal; lead integrated + verified by real observation, then
  tuned shipping values. All other slices were lead-owned (coherence-critical
  render/HUD/camera/composition work kept single-threaded, per the recorded
  decision above).

### Morning verification (files only)
- `git log night/start..HEAD` — 8 descriptive checkpoints.
- Headline before/after: `tasks/evidence/night-baseline/00-baseline-live.png`
  vs `tasks/evidence/night-A7/16-A7-noon.png` (+ `17-A7-evening.png` for the
  day/night grade). Per-slice `tasks/evidence/night-A*/contract.md`.
