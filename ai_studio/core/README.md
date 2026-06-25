# Core

Core owns the smallest always-loaded harness contract:

- agent entry policy;
- root routing;
- compatibility rules for `AGENTS.md`, `AI_PIPELINE.md`, and generated harness
  surfaces;
- the minimal public commands that let an agent start work.

Core must stay small. Domain procedure belongs in owned folders such as
`assets/`, `tech/`, `design/`, `tasks/`, or `validation/`.

## Current Compatibility Surface

These files currently stay at the repo root because external harnesses expect
them there:

- `AGENTS.md`
- `AI_PIPELINE.md`
- `CLAUDE.md`

When their content is migrated, the root files should become thin routing
facades that point into `ai_studio/core/`.

## Refactor Map Shape

During the Core audit, the `Core Harness` node is intentionally ungrouped in
the map: every current core doc, tool, and contract candidate is a direct child.
Grouping should be added only after ownership, boundaries, and compatibility
rules are verified.
