---
id: T0010
title: "Pipeline subtract: consolidate the 68-script tool surface (only 9 used in 2 active days)"
status: done
epic: E001
priority: P2
tags: [pipeline, subtract, lean, cleanup]
created: 2026-06-17
updated: 2026-06-17
---

## Conclusion (2026-06-17): surface is NOT bloated — checking Codex corrected it

The profiler-alone "9 used / 68" read was MISLEADING (it only saw 2 days of one
claude session). Cross-referencing the **Codex rollout transcripts**
(`~/.codex/sessions/.../rollout-*.jsonl`, 26 sessions) + all profiler logs shows
**40 tool scripts actively invoked**, and of the 43 not-directly-invoked, **41 are
wired** (ai.mjs subcommand backends like `status.mjs`/`checkpoint.mjs`; the
README-documented asset pipeline, several of which Codex runs heavily —
`audit_generated_ui_assets` 31x, `chroma_key_alpha`, `build_runtime_assets_from_crop_plan`;
`taskboard/lib.mjs` 17 refs; `.vscode/tasks.json` probes).

**Only 2 scripts were truly dead** (unused AND unreferenced): `devapi/agent_playtest.py`
and `devapi/shoot_size.py` (the latter added this session for aspect-testing,
redundant with `shoot_voxelheim` now the game is frozen). **Both deleted.**

**No safe merge/consolidation** beyond that: the capture tools are wired into the
IDE tasks, the asset pipeline is documented and Codex-active. Cutting would break
live work. -> The "big subtract" was a false alarm; closing this.

Lesson (evidence-or-gap): never conclude "bloated / dead" from one tool's partial
view — check ALL agents' actual usage (codex rollouts) first.

---

## (original framing) What

Profiler data (2026-06-16 + 06-17, both active days) showed **68 non-test tool
scripts on disk, only ~9 invoked**
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
- 2026-06-17: Investigated via Codex rollouts: surface NOT bloated (40 used + 41 wired of 68); only 2 truly dead -> deleted agent_playtest.py + shoot_size.py. No safe merges (capture=IDE-wired, assets=documented+codex-active). Checking codex corrected the premature 'big subtract' conclusion.
