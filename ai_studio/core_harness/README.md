# Core Harness

`core_harness/` is the first reviewed migration target inside `ai_studio/`.
It owns the smallest always-loaded harness contract:

- agent entry policy;
- root agent-facing compatibility;
- compatibility rules for root agent-facing surfaces;
- the minimal public commands that let an agent start work.

Files move here only after review. The tree lists candidates so each file can be
inspected, cleaned, and migrated deliberately.

## Current Compatibility Surface

These files currently stay at the repo root because external harnesses expect
them there:

- `AGENTS.md`
- `AI_PIPELINE.md`
- `CLAUDE.md`

When their content is migrated, the root files should become thin routing
facades.

## Tree Shape

During the Core audit, `Core Harness` is intentionally ungrouped in the map:
every current core doc, tool, and candidate is a direct child. Grouping should
be added only after ownership, boundaries, and compatibility rules are verified.
