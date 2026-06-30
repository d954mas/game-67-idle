# Iteration Cycle

Load this when a game task is broader than a single code edit and must move the
playable product forward.

## Goal

Ship one verified improvement to the game loop, UX, visuals, balance, content,
or stability while preserving enough evidence for the next iteration.

## Cycle

```text
orient -> choose goal -> packet -> implement -> validate -> evidence -> review -> update state -> commit
```

## Orient

Read only what is needed:

- root rules already loaded;
- `GAME_PROJECT.md` for current-game routing;
- `ai_studio/game_project/iteration_context.mjs` output when useful;
- relevant design/source files;
- nearby code;
- latest evidence/logs when following a failed validation.

Do not restart broad research when local source-of-truth files already answer
the question.

## Choose One Playable Goal

Good goals:

- improve the first visible action;
- make one reward readable;
- add one upgrade with visible effect;
- fix one blocking bug;
- replace one placeholder visual that hurts comprehension;
- make one validation path reliable.

Bad goals:

- polish everything;
- add the whole meta;
- make the game ready;
- refactor first, then gameplay;
- create all assets before one slice works.

## Task Packet

Use this format for non-trivial implementation:

```markdown
### Iteration Goal
[One player-visible improvement.]

### Scope
[What will change.]

### Out Of Scope
[What will not be touched this pass.]

### Implementation
[Concrete code/data/content tasks.]

### Proof
[Commands, screenshots, logs, runtime reports, or manual checks.]

### Done
[Observable criteria.]
```

If the packet has too many bullets, cut scope.

## Implement

- Follow existing engine and project patterns.
- Avoid unrelated refactors.
- Preserve engine, submodule, vendor, and generated-file boundaries.
- Add observability only when it reduces future debugging time.
- Do not replace product validation with compile-only success.

## Validate

Use the fastest reliable primary target from the project rules. Evidence should
include at least one of:

- passing test or smoke command;
- screenshot or recording;
- saved report;
- launch/runtime log path;
- before/after state summary;
- manual playtest notes.

When validation fails:

1. read the error;
2. inspect logs/evidence;
3. fix the smallest blocker;
4. rerun the same validation.

## Review

Review as a product:

- core loop still works;
- first 30 seconds are clearer or at least not worse;
- feedback is readable;
- UI is not more cluttered;
- visual tone matches the audience;
- balance did not create stuck states;
- no critical runtime errors are visible;
- evidence supports the claim.

## Durable State

Update durable state only when it helps the next agent:

- task log for work/proof/review outcomes;
- project design docs when a game-specific decision changes;
- GDD/handoff when implementation source-of-truth changes;
- reusable knowledge only when a reusable principle was learned.

Do not store current-game facts in reusable knowledge.

## Commit Boundary

Good boundaries:

- one gameplay slice;
- one tooling improvement;
- one documentation/process update;
- one asset/content batch with references.

Stage only intentional files. If a normal slice changes too many files, split by
phase unless the user explicitly asked for a snapshot.
