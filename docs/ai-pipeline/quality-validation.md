# Quality And Validation Reference

Detailed portable quality-gate and validation rules. Load this file when
changing validation routing, product gates, done criteria, or repeated-failure
guards.

## Quality Gates

Gates are separate verdicts:

- Product/readability: can a new player understand and operate the screen?
- Game-loop/fun: is there a hook, repeatable loop, reward, and next-5-minutes
  reason to continue?
- Art-source/assets: are runtime assets real, traceable, and appropriate for
  the target?
- Technical/build: does the changed runtime/tooling actually work?

Do not call a slice done from one green gate. Builds, probes, crop audits, and
manifests support the verdict; they do not replace player-facing judgment.

When a strict/product gate fails twice for the same major reason, stop the local
polish loop. Create or link the different path: architecture, tooling, source
asset, reference, or explicit lead acceptance. This is enforced by:

```powershell
node tools/product_gate/repeated_failure_guard.mjs
```

`node tools/ai.mjs validate` runs that guard in quick mode.

## Validation Defaults

- Task/status docs: `node tools/taskboard/cli.mjs validate`
- Skill/process changes: `node tools/skills_eval.mjs`
- Product gate changes: `node --test tools/product_gate/test.mjs`
- Taskboard changes: `node --test tools/taskboard/test.mjs`
- AI facade/profile changes: `node --test tools/ai.test.mjs` and focused
  `tools/ai_profile` tests
- Reusable pipeline: `node tools/ai.mjs validate`
- Review-stage context/cap pressure: `node tools/ai.mjs validate --review`
- Portable/export/runtime gates: `node tools/ai.mjs validate --full`

Escalate validation only when the changed behavior or export path requires it.
Context budgets are a review/compression gate, not a normal implementation
blocker. Full portable validation is a final/broad gate, not the default after
every small edit.
