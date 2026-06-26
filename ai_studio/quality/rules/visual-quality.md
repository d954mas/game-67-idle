# Visual Quality Rule

Apply when work changes screenshots, game presentation, UI layout, generated art,
composition, readability, or visual polish.

## Check

Inspect the actual runtime or generated output. Do not accept claims based only
on source files.

Look for:

- blank or broken render;
- unreadable text;
- overlapping UI;
- placeholder/debug visuals;
- weak composition or unclear action direction;
- mismatch with accepted visual target.

## Tools

Use product gate tools when a durable verdict is needed:

```powershell
node tools/product_gate/review.mjs
node tools/product_gate/visual_critic_run.mjs
node tools/product_gate/visual_rejection_lock.mjs
```

If a visual direction was rejected, do not continue feature/content expansion on
top of it without explicit lead acceptance.
