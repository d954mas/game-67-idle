# Technical Rules

Use this group when changed work affects code, scripts, runtime behavior,
build/run proof, input, state, save/load, packaging, or automation.

## Order

1. Start with [COMMON.md](COMMON.md).
2. Add numbered checks only when their "Use When" section matches the task.

## Checks

### [QTECH_COMMON - Technical Common](COMMON.md)
Checks obvious technical proof blockers: no narrow validation command, command
does not exercise the change, ignored failing logs, generated data does not
parse/load, or technical green is overclaimed as player-facing quality.

Use first for any code, script, runtime behavior, build, launch, state,
save/load, input, packaging, or automation change.

### [QTECH_001 - Runtime Evidence](checks/QTECH_001_runtime_evidence.md)
Checks whether the changed runtime/system behavior has the narrowest proof that
actually exercises it.

Use when code, build, launch, runtime behavior, input, state, save/load,
packaging, or runtime automation changed.

Record applied checks in the task log as `Quality: QTECH_001=pass` or
`Quality: QTECH_001=block`.
