---
id: T0217
title: "Canvas UI organization: context-menu diet, collapsible inspector sections, z-order"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Lead's live-testing feedback (2026-07-02), three parts. (1) CONTEXT-MENU DIET:
the right-click menu is overloaded — region actions take 3 rows for an
infrequent action ("им там не место"); collapse to at most one "Edit regions"
entry (dblclick stays the primary path); REMOVE "Rename" (dblclick rename is
the way) and REMOVE "Hide" (the visibility dot in layers is the way); export
also leaves the menu (goes to T0206's inspector panel). Menu keeps only
frequent object actions. (2) INSPECTOR SECTIONS: the right panel needs clearly
separated, titled action groups like Figma's Design tab (Position/Size,
Regions, Meta, Export...), each COLLAPSIBLE with persisted collapsed state
("хочется чтобы было понятнее разделение групп действий. И чтобы можно было
сворачивать группы"). (3) Z-ORDER like Figma ("есть еще очередность"): drag to
reorder siblings in the layers panel (existing dnd extends to within-parent
reorder), context-menu actions Bring forward / Send backward / Bring to front /
Send to back, shortcuts Ctrl+] / Ctrl+[ / Ctrl+Alt+] / Ctrl+Alt+[ matched by
event.code (layout-independent). Needs a journaled reorder op in ops.mjs
(sibling index move) with CLI parity (element-reorder); coordinate with T0201's
agent if ops.mjs is still in flight. (4) LAYERS PANEL COLLAPSE is broken UX
("layer плохо скрывается. я вижу просто пустую линию"): collapsed state must be
a proper slim rail with a visible re-open control (icon + tooltip), not an
empty strip; same collapse quality bar for the inspector. (5) REGION TREE
LEAVES THE LAYERS PANEL (lead 2026-07-02: duplicates region-edit mode, "давай
список регионов уберем из левой панели"): layers shows only elements/groups;
region rename lives in the inspector only. (6) RENAME BUG HUNT (live report):
dblclick rename dead for layers and regions; "Rename region" menu editor
accepts text but value not applied/displayed; region names invisible (sizes
only) — root-cause the inline.js finish/blur + layers render-guard interplay
and verify setRegions {name} persistence + inspector display. (7) REGION ROW
ERGONOMICS (live report): × delete on a region row does not work (must commit
journaled setRegions); × hit target too small and hovering it causes a
scrollbar to appear that shifts the target (reserve space, no hover layout
shift); long region lists need an EXPLICIT project-styled scrollbar
(scrollbar-gutter stable).

## Done when

- [ ] context menu: no Rename, no Hide, regions = max one entry; everything removed stays reachable (dblclick rename, layers visibility dot); Export STAYS in the menu until T0206's inspector panel lands (T0206 removes it)
- [ ] inspector renders titled collapsible sections; collapsed state survives reload; regions section compact (not 3+ rows of noise)
- [ ] z-order: layers drag reorders siblings, 4 menu actions + Ctrl+[/] shortcuts work on any keyboard layout, render order follows
- [ ] collapsed layers panel is a slim rail with an obvious re-open icon (no empty dead strip)
- [ ] reorder is ONE journaled op per gesture, undo restores exact order, CLI parity command exists
- [ ] tests cover the reorder op; validate_map --strict + doc_reference_check green

## Open questions

## Log
- 2026-07-02: Created from live lead feedback during region-workbench testing.
- 2026-07-02: Lead overruled pure-global undo: in region-edit mode Ctrl+Z clamps at the mode-entry journal position (only in-mode region edits undo; pre-mode ops need Esc first). Implement in the orchestrator acceptance pass right after this task lands (record base seq on enterRegionEdit, clamp in onKeyDown; redo unaffected).
- 2026-07-02: Lead: screen/group naming is CONFUSED across the UI - unify on GROUP everywhere (data model already says group; "screen" is a UI label + render-screen CLI). Remove the "+ Screen" button; group creation = right-click context menu in the layers panel ("Create group") + existing Ctrl+G from selection. ORCHESTRATOR ACCEPTANCE PASS item #2 (with the undo-clamp): rename UI labels/tooltips/breadcrumbs, drop the button, add layers empty-area context menu entry; keep CLI back-compat (render-screen may alias render-group).
- 2026-07-02: SHIPPED 6c76a78d (builder: menu diet, sections, z-order op+drag+shortcuts, rail; rename ROOT CAUSE = click refresh() rebuilt panel between dblclick clicks; x delete was scrollbar layout shift) + ce6b001b (orchestrator pass: undo clamp at region-edit entry seq; screen->group unification, +Screen button removed, Create group in layers/canvas context menus, CLI render-group alias). 76/76 tests. OPEN for lead: layers panel lists back-to-front (top row = painted first); Figma shows front-at-top - flipping is a small separate change if wanted.
