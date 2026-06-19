---
id: T0008
title: "PreToolUse hook: block edits to validators/budget config + evidence deletion"
status: backlog
epic: E001
priority: P3
tags: [pipeline, hooks]
created: 2026-06-19
updated: 2026-06-19
---

## What

Add a tiny PreToolUse guard hook (on the existing hook substrate that already
runs hook_record_fast) that refuses edits to `tools/context_budget_config.mjs`
and the product_gate validators, and deletion of evidence/screenshot files,
returning exit 2 + reason so an agent cannot silently silence a failing rule.
Advisory elsewhere. Borrowed: indie PreToolUse config-protection. Pairs with the
"guard is spoofable" finding.

## Done when

- [ ] editing a protected validator/config or deleting evidence is blocked with a reason
- [ ] .claude/settings.json and .codex/hooks.json stay in sync (hook config test passes)

## Open questions

## Log
