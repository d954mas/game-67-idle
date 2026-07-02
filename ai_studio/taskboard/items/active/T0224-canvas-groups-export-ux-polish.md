---
id: T0224
title: "Canvas: groups v2 + export panel UX polish (no-hack leftovers)"
status: backlog
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Documented UX leftovers from the T0219/T0206 builds (lead 2026-07-02: "хочу
чтобы было хорошо, чисто, без техдолга и хаков"). Each was consciously
deferred with a note; none should survive long-term:

1. **Multi-group inspector**: selecting 2+ groups shows the empty inspector
   (inc3 deferral). Design the honest multi-selection inspector: shared
   fields editable when values agree (visible, clip), count header
   ("3 groups"), batched apply (respects T0223's one-entry law if it lands
   first - coordinate).
2. **Filled-group-body click**: design rule 10 says a group WITH a background
   fill is click-selectable by its body (empty area inside the frame), like a
   Figma frame with a fill; today only member content + label hit. Unfilled
   frames stay marquee-passthrough (deliberate divergence, keep).
3. **Export row suffix can't be cleared in place** (T0206 note): the shared
   textInput helper doesn't commit an empty value, so removing a suffix
   requires deleting + re-adding the row. Fix the helper (or that call site)
   so an explicit empty commit works everywhere it is safe; check other
   textInput call sites for the same trap (rename fields must NOT accept
   empty - keep their guard).
4. **Ghost-hint & breadcrumb sanity pass** after the lead's live verification
   of T0219: collect his notes on the drill/breadcrumb/ghost UX and fold
   fixes here.
5. **Shift-range selection in REGION lists** (lead, live verify 2026-07-02:
   "в layers да, а в других местах нет(regions)") - the inspector Regions
   rows (and any other multi-selectable list) must support Shift-click range
   from the last plain click, exactly like the layers panel. One shared
   range-select helper, not a copy.
6. **Clip ghost hint OFF by default** (lead, live verify: "вот это я бы не
   хотел видеть по умолчанию. Но чтобы была возможность увидеть") - the
   0.25-alpha ghost of a selected element's clipped-out portion must be
   hidden by default with an on-demand reveal (proposal: hold Alt to peek
   while an element is selected; view-state only, not journaled). Touches
   workspace.js drawClipGhosts.
7. **Layers deep-nesting readability >4 levels** (lead question, answered
   with Figma/analog survey): adopt (a) collapse-by-default for group rows +
   auto-expand only the selection's ancestor path (Figma reveal pattern),
   (b) indent GUIDES - thin vertical level lines a la VS Code so a 10px step
   stays readable at depth, (c) resizable layers panel width. Indent step
   already tightened to 10px (6b585ee5).

## Done when

- [ ] 2+ selected groups get a real multi-inspector (count + shared toggles); mixed agree/disagree states handled honestly
- [ ] clicking the filled body of a background-carrying group selects the group; unfilled body still starts marquee
- [ ] export row suffix clearable in place; textInput empty-commit audited across call sites (rename guards intact)
- [ ] Shift-click range selection works in the inspector Regions list (shared helper with layers)
- [ ] clip ghost hidden by default, Alt-hold (or agreed toggle) reveals it
- [ ] "Export project (N screens)" button label counts ALL groups; must count TOP-LEVEL visible only (matches exportProject since inc3; seen on screenshot: 1 top-level group shown as "2 screens")
- [ ] layers: groups collapsed by default + selection path auto-expands; indent guides; panel width draggable
- [ ] lead's live-verification UX notes on T0219 folded in (or explicitly none)
- [ ] tests where testable + gates green

## Open questions

## Log
- 2026-07-02: Created from T0219 inc3 deferrals + T0206 build note. Lead directive: clean, no tech debt, no hacks.
