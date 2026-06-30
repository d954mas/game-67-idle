---
id: T0170
title: Move primary GDD workflow into AI Studio
status: done
epic: E001
priority: P2
tags: [game-design, gdd, skill, refactor]
created: 2026-06-30
updated: 2026-06-30
---

## What
Move the legacy primary GDD skill into AI Studio ownership without
blindly preserving old process weight. Keep the reusable GDD workflow, route
knowledge/reference concerns to the existing game-design knowledge group, and
drop obsolete skill-eval/process leftovers.

## Done when

- [x] Reviewed legacy `primary-gdd-pipeline` files and classified keep/merge/drop.
- [x] Created an `ai_studio/game_design/gdd/` group with concise workflow docs.
- [x] Replaced legacy skill surface with `nt-primary-gdd`.
- [x] Updated `ai_studio/tree.json` and generated agent surfaces.
- [x] Validated skill, map, docs, and taskboard.
- [x] Committed and pushed the slice.

## Open questions

- Resolved: embedded `skill-eval-playbook.md` is obsolete for the live GDD
  workflow. Skill evaluation belongs to `skill-creator`, explicit
  best-practice review, and concrete forward tests when needed.
- Resolved for now: web GDD stays in the GDD workflow as a document/site
  guidance file. If it becomes an actual editor or Studio surface, move the app
  surface separately later.

## Log

- 2026-06-30: Started migration slice after map validation showed the legacy
  `primary-gdd-pipeline` skill still unmapped.
- 2026-06-30: Classified legacy files. Kept GDD workflow, gameplay, visual
  proof, web GDD, stewardship, handoff, review, and templates as concise
  `ai_studio/game_design/gdd/` docs. Dropped embedded `skill-eval-playbook`
  from the runtime workflow; skill review belongs to skill-creator and explicit
  agent best-practice review.
- 2026-06-30: Validation passed for skill frontmatter, agent surface sync, map,
  doc references, and taskboard. The system `quick_validate.py` could not run in
  the current Python environments because PyYAML is unavailable; a local
  frontmatter parser verified the required `name` and `description`.
- 2026-06-30: Moved primary GDD workflow into ai_studio/game_design/gdd, replaced legacy skill with nt-primary-gdd, synced agent surfaces, and validated map/docs/taskboard.
