# Task Archive Notes

Historical evidence and cleanup notes that should not live in hot routing docs
or active task files.

## AI Pipeline Evidence Snapshot

Current source-of-truth docs are `AGENTS.md`, `ai_studio/README.md`,
`ai_studio/taskboard/README.md`, and `.codex/skills/`.

Pipeline cleanup evidence preserved from earlier taskboard/status resets:

- Closed game/runtime evidence is preserved in git snapshots and should not
  remain in the hot taskboard after a reset-to-seed.
- Profiling hot hook writes are serialized with a short file lock and covered by
  a parallel JSONL append regression test, so concurrent validations do not
  corrupt `tmp/session_profiles/sessions/*.jsonl`.
- Hot-doc budget checks and full pipeline validation were retired from active
  routing. Prefer focused module validators and direct module tests.
- `task-manager`, `generated-game-ui-assets`, `game-visual-art-direction`,
  `game-feature-iteration`, `nt-asset-workflow`, `primary-gdd-pipeline`,
  `delegated-image-generation`, `game-state-management`,
  `game-runtime-automation`, `chat-session-reflection`, and
  `design-source-knowledge` keep hot `SKILL.md` files as short routers and load
  detailed procedures from `.codex/skills/*/references/`.
- `ai_studio/core_harness/agent_surfaces/sync.mjs --check` fails on stale generated `.claude` skill
  pointers; normal sync removes generated orphans and preserves hand-written
  `.claude` skills.
