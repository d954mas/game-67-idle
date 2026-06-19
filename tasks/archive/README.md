# Task Archive Notes

Historical evidence and cleanup notes that should not live in the hot
`tasks/STATUS.md` index.

## AI Pipeline Evidence Snapshot

Current source-of-truth docs are `AGENTS.md`, `AI_PIPELINE.md`,
`tasks/README.md`, and `.codex/skills/`.

Pipeline cleanup evidence moved out of `tasks/STATUS.md`:

- Backrooms runtime/perf evidence remains in `build/captures/` and
  `gamedesign/projects/backrooms-liminal/reviews/`.
- Engine render-target issue evidence remains in
  `tasks/active/T0011-engine-render-target-api-for-portal-rendering.md`.
- Profiling hot hook writes are serialized with a short file lock and covered by
  a parallel JSONL append regression test, so concurrent validations do not
  corrupt `tmp/session_profiles/sessions/*.jsonl`.
- `tools/context_budget.mjs` is part of quick pipeline validation and guards
  hot docs plus `.codex/skills/*/SKILL.md` entrypoints from silently growing
  past the current progressive-disclosure budget.
- Full pipeline validation chooses a Python runner by required asset-test
  modules (`PIL`, `numpy`, `scipy`, `pymatting`) and supports explicit
  `AI_PIPELINE_PYTHON` commands with args.
- `task-manager`, `generated-game-ui-assets`, `game-visual-art-direction`,
  `game-feature-iteration`, `game-asset-pipeline`, `primary-gdd-pipeline`,
  `delegated-image-generation`, `game-state-management`,
  `game-runtime-automation`, `chat-session-reflection`, and
  `design-source-knowledge` keep hot `SKILL.md` files as short routers and load
  detailed procedures from `.codex/skills/*/references/`.
- `tools/skills_sync.mjs --check` fails on stale generated `.claude` skill
  pointers; normal sync removes generated orphans and preserves hand-written
  `.claude` skills.
