# Iteration Cycle

Project-specific adapter for the reusable `game-feature-iteration` cycle.

Use this for Game 67 gameplay, UI, visual, balance, content, runtime, and release-readiness iterations.

## Default Loop

1. Read `AGENTS.md`.
2. Read `agent_docs/project_state.md`.
3. Read `agent_docs/playtest_harness.md` for runtime validation.
4. Read only the relevant GDD/knowledge/source files.
5. Choose one small playable goal.
6. Define scope and out-of-scope.
7. Implement.
8. Run the fastest relevant validation.
9. Capture evidence.
10. Do director review.
11. Update durable docs only when useful.
12. Commit intentional files with path-limited staging.

## Default Validation

Native desktop is the default target.

```powershell
py -3.12 tools\devapi\agent_playtest.py 9123 --full-loop
```

Use deeper checks only when relevant:

```powershell
py -3.12 tools\devapi\smoke_test.py 9123
py -3.12 tools\devapi\full_probe.py 9123
py -3.12 tools\devapi\capture_demo.py 9123 build\captures\manual_check.png --full-loop
```

Do not run WASM/web unless the user asks for it or the task is specifically about web/WASM behavior.

## Task Packet

```md
### Iteration Goal
[One player-visible improvement.]

### Scope
[What changes this pass.]

### Out Of Scope
[What is deliberately deferred.]

### Developer
[Mechanics, state, UI integration, scripts, build work.]

### Designer
[Assets, layout, icons, effects, readability, visual tone.]

### Tester
[Commands, screenshots, logs, playtest checks, bugs to look for.]

### Done
[Observable criteria and evidence paths.]
```

## Report Format

```md
## Iteration N

### Goal
[What improved.]

### Done
[What changed.]

### Developer
[Implementation summary.]

### Designer
[Visual/UI/feedback summary.]

### Tester
[What was validated, evidence paths, bugs found.]

### Director Review
[What is better, what is still weak, release risk.]

### Next Priorities
[Concrete next steps.]
```

## Durable State Rules

- Update `agent_docs/project_state.md` after meaningful verified iterations, target changes, or newly discovered repo traps.
- Update GDD files when a Game 67 design decision changes.
- Update `gamedesing/knowledge/` only for reusable design knowledge, not current state.
- Update `.codex/skills/` only for reusable workflow/tool rules.
- Keep evidence under ignored build/capture/log folders.

## Commit Rules

- Use path-limited staging.
- Do not stage unrelated work.
- Avoid broad `git diff --stat` if it triggers submodule/LFS friction; use path-limited diffs.
- Mention validation in the final response.
