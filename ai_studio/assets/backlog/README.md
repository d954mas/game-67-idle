# Asset Backlog

Temporary holding area for asset modules that are functional but not yet cleanly
decomposed into `viewer/` or `tools/`.

Backlog code can remain live when other modules depend on it, but new asset work
should avoid adding more responsibilities here. Split or promote backlog modules
only after their ownership boundary is clear.

Current backlog:

- `storage/`: source registry helpers, intake, manifests, generated indexes,
  preview cache, and license guard.
