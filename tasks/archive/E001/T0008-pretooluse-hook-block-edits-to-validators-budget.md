---
id: T0008
title: "PreToolUse hook: block edits to validators/budget config + evidence deletion"
status: dropped
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

- 2026-06-19: DROPPED on principle. A PreToolUse node guard runs a node process
  on EVERY tool call (Edit/Write/Bash) — re-introducing exactly the per-call node
  startup overhead the C fast-path (hook_record_fast.c) exists to avoid — for
  marginal value on a solo trusted-lead workflow. Config-protection, if wanted,
  belongs in a one-shot validator or a git pre-commit hook, not a per-call
  interceptor. The spoofable-guard concern (T0003) is better met by honest gates.
