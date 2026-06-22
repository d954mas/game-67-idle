# Project Status

## Current Goal

Start the `Cozy Automation` (cozy-automation) native-first prototype from a clean
Stage 0 gate. Do not expand systems before the first product-read / native
screenshot gate and the screenshot-vs-target mismatch list are named.

## Current Gate

Stage 0 for cozy-automation: active concept, actionable task `T0104`, project wiki
(`gamedesign/projects/cozy-automation/`), native/runtime harness, and a
fake-shot / product-read / native-screenshot proof plan visible before
implementation. Visual work needs the screenshot-vs-target mismatch list before
runtime visual coding.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
node tools/taskboard/cli.mjs validate
```

## Next Priorities

1. Fill `gamedesign/projects/cozy-automation/gdd.md` — first playable loop,
   reference/fake-shot target, product proof gate, stop condition.
2. Fill `gamedesign/projects/cozy-automation/data/core_loop.json` — player verbs,
   rules, feedback, goals, replay reason. Do not assume idle/away-time/reset-meta
   loops unless the lead chooses that direction.
3. Fill `gamedesign/projects/cozy-automation/reviews/first_slice_visual_gate.md` —
   target, native screenshot plan, mismatch list, gate command, rubric, expansion
   decision.
4. Identify the native build/run command; capture the first native screenshot and
   record the mismatch list before broad content.
