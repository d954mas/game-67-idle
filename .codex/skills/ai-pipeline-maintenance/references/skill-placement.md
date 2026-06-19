# Skill And File Placement

Load this when a pipeline cleanup asks whether rules belong in files, skill
entrypoints, skill references, docs, tasks, or a new skill.

## Placement Ladder

Use the smallest durable home that lets the next agent load only what applies.

1. Mechanical invariant -> `tools/` validator or test.
2. Current work state/evidence -> `tasks/STATUS.md`, active task log, or
   evidence file.
3. Project boundary or hard repo policy -> `AGENTS.md`.
4. Portable workflow map -> `AI_PIPELINE.md`.
5. Repeatable task behavior -> existing `.codex/skills/<skill>/SKILL.md`.
6. Detailed method, checklist, or examples -> that skill's `references/`.
7. Cross-project conceptual guidance -> `docs/ai-pipeline/` or
   `gamedesign/knowledge/`, depending on domain.

## New Skill Triage

Before creating a skill, inspect current ownership:

```powershell
rg -n "<trigger or workflow phrase>" .codex/skills/*/SKILL.md .codex/skills/*/references
```

Then answer:

- What user phrase should activate it?
- What work does the agent repeat, not merely remember?
- What tool preferences, permissions, validation, or report shape differ from
  existing skills?
- What can stay behind references so startup context remains small?
- What eval anchor proves the skill still exists after future cleanup?

If those answers point to an existing owner, update that skill or add a
reference under it instead of creating a new skill.

## Update Existing Skill

Prefer an existing skill when:

- its description already matches the user's trigger;
- the procedure is a deeper version of the same task type;
- adding a reference keeps `SKILL.md` short and improves progressive loading;
- existing eval coverage can protect the new behavior.

Good examples:

- pipeline docs split -> `ai-pipeline-maintenance`;
- task/status lifecycle -> `task-manager`;
- session postmortem -> `chat-session-reflection`;
- playable implementation -> `game-feature-iteration`;
- visual polish or fake shots -> `game-visual-art-direction`.

## Create New Skill

Create a new skill only when all are true:

- the workflow has a distinct trigger users will naturally say;
- it repeats across sessions or projects;
- no existing skill owns the behavior cleanly;
- it has its own validation/report shape or tool preferences;
- the startup description can be concise and activation-specific.

Do not create a skill for one task, one game fact, one command alias, or a rule
that belongs in a validator, task log, or project GDD.

## Keep Hot Docs Thin

Hot docs should answer "where do I go next?", not teach the whole method.

- `AGENTS.md`: boundaries, source map, hard gates, validation defaults.
- `AI_PIPELINE.md`: portable routing and operating rules.
- `tasks/README.md`: task store commands and lifecycle map.
- `SKILL.md`: trigger, what to load, default procedure, stop conditions.

If a hot file needs an example, a history lesson, a long checklist, or multiple
conditional branches, move that detail behind a reference and leave a one-line
route.

## Validation

After moving rules between files and skills, run:

```powershell
node tools/context_budget.mjs
node tools/skills_eval.mjs
node tools/skills_sync.mjs --check
node tools/doc_reference_check.mjs
node tools/pipeline_validate.mjs
```
