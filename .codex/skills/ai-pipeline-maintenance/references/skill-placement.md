# Skill And File Placement

Load this when a pipeline cleanup asks whether rules belong in files, skill
entrypoints, skill references, docs, tasks, or a new skill.

## Placement Ladder

Use the smallest durable home that lets the next agent load only what applies.

1. Mechanical invariant -> `tools/` validator or test.
2. Current game routing -> `GAME_PROJECT.md`; work state/evidence -> active
   task log or evidence file.
3. Project boundary or hard repo policy -> `AGENTS.md`.
4. Portable workflow map -> `ai_studio/README.md`.
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
- task/status lifecycle -> `nt-taskboard-manager`;
- session postmortem -> `chat-session-reflection`;
- playable implementation -> `game-feature-iteration`;
- visual polish or fake shots -> `game-visual-art-direction`.

## Current Skill Ownership

Use this map before moving rules out of hot files or creating a new skill:

- Pipeline cleanup, context budgets, file/skill placement, export, validators,
  and repeated workflow friction -> `ai-pipeline-maintenance`.
- Long-session review, profiler interpretation, and improvement backlog from a
  completed run -> `chat-session-reflection`.
- Task capture, task splitting, status, active/archive lifecycle, and evidence
  rules -> `nt-taskboard-manager`.
- New game concept, GDD, reference pack, core-loop model, fake shots, and
  implementation handoff -> `primary-gdd-pipeline`.
- Playable native feature iteration, build/run proof, slice hygiene, and close
  slice -> `game-feature-iteration`.
- Native runtime driving, screenshots, video, DevAPI command contracts, and
  visual QA automation -> `game-runtime-automation`.
- Visual direction, fake-shot critique, art quality, and art request packets ->
  `game-visual-art-direction`.
- Generated UI kits, source sheets, slice9/crop manifests, and responsive UI
  composition proofs -> `generated-game-ui-assets`.
- Source assets, provenance, runtime packs, atlas validation, and pack
  reproducibility -> `game-asset-pipeline`.
- Schema-first state, generated C APIs, save/load, migrations, fixtures, and
  DevAPI state commands -> `game-state-management`.
- Research source intake, reusable design knowledge promotion, reference
  deconstruction, and source hygiene -> `design-source-knowledge`.
- External raster generation fallback through CLI/subagents ->
  `delegated-image-generation`.

Do not create a new skill for the universal AI pipeline until a repeated
workflow falls outside this map and needs its own trigger plus distinct
validation/report shape. Add a reference under the owner skill first when the
entrypoint can stay short.

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
- `ai_studio/README.md`: portable routing and operating rules.
- `ai_studio/taskboard/README.md`: task store commands and lifecycle map.
- `SKILL.md`: trigger, what to load, default procedure, stop conditions.

If a hot file needs an example, a history lesson, a long checklist, or multiple
conditional branches, move that detail behind a reference and leave a one-line
route.

## Validation

After moving rules between files and skills, run:

```powershell
node tools/context_budget.mjs
node tools/context_budget.mjs --review
node tools/skills_eval.mjs
node tools/skills_sync.mjs --check
node ai_studio/core_harness/validation/doc_reference_check.mjs
node ai_studio/core_harness/validation/pipeline_validate.mjs
```
