---
id: T0010
title: "Pipeline subtract: consolidate the 68-script tool surface (only 9 used in 2 active days)"
status: todo
epic: E001
priority: P2
tags: [pipeline, subtract, lean, cleanup]
created: 2026-06-17
updated: 2026-06-17
---

## What

Profiler data (2026-06-16 + 06-17, both active days) shows the meta-tooling
surface is bloated: **68 non-test tool scripts on disk, only ~9 actually invoked**
(`voxelheim_play_test`, `taskboard/cli`, `ui_readability`, `state_codegen/generate_state`,
`shoot_voxelheim`, `pixel_health`, `ai.mjs status/context`). Aligns with the lean
"subtract not add" direction. Lead greenlights each cluster before cutting.

**Cluster sizing:** ai_profile 15 · assets 25 · devapi 11 · product_gate 5 ·
game_context 2 · taskboard 3 · bootstrap 1 · top-level 5.

## Plan by cluster

- **KEEP (core, in use):** `ai_profile/*` (15 — telemetry, just overhauled),
  `taskboard/*` (3), `ai.mjs`, `pipeline_validate`, `skills_sync`,
  `state_codegen/*`, `devapi/{voxelheim_play_test,devapi_client,ui_readability,pixel_health}`.
- **PRIME CUT TARGET — `tools/assets` (25):** a bespoke-art generation/audit
  pipeline (`audit_*` ×8, `plan_*`, `build_ui_atlas_pack`, `build_runtime_assets_from_crop_plan`,
  `render_*_proof`, `normalize_source_sheet_chroma`, `run_ui_asset_tier`,
  `new_art_job`, `validate_art_job`, …) for a bespoke art workflow the game does
  NOT use (it ships free + agy-generated assets). Keep the few actually used
  (`chroma_key_alpha` — icon cutout). Estimated cut/consolidate ~15-18.
- **CONSOLIDATE — `devapi` capture/probe overlap (~6):** `capture_demo`,
  `capture_window`, `bot_demo`, `agent_playtest`, `shoot_size` (mine, this session),
  `shoot_voxelheim`, `full_probe`, `smoke_test` → keep one capture path + the
  play_test probe. Fold `shoot_size`'s `--window-size` into the kept capture tool.
- **REVIEW — `product_gate` (5), `game_context` (2), `skills_eval`, `tmp_sweep`.**

## Safety / sequencing (IMPORTANT)

- **A parallel Codex agent is ACTIVELY reworking the game** (src/voxelheim_main.c,
  balance.json, gdd, product_read_gate/rescue reviews) and may use the asset +
  devapi tooling RIGHT NOW. Do NOT cut assets/devapi until that workstream
  settles — coordinate or wait. Safe-now cuts are limited to clearly-dead,
  unreferenced, non-game tools.
- Verify each script's references (docs, `.vscode/tasks.json`, skills, other
  tools, `ai.mjs`) before removing; remove from any wiring too.

## Done when

- [ ] Lead greenlights the per-cluster cut list
- [ ] `tools/assets` reduced to the actually-used set (with refs/docs updated)
- [ ] devapi capture/probe overlap consolidated to one path
- [ ] tool surface roughly halved without breaking taskboard/profiler/state/probe/IDE tasks

## Open questions

- Which asset-pipeline scripts (if any) is the Codex game-rework currently using?
- Keep `shoot_voxelheim` or `shoot_size` as the single capture tool?

## Log

- 2026-06-17 Created from profiler usage data (68 scripts on disk, 9 used in 2 days).
  Deferred actual cuts: Codex is mid-game-rework in the asset/devapi tooling;
  cutting now would break its active work. Plan ready for lead greenlight.
