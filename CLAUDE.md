@AGENTS.md

Enforcement labels and proof routes are defined in
`ai_studio/core_harness/workflow/enforcement_contract.json`.

## Claude orchestration overlay

When delegation is chosen under `AGENTS.md`, select an explicit role/model; do
not let a subagent inherit Fable. Subagents execute one bounded packet and do
not re-delegate.

- Reasoning, research, or adversarial review: deep-reasoner (Opus).
- Mechanical already-decided work: fast-worker (Sonnet).
