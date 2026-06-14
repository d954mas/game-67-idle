# Project Status

Short live project-status index. Workflow rules live in `tasks/README.md`.

## Current Goal

Build `Rune Marches`, an original casual Skyrim-like open-world fantasy RPG
for native PC and web/mobile, with strong visual direction, gameplay, balance,
FTUE, and a path to Poki-audience testing.

## Active Product State

- Active game concept: `Rune Marches`.
- Active runtime: native Rune Marches playable slice in `src/main.c` with
  generated bitmap runtime assets, side quest choice, Old Bell Tower inspect
  action, Reedmere Crossing second road, Moss Shrine kindness payoff, Greenfen
  Causeway route hook, first Greenfen combat beat, Warden Rank II level-up,
  Spark Ward II lore sink, Briar Gate/Moonwell route choice, first
  chosen-route encounters, branch landmark discovery, reward clarity chip, and
  an in-game journal panel.
- Current build target: `game_seed`.
- Current task queue: `E001` active; `T0006` doing; `T0002` doing; `T0003`
  review; `T0004` review; `T0005` done; `T0001` backlog; `T0007` backlog.

## Source Pointers

- Start here: `README.md`, `AGENTS.md`, `AI_PIPELINE.md`.
- Active project wiki: `gamedesign/projects/rune-marches/README.md`.
- Current handoff: `gamedesign/projects/rune-marches/handoff_status.md`.
- First slice plan:
  `gamedesign/projects/rune-marches/game_implementation_plan.md`.
- First playtest packet:
  `gamedesign/projects/rune-marches/playtest/first_session_poki_packet.md`.
- Reusable design knowledge: `gamedesign/knowledge/`.
- Runtime state schema: `state/game_state.schema.json`.
- DevAPI probes: `tools/devapi/smoke_test.py`, `tools/devapi/full_probe.py`.

## Current Evidence

- Template runtime is expected to build with `cmake --build --preset native-debug`.
- DevAPI smoke should validate `ui.tree`, `ui.click`, `game.state`, state set/get, and screenshot capture.
- Rune Marches design gate exists under `gamedesign/projects/rune-marches/`.
- Native placeholder slice builds and passes DevAPI scenario proof:
  `cmake --build --preset native-debug`, `py -3.12
  tools/devapi/smoke_test.py 9123`, `py -3.12 tools/devapi/full_probe.py
  9123`, and `py -3.12 tmp/rune_marches_scenario.py`.
- Latest screenshot evidence:
  `tmp/rune_marches/native_first_slice_labeled.png` and
  `tmp/rune_marches/native_first_slice_portrait_current.png`.
- Generated visual direction:
  `gamedesign/projects/rune-marches/art/fake_shots/rune-marches-gameplay-v1.png`
  with guidance in `gamedesign/projects/rune-marches/art/art_direction.md`.
- Generated runtime asset path:
  `tools/assets/build_rune_marches_assets.py` slices the accepted fake shot into
  `assets/runtime/rune-marches-v1/` PNGs and ignored C texture arrays consumed
  by the native `nt_gfx` quad renderer.
- Native quest expansion:
  `tmp/rune_marches_scenario.py` now proves scout -> combat -> second reward
  -> bell rope side choice -> Spark Ward I -> tower inspect -> Reedmere
  Crossing -> Reed Raider win -> Moss Shrine blessing -> Greenfen Causeway ->
  Fen Shade win -> Warden Rank II -> Spark Ward II -> Briar Gate/Moonwell
  route choice -> Briar Stalker/Moonwell Sentinel -> Ashen Cairn/Starfall
  Grotto -> save/load.
- Latest Moss Shrine evidence:
  `tmp/rune_marches/native_moss_shrine_labeled.png` and
  `tmp/rune_marches/native_moss_shrine_portrait.png`.
- Latest Greenfen Causeway evidence:
  `tmp/rune_marches/native_greenfen_causeway_labeled.png` and
  `tmp/rune_marches/native_greenfen_causeway_portrait.png`.
- Latest first Greenfen beat evidence:
  `tmp/rune_marches/native_greenfen_beat_labeled.png` and
  `tmp/rune_marches/native_greenfen_beat_portrait.png`.
- Latest Spark Ward II evidence:
  `tmp/rune_marches/native_spark_ward_2_labeled.png` and
  `tmp/rune_marches/native_spark_ward_2_portrait.png`.
- Latest Warden Rank II evidence:
  `tmp/rune_marches/native_warden_rank_2_labeled.png` and
  `tmp/rune_marches/native_warden_rank_2_portrait.png`.
- Latest reward clarity evidence:
  `tmp/rune_marches/native_reward_chip_labeled.png` and
  `tmp/rune_marches/native_reward_chip_portrait.png`.
- Latest route choice evidence:
  `tmp/rune_marches/native_route_choice_labeled.png` and
  `tmp/rune_marches/native_route_choice_portrait.png`.
- Latest route encounter evidence:
  `tmp/rune_marches/native_route_encounter_labeled.png` and
  `tmp/rune_marches/native_route_encounter_portrait.png`.
- Latest branch landmark evidence:
  `tmp/rune_marches/native_branch_landmark_labeled.png` and
  `tmp/rune_marches/native_branch_landmark_portrait.png`.
- Passive tooling profile:
  `tmp/session_profiles/session_profile_2026-06-13.jsonl`; current scope
  `T0003/chosen-route-encounter`.
- First Poki-audience test packet:
  `gamedesign/projects/rune-marches/playtest/first_session_poki_packet.md`;
  telemetry map:
  `gamedesign/projects/rune-marches/data/playtest_telemetry.json`.
- Native playtest telemetry endpoint:
  `game.rune.telemetry` tracks the first-session FTUE milestones defined in the
  packet; latest proof screenshots are
  `tmp/rune_marches/native_telemetry_labeled.png` and
  `tmp/rune_marches/native_telemetry_portrait.png`.
- Native playtest probe:
  `tools/playtest/rune_marches_probe.py`; latest report
  `tmp/rune_marches/playtest_probe_report.json` and screenshot
  `tmp/rune_marches/playtest_probe.png`.

## Blocking Work

- Current native visuals/UX are rejected by lead review. The build passes
  automation but does not meet the casual Skyrim visual/playability bar. P0
  rescue task: `tasks/active/T0006-visual-rebuild-and-playability-rescue-for-rune-m.md`;
  review report:
  `gamedesign/projects/rune-marches/reviews/visual_product_profile_review_2026-06-13.md`.
- Final UI/art/economy pacing should not claim reference readiness until
  `T0002` adds stronger raw gameplay frame/timestamp evidence or the user
  approves a narrow exception.
- Web/mobile proof is still unstarted and currently paused after the user
  questioned why the web-build lane started. Native PC remains the active
  implementation harness.
- Runtime art is a first integrated pass, not a final compressed pack. Before
  web validation, decide whether to keep PNGs, encode WebP/compressed variants,
  or wire the engine pack builder.

## Current Gate

- Visual/FTUE rescue work must use `node tools/ai.mjs gate` after screenshot
  capture and `node tools/ai.mjs close-slice` before handoff/review. If the
  product-read gate fails, content expansion stays frozen.

## Next Priorities

1. Stop content expansion and execute `T0006`: visual rebuild and playability
   rescue for the native slice.
2. Rebuild the first screen around one readable player goal, one dominant
   primary action, clean map hierarchy, and separate exploration/combat UI.
3. Finish `T0002` raw gameplay reference evidence gaps for final UI/art/economy.
4. Review the Poki audience test packet in `T0004`, then connect the existing
   native telemetry events to Poki SDK hooks when web/mobile is active.
5. Resume `T0001` web/mobile only when that lane is explicitly active again.
6. Replace first-pass cropped sprites with cleaner transparent source sheets if
   visual QA demands it.
7. Execute `T0007` after the current pipeline utility pass: research and build
   a reusable generated-art/UI asset production skill so future art passes
   produce sliceable/stretchable runtime assets, not only attractive images.

## Validation Policy

- Normal game work: run the narrow native scenario or probe that proves the changed behavior.
- AI/tooling work: use passive, narrow tests first. Broad portable validation requires explicit release/portable/shared-behavior need.
- Web work is in scope for Rune Marches because the user explicitly requested
  web/mobile, but native PC validation remains the first implementation proof.
