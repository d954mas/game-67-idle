# AI Studio

`ai_studio/` is the target home for reviewed and refactored AI game-studio
pipeline modules.

This folder is not a dump of the current repo. Current files stay where they
are until a module is inspected, given an owner, cleaned up, and migrated with
compatibility shims where needed.

The architecture map uses `ai_studio/` as the target folder, but does not show it
as a visible graph node. The visible top level currently contains only Core
Harness plus a `Not Refactored` backlog node that preserves everything else from
the current repo until it is reviewed and promoted.

## Rule

Only move something here when it has:

- a clear domain owner;
- a source-of-truth README or contract;
- a public surface agents should use;
- known internal helpers;
- a validation path or an explicit reason why no validator applies.

## Domains

| Domain | Owns | Current sources before migration |
|---|---|---|
| `core/` | Harness contract, agent entry policy, root routing, minimal public surface. | `AGENTS.md`, `AI_PIPELINE.md`, `CLAUDE.md`, `docs/ai-pipeline/` |
| `agents/` | Lead/worker roles, delegation packets, agent protocols, handoff rules. | `docs/ai-pipeline/agent-workflow.md`, `docs/ai-pipeline/subagent-protocol.md`, taskboard packet presets |
| `skills/` | Canonical reusable agent procedures and skill references. | `.codex/skills/`, generated `.claude/skills/` |
| `tools/` | Stable AI-studio command facades and domain tool contracts. | `tools/ai.mjs`, `tools/pipeline_validate.mjs`, `tools/*` |
| `tasks/` | Durable work state, orchestration, status, evidence routing. | `tasks/`, `tools/taskboard/` |
| `assets/` | Source-first asset library workflow, provenance, intake, generated asset records. | `tools/assets/`, `tools/asset_review/`, asset skills |
| `design/` | GDD pipeline, reusable design knowledge, source notes, project wiki routing. | `gamedesign/`, `tools/game_context/` |
| `tech/` | Runtime automation, state codegen, template integration, engine-facing adapters. | `template/`, `tools/devapi/`, `tools/state_codegen/`, `external/neotolis-engine/` |
| `validation/` | Quality gates, product gates, budget/doc guards, repeated-failure stops. | `tools/product_gate/`, `tools/context_budget.mjs`, `tools/doc_reference_check.mjs` |
| `export/` | Portable export, harness sync, generated compatibility surfaces. | `tools/bootstrap/`, `tools/sync.mjs`, `tools/skills_sync.mjs`, `tools/hooks_sync.mjs` |
| `migration/` | Refactor queue, move records, compatibility decisions. | `docs/ai-pipeline/architecture-map.md`, generated architecture maps |

## Migration Loop

1. Pick one current module from the architecture map.
2. Decide whether it belongs in `ai_studio/`, remains external, or should be
   deleted.
3. Create or update the target domain README.
4. Move only reviewed source files, keeping old public commands as shims until
   every caller is updated.
5. Rebuild the architecture maps and run focused validators.
