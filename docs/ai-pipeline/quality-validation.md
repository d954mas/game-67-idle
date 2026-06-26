# Quality And Validation Reference

Portable rules for gates, done criteria, or guards.

## Quality Gates

Separate verdicts:

- Product/readability: can a new player understand and operate the screen?
- Game-loop/fun: hook, repeatable loop, reward, reason to continue?
- Art-source/assets: are runtime assets real, traceable, target-appropriate?
- Technical/build: does the changed runtime/tooling actually work?

Do not close from one green gate. Builds/probes/audits are evidence, not
acceptance. For a contested gate, run one clean verifier on only the named check
(`node tools/product_gate/review.mjs ... --verify`): CONFIRM/REFUTE, no self-grading.

When strict/product fails twice for the same reason, stop polishing and create
another path (architecture, tooling, source asset, reference) or record lead
acceptance:

```powershell
node tools/product_gate/repeated_failure_guard.mjs
```

Visual rejection first:
`node tools/product_gate/visual_rejection_lock.mjs --project <id> --task <TID> --screenshot <path> --problem "<why rejected>" --next "<different path>"`.

3D asset material floor:
`node tools/product_gate/visual_material_floor.mjs`.

For active 3D games that claim sourced/ready GLB or GLTF models, this guard
fails when model geometry is rendered through a flat color-only shader, one
fallback material/texture, or object-level tint without proving source
materials, textures, UVs, or per-primitive material colors. Treat that as a
product FAIL, not a style preference: stop feature/content expansion and create
a material/texture pass before more story, economy, map, traffic, or NPC work.

Active game workflow guard:

```powershell
node tools/game_context/workflow_guard.mjs
```

Dormant for a clean seed. For active games it fails feature/content expansion
under unresolved lead rejection, runtime work when `data/core_loop.json` says
references are not ready, and oversized `src/clean_seed_main.c` without an
architecture/decomposition task. Reviews/scores are evidence only; record lead
acceptance before overriding rejection.

Gate logs may carry `[GATE-ID]: PASS|CONCERNS|FAIL`; repeated-failure guard
counts total FAILs.

## Validation Defaults

- Task/status docs: `node ai_studio/taskboard/cli.mjs validate`
- Skill/process changes: `node tools/skills_eval.mjs`
- Product gate changes: `node --test tools/product_gate/test.mjs`
- Taskboard changes: `node --test ai_studio/taskboard/tests/taskboard.test.mjs`
- Profile changes:
  `node ai_studio/core_harness/profiling/tests/profiling.test.mjs`
- Core Harness docs/routes:
  `node ai_studio/core_harness/validation/doc_reference_check.mjs`
- Architecture map: `node ai_studio/architecture_map/validate_map.mjs`
- Portable export: `node --test tools/bootstrap/export_base.test.mjs`
- Visual/playable changes: native scenario plus screenshot/video/product gate
  evidence

Playable smokes expose named checks, not generic pass/fail. Prefer stable ids
such as `accept.chest_opens_after_combat`, then print a compact summary.

Escalate validation only when the change/export path requires it; budgets are
a review gate and full validation a final gate, not defaults after small edits.
