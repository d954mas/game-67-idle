@AGENTS.md

Mechanical gates and the advisory boundary are summarized in
`ai_studio/core_harness/workflow/README.md#enforcement-boundary`.

## Claude orchestration overlay

When delegation is chosen under `AGENTS.md`, select an explicit role/model; do
not let a subagent inherit Fable. Subagents execute one bounded packet and do
not re-delegate.

- Reasoning, research, or adversarial review: deep-reasoner (Opus).
- Mechanical already-decided work: fast-worker (Sonnet).
