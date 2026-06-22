---
id: T0053
title: Add subagent packet tool-use guardrails
status: done
epic: ""
priority: P2
tags: [pipeline, profiling, orchestration, subagents]
created: 2026-06-21
updated: 2026-06-22
---

## What

Subagent packets currently capture objective, files, evidence, and reviewer, but
do not carry the concrete tool-use guardrails surfaced by agent-rollup. Add a
preflight `tool-use guard` line to the reusable orchestration packet/template so
future subagents get path discovery, PowerShell range, and trace-evidence rules
before running commands.

## Done when

- [x] `node tools/taskboard/cli.mjs orchestration-template` includes a concise
      `tool-use guard` line.
- [x] The packet template remains accepted by existing taskboard validation
      without making old packets invalid.
- [x] `docs/ai-pipeline/subagent-protocol.md` documents the guard in the packet
      schema.
- [x] Tests cover the new template text.
- [x] Independent reviewer confirms optional-vs-mandatory placement and risks.
- [x] Focused tests and review validation pass.

## Open questions

## Log

- orchestration: used
  objective: add subagent packet tool-use guardrails before commands run
  allowed files: tools/taskboard/lib.mjs, tools/taskboard/test.mjs,
  docs/ai-pipeline/subagent-protocol.md,
  tasks/active/T0053-add-subagent-packet-tool-use-guardrails.md
  expected output: orchestration template and protocol packet schema include
  tool-use guardrails for paths, PowerShell ranges, and trace evidence sources
  evidence command: node --test tools/taskboard/test.mjs; node
  tools/taskboard/cli.mjs orchestration-template; node tools/ai.mjs status
  --agent-rollup --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose; node
  tools/taskboard/cli.mjs validate; node tools/ai.mjs validate --review
  stop condition: template includes preflight guardrails and validation remains
  backward-compatible
  independent reviewer: Newton audits optional-vs-mandatory placement and risks
- reviewer: PASS Newton confirmed `tool-use guard` should be template-default
  and validator-optional; recommended documenting the closeout example without
  adding duplicate hot-doc text to `docs/ai-pipeline/agent-workflow.md`.
- evidence: PASS `node --test tools/taskboard/test.mjs`; output showed 58
  tests passed.
- evidence: PASS `node tools/taskboard/cli.mjs orchestration-template`; output
  included `tool-use guard`.
- evidence: PASS `node tools/ai.mjs status --agent-rollup
  --require-agent-rollup-ok --parent-thread-id
  019ee5cc-1180-7eb3-b976-c0d90d5ac0dd --session-root
  C:\Users\ROG\.codex\sessions\2026\06\21 --agent-cwd
  C:\projects\game-67-idle --no-import-codex-session --verbose`; output showed
  `subagent sessions: 40`, `parent-recovered agent failures: 6`, and
  `agent tool-usage failures: 8` with prevention hints matching the new guard.
- evidence: PASS `node tools/taskboard/cli.mjs validate`; output showed no
  problems found.
- evidence: PASS `node tools/ai.mjs validate --review`; output showed reusable
  pipeline quick+review validation passed.
