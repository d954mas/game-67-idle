@AGENTS.md

Enforcement labels and proof routes are defined in
`ai_studio/core_harness/workflow/enforcement_contract.json`.

## Orchestration workflow

Fable (the main loop) is the orchestrator: plan, decompose, synthesize, keep
its own context lean. Always delegate with an explicit model — never let a
subagent inherit Fable. Subagents execute their packet and return; they do
not re-delegate.

- Reasoning-heavy phases (architecture, complex debugging, algorithm design,
  research synthesis) → deep-reasoner (Opus).
- Mechanical work (boilerplate, tests, formatting, simple edits) →
  fast-worker (Sonnet).

High-stakes decisions: run two independent deep-reasoner (Opus) instances on
the same problem in parallel, each prompted from a different angle, then
synthesize the best of both without showing either the other's answer.
