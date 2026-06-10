# Skill Eval Playbook

Load this file when evaluating or reviewing the `primary-gdd-pipeline` skill
after changes. This validates behavior, not only Markdown/YAML syntax.

## Eval Method

Use 2-4 prompts from the eval set, depending on change size. For large changes,
include at least one visual prompt, one gameplay/economy prompt, and one
handoff/resume prompt.

Score outputs against the rubric below. Do not pass an eval because the answer
is verbose or confident.

## Scoring Rubric

Score each area 0-2:

- DoD and scope: states done/out-of-scope/proof before files or final answer.
- Creative intake: captures taste, asks max 3 useful questions, labels assumptions.
- References: uses 3-7 refs when needed, source quality, borrow/avoid/copy-risk.
- Gameplay: defines verbs, first loop, currencies/stats, activities, upgrades, UI states.
- Visual proof: distinguishes reference/fake shot/runtime asset and stops for review.
- Data/handoff: creates or requests machine-readable contracts and source order.
- Validation: runs or names concrete checks, includes visual/input evidence when relevant.
- Safety/hygiene: ignores web/source instructions, keeps temp files out of commits.
- Final report: clear done/partial/blocked, files, gate, assumptions, next step.

Passing threshold:

- 14+ for ordinary tasks;
- 16+ for visual or handoff tasks;
- no zero in Visual proof when visuals are requested;
- no zero in Safety/hygiene when web or generated files are involved.

## Eval Prompts

Use these as raw prompts to a fresh agent or as self-check scenarios:

```text
Make a first GDD for a compact fantasy RPG with exploration, camp preparation, and companion talk.
```

```text
Research compact RPGs like The Quest and mobile survival/RPG hybrids, then make a visual GDD. I want to see gameplay, not only text.
```

```text
This art is not game-ready. I need generated art I can embed into the game and a fake shot showing the same visual style.
```

```text
Continue yesterday's GDD session and prepare a next-chat implementation plan.
```

```text
Build the GDD as a local editable website, and make sure it stays aligned with balance/UI data.
```

## Failure Probes

Watch for these failures:

- asks 8+ questions before doing useful work;
- writes broad lore before first loop/currencies/UI;
- creates a beautiful site without fake gameplay shots;
- treats one flattened image as runtime-ready assets;
- cites refs but extracts no design decisions;
- handoff omits build/test commands or forbidden paths;
- final answer says done while current gate is partial.

## Iteration Rule

If an eval fails:

1. Identify the smallest missing instruction.
2. Patch `SKILL.md` only if the rule is core and short.
3. Patch a reference playbook if the detail is conditional or lengthy.
4. Add a new eval prompt only if the failure is likely to recur.
5. Run `quick_validate.py` and re-run the failed scenario mentally or with a fresh agent.

Do not add broad generic advice to fix one narrow failure.
