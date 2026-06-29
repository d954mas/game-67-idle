# Quality

Quality defines how agents decide whether game-development work is good enough
to accept, continue, reject, or escalate.

This module owns rule navigation, lightweight quality evidence, and rule usage
profiling. It does not own asset production, task storage, runtime automation,
or game implementation.

Do not create ad-hoc quality rule IDs in project docs, generated templates,
tasks, or legacy tools. New reusable rules belong under `ai_studio/quality/rules`
and must be linked from the matching group README.

## Rule Groups

Open only the group that matches the changed work:

- [Player Clarity](rules/player_clarity/README.md): UI/UX, HUD, scene clarity,
  sprites, feedback, interactive elements, responsive viewports, virtual
  controls, and player-facing presentation.
- [Art](rules/art/README.md): art direction, composition, polish, generated
  art, visual target fit, and final-looking output.
- [GDD](rules/gdd/README.md): design source packages, source order, file roles,
  contradictions, requirements, and acceptance criteria.
- [Game Design](rules/game_design/README.md): core loop, player motivation,
  economy, progression, feature fit, design data, and playable-slice strength.
- [Technical](rules/technical/README.md): code, scripts, generated data,
  runtime behavior, build/run proof, input, state, save/load, packaging, and
  automation.
- [Assets](rules/assets/README.md): sourced/generated/runtime assets,
  provenance, licenses, manifests, publishability, and runtime-ready formats.

Each group has numbered `checks/Q*_NNN_*.md` rules. Rule `001` is the basic
group check.

Read this file, then the relevant group README, then only the rules needed for
the task.

## How To Use

Do not run every rule. Pick the group from the changed work.

Start with the group's `001` rule. It catches basic failures quickly. If it
fails, fix that before spending time on more specialized review.

Use numbered checks when the task matches their "Use When" section. If a
numbered check is not relevant, do not run it.

Record evidence when the work changes project state: screenshot, runtime
observation, validator output, source/provenance link, task log entry, or
another durable artifact.

## Profiling

Record applied rules in task `## Log` using a stable line:

```text
- YYYY-MM-DD: Quality: QCLR_001=pass; QART_001=block; evidence: <short proof or artifact>.
```

Allowed outcomes are `pass`, `block`, `review`, and `skip`.

Summarize rule usage with:

```powershell
node ai_studio/quality/profile.mjs
node ai_studio/quality/profile.mjs --include-archive --json
```

The profile is diagnostic. It shows which rules are used often and which ones
block work, but it is not a global validator.

Module implementation tests stay with the owning module.

## Principle

One green check is not acceptance. Technical proof, player clarity, visual
quality, asset provenance, and workflow state can fail independently.
