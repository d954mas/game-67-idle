---
name: game-feature-iteration
description: Use when implementing, changing, debugging, or validating a playable game feature in an existing game repository. Triggers include gameplay mechanics, controls, cameras, UI flows, game state, progression, balance-affecting code, engine integration, and requests to make a small playable prototype or vertical slice. Works across engines by discovering local build/run/test conventions first.
---

# Game Feature Iteration

Use this skill to make small, playable game changes without losing project context.

## Workflow

1. Read local project rules first: `AGENTS.md`, project state/runbooks if present, then relevant design docs, build presets, and nearby code.
2. Identify the smallest playable slice that satisfies the request.
3. For non-trivial work, use the iteration cycle in `references/iteration-cycle-playbook.md`.
4. Keep implementation close to existing engine and game patterns.
5. Avoid broad refactors unless the feature cannot be implemented safely without them.
6. Validate the primary runtime target first; validate secondary targets only when relevant or requested.
7. Capture evidence and report what changed, where to run it, and what was verified.

## Discovery

Prefer local source-of-truth files over assumptions:

- Project rules: `AGENTS.md`
- Design docs: `gamedesign/`, `docs/design/`, `GDD.md`, or equivalent local folder
- Build/run: `CMakePresets.json`, `.vscode/tasks.json`, package scripts, engine docs
- Existing examples: `examples/`, `samples/`, nearby features, engine submodules

If naming differs, infer the equivalent directories from the repository.

## Implementation Rules

- Make one coherent gameplay increment at a time.
- Write or infer a short task packet for work that spans design, code, visuals, or validation.
- Keep code agent-readable: clear names, small functions, limited comments.
- Preserve engine boundaries; do not edit submodules or vendored engine code unless explicitly requested.
- Do not silently wire asset-pack generation into every normal game build.
- When adding state, input, or rendering, include a simple way for the user to observe the behavior.
- Keep reusable workflow in skills; keep project-specific facts and run commands in project docs.

## Validation

Use the project primary target from `AGENTS.md` or local docs. If none is defined, prefer the fastest native desktop/dev target.

For visual or interaction changes, run or inspect the game when possible and capture evidence in the project scratch area if one exists.

If the project exposes an agent playtest harness or runtime runbook, use it before ad hoc checks.

## References

- `references/iteration-cycle-playbook.md`: director/developer/designer/tester iteration loop, task packet, evidence, review, state update, and report format.
