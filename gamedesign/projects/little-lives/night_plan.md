# OVERNIGHT PLAN — Little Lives: Debug-Art Monolith → Polished Stylized Low-Poly Game

*For a human lead to launch an autonomous orchestrator agent. Repo: `C:/projects/game-67-idle`. Active game: Little Lives. Branch off master before starting.*

---

## 1. Night goal

Take Little Lives from a single-file debug-art prototype (`src/clean_seed_main.c`, ~2032 lines, flat untextured `nt_shape_renderer_mesh`) to a **full, polished, coherent stylized-flat-low-poly life sim** — real gameplay depth (needs/AI/build-buy/careers/social), one authored art direction (baked vertex lighting + AO, gradient sky, fixed diorama camera, color grading), a clean engine-font HUD, and juice on every core action. The development itself is **delegated by module**: the orchestrator splits the monolith into disjoint files behind frozen interfaces, fans out leaf modules to parallel coding subagents, and keeps integration + the shared state struct + the main loop single-threaded. **Honest target: a deliberately polished STYLIZED FLAT-SHADED LOW-POLY "miniature diorama" look — NOT photoreal.** Photoreal is impossible: the engine (`external/neotolis-engine`) is read-only and exposes only the flat shape renderer. The whole night runs unattended: snapshot before every wave, bounded per-worker writes, per-slice self-verification, commit per slice, stop on repeated failure.

---

## 2. Target modular architecture

The current monolith already has clean internal seams (verified against the file). Wave 0 extracts them into disjoint `.c`/`.h` pairs. **Globals (`s_sims`, `s_objects`, `s_lots`, `s_vp`, `s_daylight`) move behind accessors in lead-owned `ll_state.c` so modules touch state through the interface, never each other's raw globals** — this is the single biggest enabler of safe parallel writers ([Cognition: conflicting implicit decisions are what break parallel agents](https://cognition.com/blog/dont-build-multi-agents)).

| Module (file) | Owns (current lines) | Public interface (lead-frozen header) |
|---|---|---|
| **`ll_state.{c,h}`** *(LEAD-ONLY)* | `GameState`/`Sim`/`Object`/`Lot`/`Box` structs, enums, generated state codegen, all global accessors, `game_reset` | `ll_state.h` — the one struct/enum/accessor contract every other module includes read-only |
| **`ll_sim.{c,h}`** | needs decay, `decay_per_min`, `sim_mood`, `command_need/work`, `sim_update`, `update_clock`, `game_update` (~245–776) | `ll_sim.h` — `sim_update()`, `command_*()`, `sim_mood()` |
| **`ll_ai.{c,h}`** | `sim_ai`, `move_toward`, `object_for_need`, `find_free_object`, `release_target`, `go_idle`, `social_autopair` (~432–740) | `ll_ai.h` — `sim_ai()`, `social_autopair()`, pathing helpers |
| **`ll_world.{c,h}`** | `place_object`, `furnish_lot`, `world_init`, lot setup, `set_active_lot` (~306–424, 1219) | `ll_world.h` — `world_init()`, `place_object()`, `set_active_lot()` |
| **`ll_buildbuy.{c,h}`** | `try_place_object`, `remove_object_at_cursor`, `find_free_object` (build path), prices/palette (~1308–1352, `object_price`) | `ll_buildbuy.h` — `try_place_object()`, `remove_object_at_cursor()` |
| **`ll_careers.{c,h}`** | skills, career levels, work payouts, promotion logic (currently inline in sim/command_work) | `ll_careers.h` — `career_tick()`, `career_promote()`, `skill_gain()` |
| **`ll_render.{c,h}`** | `compute_camera`, `draw_ground/lot/object/mesh/sim/build_ghost`, `render_world`, `world_to_screen`, `mesh_for_kind` (~778–1033) | `ll_render.h` — `render_world()`, `compute_camera()` |
| **`ll_art.{c,h}`** *(LEAD-FROZEN TOKENS)* | palette constant, `shade()`, baked sun dir + AO model, `need_color`, `object_color`, `s_daylight`, sky/fog, camera framing | `ll_art.h` — read-only palette + lighting tokens consumed by render/hud (authored ONCE) |
| **`ll_ui.{c,h}`** | `hud_layout`, `draw_panel`, `draw_hud`, `draw_sim_billboards`, `rect2`, engine-font text (~1033–1219, `text_*`) | `ll_ui.h` — `draw_hud()`, `hud_layout()` |
| **`ll_input.{c,h}`** | `pick_world`, `handle_input`, picking math (~1233–1521) | `ll_input.h` — `handle_input()`, `pick_world()` |
| **`ll_devapi.{c,h}`** | `sims_json`, `objects_json`, `emit_state`, `ep_*`, `register_endpoints` (~1522–1716) | `ll_devapi.h` — `register_endpoints()` |
| **`clean_seed_main.c`** *(LEAD-ONLY)* | thin `frame()`, `main()`, init order, DevAPI registration aggregation | calls every module's public API |

**The art-direction tokens (`ll_art.h`) are authored once by the lead and handed to every draw agent as a read-only contract.** Each draw agent inventing its own palette/lighting is the Flappy-Bird mismatch at the visual level ([Cognition](https://cognition.com/blog/dont-build-multi-agents)) — forbidden.

---

## 3. Operating loop (autonomous, all night)

Maps Anthropic's long-running-harness pattern onto this repo: durable state lives in files, not context; boot every session with the same smoke; verify by real observation; commit per slice; never delete tests ([Anthropic: Effective harnesses for long-running agents](https://www.anthropic.com/engineering/effective-harnesses-for-long-running-agents)).

**Durable state (all outside model context):**
- Backlog = `tasks/active/` slices, each with a `passes:false/true` flag.
- Progress log = per-slice evidence files in `tasks/evidence/` + append-only `tasks/STATUS.md` session log.
- `init.sh` = the smallest build+run preset (below).
- Git history = checkpoint per slice.

**`init.sh` (lead writes in Wave 0, the one reliable build+run):**
```sh
cmake --preset native-debug
cmake --build --preset native-debug --target game_seed
# run with DevAPI for headless smoke:
build/game_seed/native-debug/game_seed.exe --devapi 9123 --window-size 1280x720 &
```

**Per-slice loop (orchestrator repeats until backlog done or stop condition):**
1. **Boot:** `pwd`; read `git log` + `tasks/STATUS.md` + latest evidence; run `init.sh` + DevAPI smoke (`ui.tree` round-trips, `state` emits, screenshot non-blank). **Fix any regression FIRST. Never stack work on a broken foundation.**
2. **Pick** highest-priority `passes:false` slice on a disjoint seam.
3. **Snapshot:** `git tag night/<slice-id>-pre` before anything risky (refactor/schema/art-pivot).
4. **Sprint contract:** write the slice's deliverable + named acceptance checks into its evidence file BEFORE building.
5. **Build:** if the slice is N independent leaf modules → dispatch 3–5 parallel coding subagents, each with a packet (objective; owned writable files; read-only contract header; forbidden files; `build_cmd`; self-verify = build + `node tools/ai.mjs validate` + DevAPI screenshot if visual; return = patch + evidence). Coupled glue (frame order, struct, DevAPI table) → lead does it inline. Treat each subagent return as an **unverified proposal**, not a commit ([Cognition](https://cognition.com/blog/dont-build-multi-agents)).
6. **Integrate (lead, single-threaded):** apply patches, full build of `little_lives`.
7. **Self-verify (real observation, not "I wrote code"):** build green → `node tools/ai.mjs validate` (add `--full` on render/UI waves) → DevAPI smoke (scripted `select sim → command need → advance time → capture`) → product/visual gate `node tools/ai.mjs gate` with before/after screenshot vs the fixed art direction.
8. **Commit + flip:** one descriptive commit; set `passes:true`; append the slice + evidence to `tasks/STATUS.md`. Co-author trailer per repo rules.
9. **Next slice.**

**Hard rules every iteration:** no edits to `external/neotolis-engine`; writes bounded to `src/` + `tasks/`; subagents may add acceptance checks but **may NOT delete or weaken existing tests/gates** to claim done ([Anthropic](https://www.anthropic.com/engineering/effective-harnesses-for-long-running-agents)); circuit breaker trips at **2 consecutive integration failures of the same slice** OR a no-progress loop OR ~12 iterations/slice → revert to `night/<slice-id>-pre`, run `node tools/product_gate/repeated_failure_guard.mjs`, **park the slice with a written reason and switch path** (do not retry forever).

---

## 4. Backlog (prioritized, bounded, verifiable)

Front-loads the safe refactor (unlocks parallelism), then depth, then the art/polish/juice payload, then hardening. `[P]` = parallel module fan-out; `[S]` = serial / lead-only.

| # | Slice | Acceptance |
|---|---|---|
| **S0** `[S]` | **Wave 0 — split monolith into module files; author + freeze `ll_state.h` (struct/enum/accessors) and per-module `ll_api.h`; move globals behind accessors.** *Riskiest; do first; snapshot after.* | Green build of `little_lives`; `node tools/ai.mjs validate`; DevAPI smoke unchanged from pre-split screenshot (behavior identical). |
| **S1** `[S]` | Author `ll_art.h` art-direction tokens ONCE: limited palette (8–15 colors, warm=nature/cool=built/one reserved accent=interactive), single sun dir, ambient 0.35–0.5, AO shadow tone, fog color, fixed 3/4 diorama camera params. | Build; tokens compile; before/after screenshot shows palette applied to one test prop. |
| **S2** `[P]` | **Wave 1:** `ll_sim` agent (needs/decay/mood) + `ll_world` agent (lots/placement) — disjoint, behind accessors. | Each TU compiles; full build; validate; DevAPI: needs decay over advanced time, lot furnishes. |
| **S3** `[P]` | `ll_ai` agent (pathing, target selection, `social_autopair`) + `ll_buildbuy` agent (place/remove/price). | Build; validate; DevAPI: sim routes to need object; place + remove object via `ui.click` flow passes named checks. |
| **S4** `[P]` | **Careers depth:** `ll_careers` agent — skills gain, 5-level career ladder, payouts, promotion; wire into `sim_update` via interface. | Build; validate; DevAPI: work raises skill + money, promotion fires at threshold. |
| **S5** `[S]` | **Social/relationship depth** (lead — touches shared sim state): relationship scores, autopair → friendship tiers, mood effects. | Build; validate; DevAPI: two sims socialize, relationship score rises, mood reflects. |
| **S6** `[P]` | **Wave 2 — art payload (all read `ll_art.h`, write only own draw file):** `ll_render` agent bakes per-vertex sun lighting + AO into mesh colors (flat/split normals, crisp facets). | Build; validate `--full`; screenshot shows faceted shading + grounded AO contact shadows (vs flat-color before). |
| **S7** `[P]` | `ll_render` agent (cont.): gradient sky dome/quad + graded ground + distance fog (aerial-perspective blue shift); warm-highlight/cool-shadow temperature split + final grade. | Build; validate `--full`; screenshot: scene reads as a place, not objects floating in void. |
| **S8** `[P]` | **HUD polish:** `ll_ui` agent — engine font renderer ONLY (debug `draw_text` banned in product view), max 2 fonts, line-height ≥1.5, palette-matched flat panels, corner-parked minimal HUD grouped by importance. | Build; validate `--full`; screenshot at UI/text zoom; no shape-renderer text in product view. |
| **S9** `[S]` | **Camera life** (lead — main-loop coupled): commit fixed diorama angle; subtle eased follow/drift/parallax, gentle constant motion. | Build; DevAPI capture sequence shows smooth eased camera, no snaps. |
| **S10** `[P]` | **Juice — spawn/place/click:** squash-stretch (~150ms overshoot) on place/select; button press squish, hover scale; panel slide/fade (~200–300ms). Tweens in `ll_ui`/`ll_render` own files. | Build; validate; capture sequence shows eased transitions (no instant snaps). |
| **S11** `[P]` | **Juice — feedback:** primitive particles (dust on place, sparkle/coin on reward, footstep dust), brief flashes on need-complete/level-up, short screen-shake (~100–200ms decay) on impactful events; optional hit-stop on big confirms. | Build; validate; screenshots of placement dust + reward sparkle; juice tied to specific gameplay events only. |
| **S12** `[S]` | **`ll_devapi` + save** (lead — registration table is shared surface): aggregate all modules' endpoints; `emit_state` round-trips full new state; JSON save/load via `game_state`. | Build; validate; DevAPI: state save → reset → load restores sims/objects/careers/relationships. |
| **S13** `[S]` | **Integration polish + composition pass** (lead): negative-space framing, silhouette/readability check from fixed camera, final color grade consistency, juice balance (cut noisy/excess). | Build; validate `--full`; product gate `node tools/ai.mjs gate` green; full before/after night screenshot. |
| **S14** `[S]` | **Hardening:** full playable smoke (named acceptance checks + compact summary), `node tools/ai.mjs validate --full`, `node tools/taskboard/cli.mjs validate`, `node tools/skills_eval.mjs`. | All validators green; smoke prints all named checks passing. |
| **S15** `[S]` | **Morning handoff:** finalize `tasks/STATUS.md` (shipped / parked + reasons), gather per-slice screenshots, `node tools/ai.mjs status --agents` telemetry into evidence. | Handoff file + screenshot set + telemetry present; backlog completion % computable from files alone. |

Optional stretch (only if budget remains, each `[P]`-able leaf): ambient floaters/birds, time-of-day palette shift, more object kinds, second-lot visual variety.

---

## 5. Guardrails & stop conditions

**Setup before launch:** `git checkout -b night/little-lives-polish`; `git tag night/start`. Work on the branch all night; never commit to master.

**Invariants (every slice, non-negotiable):**
- **Never edit `external/neotolis-engine`** (read-only). Never use the debug shape `draw_text` in product UI.
- **Bounded writes:** each subagent writes ONLY its packet's owned files; lead owns `ll_state.h`, `ll_art.h`, `clean_seed_main.c`, DevAPI registration table, frame order. A subagent needing a header change files it back; lead edits serially and re-broadcasts.
- **Write-lock map per wave:** explicit file→owner table; no file appears twice; coupled surfaces serialized, never parallelized ([Cognition](https://cognition.com/blog/dont-build-multi-agents)).
- **Snapshot before risk:** `git tag night/<slice>-pre` before every refactor/schema/art-pivot.
- **Gate green per slice:** build + `node tools/ai.mjs validate` + product gate must pass before commit; flip `passes:true` only on real observed evidence.
- **Test ratchet:** add checks, never delete/weaken them ([Anthropic](https://www.anthropic.com/engineering/effective-harnesses-for-long-running-agents)).
- **Cap fan-out** at 3–5 subagents/wave; trivial change = lead inline, no subagent.
- **Fail-closed:** if a gate dependency is unavailable, requeue/park — do not dispatch unverified work.

**Circuit breaker (any trips → revert to last green tag, narrow or park):**
- 2 consecutive integration failures (build/gate) of the same slice → `node tools/product_gate/repeated_failure_guard.mjs`, park, switch path.
- No-progress loop (identical action repeated) or ~12 iterations on one slice.

**Stop the night (any one ends the run):**
- Backlog exhausted (all `passes:true` or parked).
- Circuit breaker tripped on 2 distinct slices in a row.
- `init.sh` + smoke cannot reach a working app after one repair attempt.
- Global iteration/cost budget hit.

**On stop:** write the morning handoff (S15) regardless — what shipped, what parked + why, screenshots, telemetry.

---

## 6. Morning verification (lead checks files only, no rerun needed)

1. **`git log night/start..HEAD`** — one descriptive commit per slice; confirm checkpoints exist (recoverable, not one end-of-night blob).
2. **Build + run:** `cmake --preset native-debug` → build `game_seed` → launch with `--devapi 9123` → eyeball the game.
3. **Screenshots:** compare `night/start` screenshot vs final — does it read as a polished stylized diorama (faceted lighting, AO, gradient sky/fog, clean engine-font HUD, juice)?
4. **`node tools/ai.mjs status --agents`** — how it delegated: which slices fanned out, subagent counts, any thrash.
5. **Validators:** `node tools/ai.mjs validate --full`; `node tools/taskboard/cli.mjs validate`; `node tools/ai.mjs gate`; `node tools/skills_eval.mjs` — all green.
6. **Per-slice evidence:** each `tasks/evidence/` file has sprint contract + named acceptance + screenshot; `tasks/STATUS.md` lists shipped vs parked (with reasons) + completion %.

---

## 7. The overnight prompt (copy-paste into a fresh chat)

```
You are the autonomous orchestrator for an overnight, unattended run on the game
"Little Lives" at C:/projects/game-67-idle. Engine external/neotolis-engine is
READ-ONLY. Goal: take Little Lives from a debug-art monolith
(src/clean_seed_main.c) to a full, polished STYLIZED FLAT-SHADED LOW-POLY life
sim (gameplay depth + one coherent art direction + visual polish + juice).
Photoreal is impossible and not the target.

DURABLE STATE lives in files, not your context: backlog = tasks/active/ slices
(each has passes:false/true); progress = tasks/evidence/ files + append-only
tasks/STATUS.md; init.sh = smallest build+run; git history = checkpoints.

FIRST: branch `git checkout -b night/little-lives-polish`, tag `night/start`.
Write/refresh the backlog and init.sh, capture a baseline screenshot.

THEN loop until backlog done or a stop condition, per slice:
1. Boot: pwd; read git log + tasks/STATUS.md + latest evidence; run init.sh +
   DevAPI smoke (ui.tree, state, non-blank screenshot). Fix regressions FIRST.
2. Pick highest-priority passes:false slice on a DISJOINT module seam.
3. Snapshot: git tag night/<slice>-pre before any refactor/schema/art change.
4. Write the slice's deliverable + named acceptance checks into its evidence file.
5. Build it. If it's N independent leaf modules, dispatch 3-5 parallel coding
   subagents, each with a packet: {objective; owned writable files; read-only
   contract header it must satisfy; forbidden files; build_cmd; self-verify =
   build + `node tools/ai.mjs validate` + DevAPI screenshot if visual; return =
   patch + evidence}. Coupled glue (the GameState struct, frame()/main loop
   order, the DevAPI registration table, the ll_art.h palette/lighting tokens)
   is YOURS alone — never parallelize it. Treat subagent returns as unverified
   proposals; YOU integrate and compile.
6. Self-verify by real observation: build green -> `node tools/ai.mjs validate`
   (--full on render/UI) -> DevAPI scripted flow (select sim -> command need ->
   advance time -> capture) -> product/visual gate `node tools/ai.mjs gate`
   with before/after screenshot vs the fixed art direction.
7. Commit one descriptive checkpoint, set passes:true, append slice + evidence
   to tasks/STATUS.md.
8. Next slice.

HARD RULES: never edit external/neotolis-engine; writes bounded to src/ + tasks/;
one file = one owner per wave (write-lock map, no file twice); author ll_state.h
+ ll_art.h FIRST and keep them read-only to subagents; you may ADD acceptance
checks but NEVER delete or weaken existing tests/gates; cap fan-out at 5; the
debug shape draw_text is banned in product UI (engine font only).

CIRCUIT BREAKER: 2 consecutive integration failures on the same slice, or a
no-progress loop, or ~12 iterations -> revert to night/<slice>-pre, run
`node tools/product_gate/repeated_failure_guard.mjs`, PARK the slice with a
written reason, switch path. Do not retry forever.

STOP the night when: backlog exhausted; breaker trips on 2 distinct slices in a
row; init.sh+smoke can't reach a working app after one repair; or budget hit.
On stop, always write a morning handoff to tasks/STATUS.md (shipped, parked +
reasons, screenshots, `node tools/ai.mjs status --agents` telemetry).

Keep going autonomously until the backlog is done or you hit a stop condition.
Commit each slice. Leave a complete morning summary in files.
```

---

**Sources:**
- [Anthropic — Effective harnesses for long-running agents](https://www.anthropic.com/engineering/effective-harnesses-for-long-running-agents) (durable-state files, boot smoke, init.sh/progress/feature_list, real-observation verification, test ratchet "unacceptable to remove or edit tests")
- [Cognition — Don't Build Multi-Agents](https://cognition.com/blog/dont-build-multi-agents) (conflicting implicit decisions break parallel subagents; the Flappy-Bird mismatch; disjoint-seam-only parallelism; treat subagent output as a proposal)
- [AINews — Cognition vs Anthropic (June 2026)](https://news.smol.ai/issues/25-06-13-cognition-vs-anthropic) (lead-orchestrator + disjoint fan-out is the reconciled pattern)

Repo facts grounding the plan (verified this session): monolith seams at the line ranges in §2 (`C:/projects/game-67-idle/src/clean_seed_main.c`); build via `cmake --preset native-debug` target `game_seed` (`C:/projects/game-67-idle/CMakePresets.json`); gates at `C:/projects/game-67-idle/tools/ai.mjs` (`validate`/`gate`), `C:/projects/game-67-idle/tools/product_gate/repeated_failure_guard.mjs`, `C:/projects/game-67-idle/tools/taskboard/cli.mjs validate`, `C:/projects/game-67-idle/tools/skills_eval.mjs`; backlog/evidence store at `C:/projects/game-67-idle/tasks/`.