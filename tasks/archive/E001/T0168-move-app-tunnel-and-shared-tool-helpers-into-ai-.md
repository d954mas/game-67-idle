---
id: T0168
title: Move app tunnel and shared tool helpers into AI Studio
status: done
epic: E001
priority: P2
tags: [core-harness, studio-shell, skill, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What

Move the small public tunnel helper and shared one-shot tool helpers out of
legacy `tools/` into reviewed AI Studio ownership.

Target ownership:

- `ai_studio/core_harness/tool_lib/`: shared low-level CLI, JSON, hash, MIME,
  path, and tmp export helpers used by AI Studio modules.
- `ai_studio/studio_shell/tunnel/`: public quick tunnel for exposing a local
  self-contained surface to another device.
- `nt-app-tunnel`: reviewed skill surface for the tunnel workflow.

This is a behavior-preserving move. Do not redesign cloudflared behavior in this
slice.

## Done when

- [x] `tools/lib/*` no longer owns current AI Studio helper code.
- [x] `tools/serve_tunnel.mjs` is moved under `ai_studio/studio_shell/tunnel/`.
- [x] Current imports and bootstrap export paths use the new AI Studio paths.
- [x] `app-tunnel` skill is migrated to reviewed `nt-app-tunnel`.
- [x] Architecture map owns the moved files and no longer reports these files as
      unmapped legacy.
- [x] Existing JS tests, skill validation, map validation, and taskboard
      validation pass.

## Open questions

- None yet.

## Review

### Moved

- `tools/lib/*` -> `ai_studio/core_harness/tool_lib/`.
- `tools/serve_tunnel.mjs` ->
  `ai_studio/studio_shell/tunnel/serve_tunnel.mjs`.
- `.codex/skills/app-tunnel` -> `.codex/skills/nt-app-tunnel`.

### Ownership

- `core_harness/tool_lib` now owns policy-free shared helpers for AI Studio CLI
  tools: `cli`, `json`, `hash`, `mime`, `paths`, and `tmp_exports`.
- `studio_shell/tunnel` now owns the temporary public quick tunnel because it is
  a shell/surface sharing helper, not a game module and not generic root tools.
- `nt-app-tunnel` is the reviewed skill surface. `.claude/skills/app-tunnel` was
  removed by skill sync and `.claude/skills/nt-app-tunnel` was generated.

### Behavior Notes

- Tunnel behavior stayed the same: serve a self-contained directory, run a
  cloudflared quick tunnel, print `TUNNEL_URL`.
- Downloaded `cloudflared` cache moved from `tools/bin/` to
  `.tmp/ai_studio/studio_shell/tunnel/bin/`, which is already ignored by git.
- `tools/README.md` and `tools/requirements/ai-pipeline-full.txt` remain legacy
  candidates for later review; they were not part of this move.
- Remaining map legacy is now mostly old game-design/state/prototype skills,
  old epics, `tools/state_codegen`, and `tools/tmp_sweep`.

### Validation

- `node --test ai_studio/core_harness/tool_lib/*.test.mjs
  ai_studio/assets/**/*.test.mjs ai_studio/taskboard/**/*.test.mjs`: 150 passed.
- `py -3.12 .../quick_validate.py .codex/skills/nt-app-tunnel`: valid.
- `node ai_studio/core_harness/agent_surfaces/skills_sync.mjs --check`: ok.
- `node ai_studio/architecture_map/validate_map.mjs`: ok, missing=0,
  unmapped_ai_studio=0, unmapped_legacy=40.
- `node ai_studio/core_harness/validation/doc_reference_check.mjs`: ok.
- `node ai_studio/taskboard/cli.mjs validate --json`: ok.
- `node ai_studio/studio_shell/tunnel/serve_tunnel.mjs --dir
  tmp/definitely-not-a-tunnel-dir`: imports and exits with expected missing-dir
  error before any network/download step.

## Log

- 2026-07-01: Created after the map showed `app-tunnel`,
  `tools/serve_tunnel.mjs`, and `tools/lib/*` as unmapped legacy even though
  current AI Studio modules still depend on them.
- 2026-07-01: Moved tool helpers and tunnel into AI Studio ownership, migrated
  the skill to `nt-app-tunnel`, updated imports/export paths, and validated the
  affected modules.
- 2026-06-30: Moved app tunnel and shared tool helpers into AI Studio ownership; nt-app-tunnel and map entries validated.
