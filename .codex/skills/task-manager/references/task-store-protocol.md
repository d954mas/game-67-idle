# Task Store Protocol

Authoritative store and live-status rules live in `tasks/README.md`; the live
status index is `tasks/STATUS.md`.

## Orientation

- Start quick orientation with `node tools/taskboard/cli.mjs summary`.
- Use `node tools/taskboard/cli.mjs context` for long work.
- Read full `tasks/STATUS.md` only when changing it or auditing a specific
  linked claim.
- `list` shows current work only; use `list --review` only for review cleanup.

## State Rules

- Capture deferred work as `status: idea`.
- Refine `idea` work before implementation: answer user questions, add
  researched context, and write checkable `## Done when` before moving to
  `backlog`.
- A `backlog` task must state what visible change proves it done, what is out of
  scope, and which docs/data/skills it touches. Otherwise it stays `idea` with
  `## Open questions` for the user.
- Decompose large requests into an epic plus tasks each completable in one
  focused session; scope boundaries live in the epic's in/out-of-scope.
- Mark `doing` on start, `review` when awaiting feedback, and `done` only with
  ticked criteria plus an evidence line in `## Log`.

## Review Queue

Treat `review` as a separate acceptance/cleanup queue, not default current work.
Do not inspect the review queue during normal game implementation unless the
user asks to review/close old tasks or the current decision depends on a
specific review item.

## Prototype Closeout

When the user says a prototype/game was only a test run or should stop, stop
implementation first and follow the latest explicit instruction for task/status
disposition. If the lead asks to close it, set related tasks and epics to
`dropped` or `review` with a log entry, let the tooling archive only files whose
status becomes terminal, update `tasks/STATUS.md`, and keep only reusable
lessons in pipeline docs/skills. Never delete task files to hide closed work.

## Reporting

When the user asks "where are we" or "what next", follow the reporting rules in
`tasks/README.md`.
