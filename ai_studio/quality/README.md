# Quality

Quality defines how agents decide whether game-development work is good enough
to accept, continue, reject, or escalate.

This module owns rule navigation, lightweight quality evidence, and rule usage
profiling. It does not own asset production, task storage, runtime automation,
or game implementation.

Do not create ad-hoc quality rule IDs in project docs, generated templates, or
tasks. New reusable rules belong under `ai_studio/quality/rules` and must be
listed in the catalog below.

## Rule Catalog

Use this table to decide without loading rule files. After selecting an ID,
open only its linked check.

| Group | Rule | Use when | Do not use for |
| --- | --- | --- | --- |
| Player Clarity | [QCLR_001](rules/player_clarity/checks/QCLR_001_player_clarity.md) | a changed screen, HUD, feedback, or flow may obscure state or next action | pure geometry, art direction, technical invariants, assets, or game-loop design |
| Player Clarity | [QCLR_002](rules/player_clarity/checks/QCLR_002_responsive_viewports.md) | viewport ratios or orientation can crop, overlap, hide, or misplace UI | unchanged layout or non-visual technical behavior |
| Player Clarity | [QCLR_003](rules/player_clarity/checks/QCLR_003_virtual_controls.md) | touch controls may be unclear, unreachable, or cover gameplay/HUD | keyboard/controller delivery or virtual-control-free screens |
| Art | [QART_001](rules/art/checks/QART_001_closest_practical_visual.md) | player-facing visuals need the closest practical final direction | clarity/layout, asset provenance, runtime behavior, or game-loop design |
| GDD | [QGDD_001](rules/gdd/checks/QGDD_001_design_source_readiness.md) | a design package must guide implementation, review, or a lead decision | visual clarity, art/assets, runtime behavior, or loop quality |
| Game Design | [QDES_001](rules/game_design/checks/QDES_001_playable_loop.md) | loop, economy, progression, rewards, or challenge change player action | source-package clarity, presentation, assets, or runtime proof |
| Technical | [QTECH_001](rules/technical/checks/QTECH_001_behavior_evidence.md) | code, data, build, state, input, packaging, automation, or runtime behavior changes | presentation-only, art/assets, loop design, or source clarity |
| Assets | [QASSET_001](rules/assets/checks/QASSET_001_asset_readiness.md) | an asset is accepted, published, copied, or claimed game-use-ready | presentation, art fit, or unrelated runtime behavior |
| Assets | [QASSET_002](rules/assets/checks/QASSET_002_material_readiness.md) | conversion or preparation can lose textures, UVs, colors, maps, or assignments | non-material assets or unrelated presentation/runtime work |

## How To Use

Do not run every rule. Pick the group or groups from the changed work. A single
change can need more than one group: for example, a player-facing asset change
can need Assets for provenance/readiness, Player Clarity for visible
understanding, and Technical for behavior evidence.

Start with a group's `001` rule when its "Use When" section matches the task.
If it does not match, use the more specific rule directly.

Use numbered checks when the task matches their "Use When" section. If a
numbered check is not relevant, do not run it.

Record evidence when the work changes project state: screenshot, inspected
runtime state, validator output, source/provenance link, task log entry, final
report note, PR/review comment, or another durable artifact.

## Profiling

When a task file exists, record applied rules in task `## Log` using a stable
line:

```text
- YYYY-MM-DD: Quality: QCLR_001=pass; QART_001=block; evidence: <short proof or artifact>.
```

Historical logs may contain `pass`, `block`, `review`, `skip`, and `unverified`.
Current Taskboard state permits `pass`, `block`, `review`, and `unverified`;
`skip` is history-only and new non-applicable decisions require a reason.
The structured Taskboard close-transition contract is owned by
[`task-store-reference.md`](../taskboard/task-store-reference.md#done-and-evidence).
Quality owns rule meaning; Taskboard validates current state and keeps the dated
log only as profiling history.

Summarize rule usage with:

```powershell
node ai_studio/quality/profile.mjs
node ai_studio/quality/profile.mjs --include-archive --json
```

The profile uses Taskboard's canonical dated-log scanner. It ignores bare,
fenced, commented, and post-section examples, and does not count outcomes
recorded only in final responses, PR/review comments, or other artifacts.

The profile is diagnostic. It shows which rules are used often and which ones
block or remain unverified, but it is not a global validator.

Quality outcomes are process records. Mechanical gates and the advisory
boundary are summarized in the
[Core Harness workflow](../core_harness/workflow/README.md#enforcement-boundary).

Module implementation tests stay with the owning module.

## Principle

One green check is not acceptance. Technical proof, player clarity, visual
quality, asset provenance, and workflow state can fail independently.
