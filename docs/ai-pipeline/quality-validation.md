# Quality And Validation Reference

Portable quality-gate and validation rules. Load when changing validation
routing, product gates, done criteria, or repeated-failure guards.

## Quality Gates

Separate verdicts:

- Product/readability: can a new player understand and operate the screen?
- Game-loop/fun: hook, repeatable loop, reward, reason to continue?
- Art-source/assets: are runtime assets real, traceable, target-appropriate?
- Technical/build: does the changed runtime/tooling actually work?

Do not call a slice done from one green gate; builds/probes/audits support a
verdict, not replace player-facing judgment. For a contested gate the lead runs
ONE independent verifier in a clean context that re-runs only the named check and
returns CONFIRM/REFUTE (`node tools/ai.mjs gate ... --verify`, opt-in): a green
gate is not self-graded.

When a strict/product gate fails twice for the same major reason, stop polishing
and create/link a different path (architecture, tooling, source asset, reference)
or record explicit lead acceptance. Enforced by:

```powershell
node tools/product_gate/repeated_failure_guard.mjs
```

`node tools/ai.mjs validate` runs it in quick mode. Gate lines may carry a
parseable `[GATE-ID]: PASS|CONCERNS|FAIL` verdict; the guard clusters a gate's
FAILs by it and counts TOTAL (not just consecutive) occurrences, so interleaved
axes can't slip a loop past.

## Validation Defaults

- Task/status docs: `node tools/taskboard/cli.mjs validate`
- Skill/process changes: `node tools/skills_eval.mjs`
- Product gate changes: `node --test tools/product_gate/test.mjs`
- Taskboard changes: `node --test tools/taskboard/test.mjs`
- AI facade/profile changes: `node --test tools/ai.test.mjs` + focused
  `tools/ai_profile` tests
- Reusable pipeline: `node tools/ai.mjs validate`
- Review-stage context/cap pressure: `node tools/ai.mjs validate --review`
- Portable/export/runtime gates: `node tools/ai.mjs validate --full`
- Visual/playable changes: native scenario plus screenshot/video/product gate
  evidence

Playable smoke scripts should expose acceptance criteria as named checks, not
just generic pass/fail. Prefer stable ids such as
`accept.first_rune_claims_without_opening_gate`,
`accept.chest_opens_after_combat`, and `visual.screenshot_captured`, then print
a compact summary. Keep task `Done when` readable in Markdown while making the
runtime contract machine-checkable.

Escalate validation only when the change/export path requires it; budgets are
a review gate and full validation a final gate, not defaults after small edits.
