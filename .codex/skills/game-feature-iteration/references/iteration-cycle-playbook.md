# Game Iteration Cycle Playbook

Reusable workflow for fast AI-assisted game iterations.

Use this playbook when a game task is broader than a single code edit and must
move the playable product forward.

## Objective

Ship small verified improvements to the game loop, UX, visuals, balance, content,
or stability while preserving context and evidence for the next iteration.

## Roles

A single agent may perform all roles, but the responsibilities stay separate.

- Director: chooses priority, cuts scope, defines done, reviews player-facing quality.
- Developer: implements mechanics, UI, state, integration, build fixes, scripts.
- Designer: improves assets, layout, feedback, readability, visual consistency.
- Tester: runs the game, captures evidence, finds bugs, checks first-session clarity.

## Cycle

```text
Orient
  -> choose one small playable goal
  -> write task packet
  -> implement
  -> validate with the fastest reliable target
  -> capture evidence
  -> director review
  -> update durable state
  -> commit
  -> choose next priority
```

## 1. Orient

Read only what is needed:

- project rules;
- current project state;
- relevant design/source files;
- relevant knowledge or skill references;
- latest evidence/logs when the task follows a failed validation.

Do not restart from broad research when local source-of-truth files already
answer the question.

## 2. Choose One Small Playable Goal

Good goals:

- improve the first visible action;
- make one reward readable;
- add one upgrade with visible effect;
- fix one blocking bug;
- replace one placeholder visual that hurts comprehension;
- make one validation path reliable.

Bad goals:

- "polish everything";
- "add the whole meta";
- "make the game ready";
- "refactor first, then gameplay";
- "create all assets before one slice works."

## 3. Task Packet

Use this format before implementation when the work is non-trivial:

```md
### Iteration Goal
[One player-visible improvement.]

### Scope
[What will change.]

### Out Of Scope
[What will not be touched this pass.]

### Developer
[Concrete implementation tasks.]

### Designer
[Concrete visual/UI/feedback tasks.]

### Tester
[Concrete validation tasks and evidence.]

### Done
[Observable criteria, commands, screenshots, logs, or tests.]
```

Keep task packets short. If a packet has too many bullets, cut scope.

## 4. Implement

- Follow existing engine and project patterns.
- Avoid unrelated refactors.
- Keep reusable workflow in skills and project-specific state in project docs.
- Preserve engine/vendor boundaries.
- Add observability when it reduces future debugging time.
- Do not replace product validation with compile-only success.

## 5. Validate

Use the fastest reliable primary target from the project rules. If the project
has an agent playtest harness, run it before ad hoc checks.

Validation evidence should include at least one of:

- passing test/smoke command;
- screenshot or recording;
- saved report;
- launch/runtime log path;
- before/after state summary;
- manual playtest notes.

When validation fails:

1. read the error;
2. inspect logs if available;
3. inspect evidence;
4. fix the smallest blocker;
5. rerun the same validation.

## 6. Director Review

Review as a product, not only as code.

Check:

- core loop still works;
- first 30 seconds are clearer or at least not worse;
- feedback is readable;
- UI is not more cluttered;
- visual tone matches the audience;
- balance did not create stuck states;
- no critical runtime errors are visible;
- evidence supports the claim.

## 7. Update Durable State

Update durable state only when it helps the next agent:

- project state after meaningful verified iterations or target changes;
- GDD when a game-specific design decision changes;
- knowledge base only when a reusable principle was learned;
- skill only when a reusable workflow/tool rule changed;
- issue/backlog when work is intentionally deferred.

Do not store current-game facts in reusable knowledge.

## 8. Commit

Commit only intentional files. Prefer path-limited staging in repos with
submodules, generated files, or large assets.

Before commit or handoff for a prototype slice, record build evidence,
scenario/probe evidence, screenshot or recording evidence for player-facing
changes, and selected quality rule outcomes when quality rules applied.

If the normal slice changes more than 30 files, split by phase or rerun with
`--snapshot` only when the lead explicitly asked for an end-of-experiment
snapshot. If changed review/audit files still contain fail verdicts, refresh
them, archive them as historical evidence, or name the accepted debt in final
notes. Check push/upstream state before promising push.

Good commit boundaries:

- one gameplay slice;
- one tooling improvement;
- one documentation/process update;
- one asset/content batch with its references.

## Iteration Report

Use this report shape for game-director loops:

```md
## Iteration N

### Goal
[What improved.]

### Done
[What changed and where.]

### Developer
[Implementation summary.]

### Designer
[Visual/UI/feedback summary.]

### Tester
[Validation run, evidence paths, bugs found.]

### Director Review
[What is better, what is still weak, release risk.]

### Next Priorities
[1-5 concrete next steps.]
```

## Anti-Patterns

- Skipping orientation and rediscovering already documented decisions.
- Starting multiple large systems before one loop is playable.
- Running slow secondary-platform checks before primary-target gameplay is ready.
- Accepting "works technically" when the first player action is unclear.
- Creating project docs that duplicate reusable skill guidance.
- Creating reusable skill guidance that contains current project state.
- Ending without evidence paths, known issues, or next priorities.
- Committing unrelated staged changes because they were already present.

## Quick Checklist

- One playable goal chosen.
- Scope cut is explicit.
- Primary target validated.
- Evidence captured.
- Logs inspected on failure.
- Product review done.
- Durable state updated if useful.
- Intentional commit made.
