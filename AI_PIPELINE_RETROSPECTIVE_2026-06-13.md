# AI Pipeline Retrospective - 2026-06-13

Scope: long 67 World AI-assisted development session across concept work,
reference study, generated art, native gameplay, balance, release packaging,
and AI workflow rules.

## Summary

The game made substantial native progress, but the AI workflow became too
heavy. Tooling for profiling, validation, reflection, task status, reference
study, and release evidence started creating process obligations that competed
with the actual game work.

Current product truth remains simple: automated release/package checks can pass,
but the game is not release-ready until real manual child-test/user acceptance
is returned and any resulting fixes are applied.

## Main Problems

- The agent sometimes optimized for faster-looking paths instead of the native
  PC harness.
- Reference work was initially too shallow, then overcorrected into a bulky
  gate duplicated across docs.
- Visual work sometimes polished placeholders instead of moving through the art
  asset pipeline.
- `tasks/STATUS.md` became an evidence log instead of a short current index.
- Many completed tasks stayed in `review`, making current work look noisy.
- Profiling/reflection tools grew into a maintenance workflow.
- Broad validation was too easy to run repeatedly.

## Current Fixes

- Tool defaults are passive and advisory.
- Deep AI workflow artifacts require explicit `--deep`, `--verbose`,
  `--review`, `--all`, or `--include-final`.
- `tasks/STATUS.md` is compact again.
- Taskboard hides review tasks from normal list/context output.
- Profiling records slow/failing/large-gap signals by default instead of every
  small step.
- Validation planner defers broad/final checks unless explicitly requested.

## Next Rule

If AI tooling creates work that does not directly help answer “what should we
build, change, or verify next in the game?”, simplify the tool or move the
behavior behind an explicit deep mode.
