---
name: nt-blender-production
description: "Use for Blender 3D scene creation, modeling, procedural geometry, look-development, lighting, rendering, technical scene audits, visual-reference matching, style propagation, or Blender-to-engine handoff. Enforces a fail-closed feedback loop so geometry integrity, art-direction fidelity, completion, evidence freshness, and independent review are proven before user review or rollout."
---

# NT Blender Production

Use the canonical pipeline at
`ai_studio/assets/tools/blender/README.md`. Read it completely before changing a
Blender scene. It owns the audit scripts, proof packet, feedback loop and stop
rules.

## Required Sequence

1. Write a reference lock: immutable source scene, output version, approved
   spec/refs, camera, scope, style constraints, proof renders and done condition.
2. Build one thin coherent slice in a separate `.blend`.
3. Run `blender_scene_audit.py` before surface polish and after every risky
   geometry/modifier change.
4. Fix the first failing technical stage; rerun until no technical block remains.
5. Render clay, color, three-quarter depth, master frame, grayscale and
   silhouette evidence from the exact saved scene.
6. Open and compare current renders with every approved reference. Record a
   dimension-by-dimension art-direction mismatch audit.
7. Fix all critical/high style, completion and technical findings; repeat the
   loop from step 3.
8. Obtain independent visual and technical review when the reference controls
   final art, after user rejection, before propagation, or before handoff.
9. Validate a fresh-hash review bundle with `gate.py`.
10. Show the user only genuine design choices after the bundle passes. Keep
    propagation and handoff blocked otherwise.

## Non-Negotiable Gates

- A successful render is not a topology pass.
- A topology pass is not a style pass.
- An approved color is not approval of geometry, architecture or lighting.
- Close-up beauty does not replace three-quarter diagnostics or the approved
  camera.
- Do not hide known defects with camera, materials, AO, bevel or post effects.
- Do not call a requested component complete when any requested part is absent
  or visibly provisional.
- Any save after audit/render invalidates previous scene-hash evidence.
- Never propagate a bay/module/style while a critical/high mismatch is open.

## Style Fidelity

When a named style or reference drives the work, also use
`nt-design-knowledge`. Compare the native render against the durable reference
contract for composition, silhouette, architecture/form genealogy, proportions,
rhythm, palette, light, detail hierarchy, originality and completion. Label
observations and inferences separately. Convert every rejection into a durable
validator, checklist, regression case or source rule.

## Reviewer Roles

Keep building coherent and local. Use fresh read-only reviewers only at the
independent gate:

- visual verifier: approved refs/spec plus native evidence packet;
- technical verifier: `.blend`, audit report and construction scripts;
- lead: integrate findings, fix the scene, rerun all affected gates.

Reviewers cannot approve missing evidence or override a mechanical block.

## Completion Report

Report:

- source and output scene paths/hashes;
- reference/spec paths;
- technical audit status and blockers fixed;
- art-direction matches and remaining mismatches;
- independent-review result;
- evidence render paths;
- `QART`, `QASSET`, and `QTECH` outcomes where applicable;
- whether user review, propagation, or handoff is unlocked.
