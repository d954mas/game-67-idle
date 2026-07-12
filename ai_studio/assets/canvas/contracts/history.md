# Journal and history

Every persisted gesture appends one thin mutation row plus a sidecar snapshot.
`project.json.history_seq` is the applied head. Undo restores the head's before
snapshot; redo selects the newest child of the current head; a new mutation
after undo creates a new branch and invalidates the stale redo tail.

Agents must read `history-list` immediately before `undo`, `redo`, or
`history-jump`, then pass the observed integer as `--expect-head`. A mismatch
refuses before any write. Files remain immutable, so metadata snapshots restore
pixel identity through their content-addressed references.

History depth is configured by the portable Studio config/local override and
old entries are compacted into the archive without changing the visible spine.
