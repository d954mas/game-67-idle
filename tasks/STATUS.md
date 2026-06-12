# Project Status

Short live project-status index. Workflow rules live in `tasks/README.md`.

## Current Goal

Prepare this repository itself as a clean base for the user's next game idea.

Source: current user request.

## Active Work

No game concept is active yet. Do not invent one.

The project is intentionally clean:

- old game/task history is not part of the current context;
- `tasks/` is still the single source of truth for future work items;
- DevAPI, state codegen, save/load, migrations, and capture tooling are kept as
  reusable AI/runtime infrastructure;
- new work starts only after the user describes the next game or explicitly
  asks for a pipeline change.

## Current Gate

Wait for the user's new game concept. When it arrives, translate the natural
language request into one explicit scope, say what will be done, ask if
ambiguous, then create only the needed current task/epic.

Do not remove `state/`, `tools/state_codegen/`, `src/devapi/`, `tools/devapi/`,
`src/game_storage.*`, or `external/cjson/`; they are the reusable runtime seed.

## Required Validation

```powershell
node tools/taskboard/cli.mjs list
node tools/pipeline_validate.mjs
cmake --preset native-debug
cmake --build --preset native-debug
```

## Last Known Good Evidence

Clean seed runtime evidence:

- `node tools/taskboard/cli.mjs validate`
- `node tools/skills_eval.mjs`
- `node tools/pipeline_validate.mjs`
- `py -3.12 tools/state_codegen/generate_state.py`
- `cmake --preset native-debug`
- `cmake --build --preset native-debug`
- `py -3.12 tools/devapi/smoke_test.py 9123`
- `py -3.12 tools/devapi/full_probe.py 9123`
- `py -3.12 tools/devapi/scenarios/state_roundtrip.py 9124`
- `py -3.12 tools/devapi/scenarios/settings_modal.py 9125`
- `py -3.12 tools/devapi/scenarios/ui_button_text.py 9126`

All passed after the cleanup.

## Blocking Work

None.

## Non-blocking Debt

None recorded.

## Next Priorities

1. Wait for the user's next game concept.
2. Turn the concept into one scoped current task or epic.
3. Keep the clean seed runtime and AI pipeline docs short and current.
