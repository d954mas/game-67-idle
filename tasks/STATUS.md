# Project Status

## Current Goal

Start the `Voxelheim` (voxelheim) native-first prototype from a clean Stage 0
startup gate. Do not expand broad systems until the first fake shot/product-read
gate, native screenshot proof, and screenshot-vs-target mismatch list are
named. For beautiful/casual/generated-UI/fake-shot slices, the product gate
uses `--visual-strict`.

## Blocking Work

- No runtime implementation blocker is known yet; the next blocker should come
  from the first GDD/reference/fake-shot pass.

## Non-blocking Debt

- None recorded for this prototype yet.

## Current Gate

Stage 0 startup gate for voxelheim: active concept, actionable task T0001,
project wiki, native/runtime harness, and fake shot/product-read/native
screenshot proof plan must be visible before implementation. For visual work,
the 5-line session contract and screenshot-vs-target mismatch list are required
before runtime visual coding. Strict visual product gates require six scores
and blocker/major issue reporting before any pass.

## Required Validation

```powershell
node tools/game_context/iteration_context.mjs
node tools/taskboard/cli.mjs validate
```

## Last Known Good Evidence

- `tmp/prototype_startup_gate_context.json` after kickoff.
- `gamedesign/projects/voxelheim/reviews/first_slice_visual_gate.md` is the
  first-slice visual/product gate template and must be filled before broad
  runtime work; it names the optional visual critic packet command.

## Next Priorities

1. Fill `gamedesign/projects/voxelheim/gdd.md` with the first playable loop,
   references/fake shot target, visual/product proof gate, and stop condition.
2. Fill `gamedesign/projects/voxelheim/reviews/first_slice_visual_gate.md`
   with the target, native screenshot/capture plan, mismatch list, gate command,
   critic packet command, strict visual rubric, and expansion decision.
3. Identify the native build/run command for this prototype.
4. Capture or plan the first native screenshot, compare it with the accepted
   target, and record the mismatch list before broad content.
