# Design Source Knowledge Templates

Load this file only when creating or substantially rewriting source notes,
knowledge pages, or source-review reports.

## Reusable Source Note

```markdown
---
type: Source Notes
title: Short Source Packet Title
description: Raw or near-raw notes supporting reusable game-design knowledge.
tags: [sources, design]
timestamp: YYYY-MM-DDT00:00:00Z
---

# Sources - Topic (checked YYYY-MM-DD)

Purpose: what reusable knowledge this source packet can support.

## Source Matrix

| Source | Link/Path | Quality | Checked | Proves | Does Not Prove |
|---|---|---|---|---|---|
| Title | URL or local path | official / gameplay / guide / review / deconstruction / talk / user-provided / unknown | YYYY-MM-DD | Specific observed fact | Missing or uncertain area |

## Evidence Notes

- `observed` - fact directly seen/read in the source, with section/timestamp/frame when useful.
- `secondary` - interpretation by another author.
- `inferred` - conclusion drawn from multiple facts; name the inference.
- `unknown` - not proven by the source packet.

## Reusable Takeaways

- [Takeaway] (label: observed/secondary/inferred). Why it matters.

## Candidate Knowledge Updates

- `gamedev_knowledge/knowledge/<page>.md` - proposed reusable rule or checklist item.
```

## Reusable Knowledge Page

```markdown
---
type: Game Design Knowledge
title: Short Human Title
description: One sentence describing when to use this page.
tags: [knowledge, topic]
timestamp: YYYY-MM-DDT00:00:00Z
---

# Short Human Title

Reusable rule or method, not project status.

## Use When

- Condition where this rule helps.

## Rules

1. Actionable rule.
2. Actionable rule.

## Anti-Patterns

- Mistake to avoid.

## Validation

- How an agent/human can check this rule was applied.

## References

- [Source note](../sources/source_file.md) - what it supports.
```

## Project Source Or Reference Note

```markdown
---
type: Project Source Notes
title: Reference Or Source Title
description: Source notes for <game-id>; project-specific, not reusable knowledge.
tags: [project, references]
timestamp: YYYY-MM-DDT00:00:00Z
---

# Reference / Source Title

Scope: what current game decision this supports.

## Source Matrix

| Source | Link/Path | Quality | Checked | Proves | Does Not Prove |
|---|---|---|---|---|---|

## Observations

- `observed` - claim with timestamp/frame/section.

## Application To <Game>

- Borrow:
- Avoid:
- Copy-risk:
- Current-build mismatch:
- Next proof screenshot/scenario:
```

## Source Review Report

```markdown
## Findings

- [Severity] File/path - issue and why it can mislead future agents.

## Routing Audit

- Reusable knowledge correctly placed:
- Project-specific material found in global folders:
- Raw sources missing from source shelf:
- Claims missing evidence labels:

## Recommended Edits

- Edit existing:
- Add new:
- Leave unchanged:
```
