---
id: T0219
title: "Canvas groups: z-order among siblings, NESTED groups, layers indent polish"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Lead's live feedback (2026-07-02) after T0217 landed. (1) GROUP Z-ORDER:
groups need the same sibling reordering elements got — drag in layers +
Order actions + shortcuts; extend `reorderElement` or add a sibling op for
groups (T0217 shipped elements-only, documented follow-up). (2) NESTED
GROUPS ("группы могут быть вложенными"): the model currently allows ONE
level (elements->group); extend to groups-in-groups. This is a real model
change — parent ref on groups, recursion in: composite render (renderGroup
must render child groups), layers panel tree, reparent drag (group onto
group), marquee/selection, move-translates-members, visibility cascade,
clip-to-frame later (T0213). Design first (deep-reasoner): keep the flat
elements[] z-order as the single paint order or move to a scene-graph
order? — decide explicitly, journal/undo untouched, tool parity (CLI
group-assign accepts group ids). (3) LAYERS INDENT POLISH: ungrouped
element rows carry a caret-width left spacer that reads as group
membership ("какой-то странный сдвиг слева") — visually distinguish
top-level rows from group members (no fake indent; members indent, roots
don't).

## Done when

- [ ] groups reorder among siblings (drag with insertion line + Order actions + Ctrl+[/]); render/export honors the order; one journal entry per gesture
- [ ] groups nest: create/move a group inside a group; composite render + visibility + move cascade correctly; layers tree shows the hierarchy; CLI parity
- [ ] top-level rows have no fake indent; group members clearly indented
- [ ] tests for group reorder + nested render; validate_map --strict + doc_reference_check green

## Open questions

## Log
- 2026-07-02: Created from live feedback during T0217 review.
- 2026-07-02: Lead: inspector Regions section "Edit" and "+ Add" buttons are redundant ("избыточно") - dblclick/context menu enter the mode, drag draws a rect (and T0209 brings the tool row). REMOVE both in the bug-fix acceptance pass; Detect and Slice buttons stay (no other UI path). Same pass: kill the fake left indent on top-level rows (item 3 of What).
