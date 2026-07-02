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
- [ ] group CLIP: a group can clip members at its bounds (Figma frame behavior) — canvas draw + renderGroup honor the flag; toggle in inspector; default off
- [ ] group BACKGROUND: optional fill on the group (color first), drawn on canvas AND composited by renderGroup (replaces the render-only background param as the primary path)
- [ ] sliceRegions wraps its crops into a new group "<sheet name> slices" in the SAME op/journal entry (undo removes group+crops together); site + CLI parity
- [ ] deleteGroup DELETES its member elements with it (one journal entry, undo restores both); dissolving a group without deleting content stays on Ungroup
- [ ] tests for group reorder + nested render + clip + background + slice-group; validate_map --strict + doc_reference_check green

## Open questions

## Log
- 2026-07-02: Created from live feedback during T0217 review.
- 2026-07-02: Lead: inspector Regions section "Edit" and "+ Add" buttons are redundant ("избыточно") - dblclick/context menu enter the mode, drag draws a rect (and T0209 brings the tool row). REMOVE both in the bug-fix acceptance pass; Detect and Slice buttons stay (no other UI path). Same pass: kill the fake left indent on top-level rows (item 3 of What).
- 2026-07-02: Lead: slice belongs in the region-mode right-click menu, count-aware - ONE region selected -> "Slice region", several -> "Slice selected (N)" (slices the SELECTION, not just the clicked row). Add in the bug-fix acceptance pass (context_menu.js busy).
- 2026-07-02: Lead (evening live testing): (a) slice dumps N crops straight onto the scene ("сразу 43 обьекта прямо на сцене") -> sliceRegions must create a wrapping group in the same journaled op; SMALL, pull forward: implement right after T0220 lands (ops.mjs is in the sweep's hands until then). (b) groups must be able to CLIP members at their bounds ("нужна возможность чтобы группа клипала по границам"). (c) From "почему я не вижу фон в канвасе": renderGroup's background is a render-time param only; groups need a real background (canvas-visible + render parity). (d) Figma reference discussed: images can't parent other layers in Figma - containers are frames/groups; a frame can carry an image fill. Design implication: keep groups as the only container, give them fill (color now, image later) instead of inventing image-parents.
- 2026-07-02: Lead: deleting a group must delete everything inside ("при удалении группы я ожидал что все вложенное удалится") - Ungroup is the explicit way to dissolve. Today deleteGroup orphans members to top level (ops.mjs:660-663). Pull forward with slice-group: implement right after T0220 frees ops.mjs.
- 2026-07-02: Indent item (3) already fixed live in 4c5b6815 (margin removed, caret spacer alone indents members); Edit/+Add removal + count-aware slice menu landed earlier same day. Remaining scope = groups z-order/nesting/clip/background + slice-group.
