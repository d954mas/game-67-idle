---
id: T0006
title: Skill sync generator (.codex -> .claude thin pointers)
status: done
epic: E002
priority: P1
tags: [ai-pipeline, tooling]
created: 2026-06-11
updated: 2026-06-11
---

## What

Generate .claude/skills/* thin pointers from canonical .codex/skills/* so
Claude Code discovers the same skills without duplicating content.

## Done when

- [x] generator exists (tools/skills_sync.mjs) and is idempotent
- [x] all 9 skills mirrored with frontmatter description preserved
- [x] hand-written .claude skills are never overwritten (marker check)

## Open questions

## Log

- 2026-06-11: Done. Evidence: `node tools/skills_sync.mjs` -> "9 generated, 0 skipped"; sample verified (.claude/skills/game-state-management/SKILL.md).
