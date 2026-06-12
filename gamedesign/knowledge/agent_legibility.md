# Agent Legibility And Feedback Loops

## Goal

Keep the project legible to AI agents: what an agent cannot inspect,
retrieve, validate, or act on through tools is operationally absent from its
world. Humans steer (priorities, acceptance criteria, taste, boundaries);
agents execute, gather evidence, and surface judgment calls.

## Checklist

- Encode durable knowledge into versioned project files, not chat history,
  meetings, or heads.
- Keep the top-level instruction file a short map that points to deeper
  references loaded only when needed.
- For every workflow, define the signals that prove progress (build passes,
  scenario exits zero, screenshot shows the change) so the agent can validate
  without a human copying data into the prompt.
- When an agent fails, name the missing component before rewriting prompts:
  missing instruction, source of truth, tool, validator, permission rule,
  eval, or recovery path. Encode the fix there so the improvement compounds.
- Convert recurring guidance into mechanical checks: schema validators, lint
  rules, structural tests, freshness checks, regression evals. Give validators
  remediation messages safe to show the model.
- Tier validation by risk: automated checks and sampling for low risk,
  targeted human review for medium, explicit approval for destructive or
  direction-changing work.
- Run recurring garbage collection: stale docs, repeated tool failures,
  low-quality examples agents imitate, unused tools/skills, duplicate
  instructions, obsolete workflows.

## Anti-Patterns

- Fixing every agent failure by adding more prompt text.
- An encyclopedia-sized instruction file loaded in every session.
- Validators that prove consistency while the real question is quality or
  playability.
- Letting weak examples and dead rules accumulate; agents replicate existing
  patterns, including bad ones.
- Knowledge that exists only in a past conversation.

## Validation

- New rules should change a real decision, review finding, or validation
  step; otherwise do not add them.
- After each substantial failure, check that something outside the prompt
  changed: a doc, a tool, a validator, an eval, or a recovery path.

## References

- Full playbook: user-level skill `agents-best-practices`
  (`references/agent-legibility-feedback-loops.md`).
