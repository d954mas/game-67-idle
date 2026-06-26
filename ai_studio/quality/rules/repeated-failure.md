# Repeated Failure Rule

Apply when strict/product validation, visual review, or lead review fails more
than once for the same reason.

## Rule

Do not keep polishing the same path. Stop and choose one:

- change architecture;
- change tooling;
- source or generate better assets;
- narrow the slice;
- record explicit lead acceptance.

## Guard

```powershell
node tools/product_gate/repeated_failure_guard.mjs
```

Lead rejection is project state. If it is unresolved, log the rejection and next
path before continuing feature work.
