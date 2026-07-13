---
id: T0224
title: "Canvas: groups v2 + export panel UX polish (no-hack leftovers)"
status: done
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-10
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
7. **Batched addImages op** (found by T0223's gesture audit, 2026-07-02):
   multi-file image drop (`addImageFiles`, site/actions.js) issues one
   POST /images per file = N journal entries for ONE drop gesture - the
   only remaining violation of the one-entry law. New op
   `addImages({projectId, images:[...]})` (one commitMutation), HTTP + CLI
   parity; page drop/paste path rewired; single-image add stays on addImage.
8. **Layers deep-nesting readability >4 levels** (lead question, answered
   with Figma/analog survey): adopt (a) collapse-by-default for group rows +
   auto-expand only the selection's ancestor path (Figma reveal pattern),
   (b) indent GUIDES - thin vertical level lines a la VS Code so a 10px step
   stays readable at depth, (c) resizable layers panel width. Indent step
   already tightened to 10px (6b585ee5).

## Done when

- [x] 2+ selected groups get a real multi-inspector (count + shared toggles); mixed agree/disagree states handled honestly
- [x] clicking the filled body of a background-carrying group selects the group; unfilled body still starts marquee
- [x] export row suffix clearable in place; textInput empty-commit audited across call sites (rename guards intact)
- [x] Shift-click range selection works in the inspector Regions list (shared helper with layers)
- [x] clip ghost hidden by default, Alt-hold (or agreed toggle) reveals it
- [x] "Export project (N screens)" button label counts ALL groups; must count TOP-LEVEL visible only (matches exportProject since inc3; seen on screenshot: 1 top-level group shown as "2 screens")
- [x] addImages batched op: multi-file drop/paste = 1 journal entry, 1 undo; HTTP + CLI parity
- [x] layers: groups collapsed by default + selection path auto-expands; indent guides; panel width draggable
- [x] lead's live-verification UX notes on T0219 folded in (or explicitly none)
- [x] tests where testable + gates green

## Open questions

## Log
- 2026-07-02: Created from T0219 inc3 deferrals + T0206 build note. Lead directive: clean, no tech debt, no hacks.
- 2026-07-02: Built all 8 items. Design record:
  - **Item 1 (multi-group inspector):** new batched `patchGroups({projectId, groupIds, visible?, clip?})`
    op (mirrors patchElements) — ONE commitMutation, atomic id validation, clip:false clears to
    an absent field. HTTP `POST /groups-set` + CLI `groups-set --groups g1,g2 [--visible][--clip]`.
    Inspector `renderMultiGroup` shows "N groups" + tri-state shared toggles (checkbox `.indeterminate`
    when groups disagree; a click drives all via one op). Chose visible+clip only (per task); geometry
    stays per-group.
  - **Item 2 (filled-group-body click):** integrated a group-body hit INTO hitElement's front→back
    walk at the group's own z-slot (after its children, before siblings behind it) — a filled frame
    occludes what's behind it. `hasGroupFill` gates it; unfilled frames stay marquee-passthrough; the
    clip guard keeps clipped-out bodies unhittable. Generalized `resolveClickSelection` to tag groups
    (kind:"group") so the scope/drill model resolves a body click to the right container; fixed the
    Ctrl-click branch to selectGroupOnly for a group hit.
  - **Item 3 (suffix clear-in-place):** added `allowEmpty` opt to inspector `textInput`; only the
    Export **suffix** call passes it. Audited all textInput sites — element/group/text name + Export
    **scale** keep the non-empty guard (default). Region/project rename use inline.js (untouched).
  - **Item 4 (ghost/breadcrumb):** chief note = item 6 (done). No other breadcrumb/ghost bug found in
    the drill/scope code.
  - **Item 5 (region Shift-range):** extracted shared pure `rangeSelectIds(orderedIds, anchor, target)`
    in app.js; layers `selectRange` and the new inspector `selectRegionRow` both call it. Region rows
    get a module anchor + `selectRegionRange` (enters isolation, selects the run).
  - **Item 6 (clip ghost off by default):** `drawClipGhosts` gated on new view-state `state.clipGhostPeek`;
    Alt-hold sets it (canvas.js onKeyDown), Alt-release clears it (onKeyUp) + repaints. Never journaled/persisted.
  - **Item 7 (batched addImages):** new `addImages({projectId, images:[{name,bytes,x?,y?}]})` op — one
    commitMutation, up-front header validation (atomic), front-order hook extended to a batch. HTTP
    `POST /images-batch`, CLI `add-images --files a,b`. actions.js addImageFiles routes 1 file → addImage,
    2+ → addImages; drop AND paste flow through it.
  - **Item 8 (deep nesting):** (a) groups collapse by DEFAULT — state.collapsedGroups → state.expandedGroups
    (inverted); `revealSelectionPath()` auto-expands the selection's ancestor path on selection change only
    (manual collapse sticks). (b) VS Code indent guides via a CSS `::before` repeating-gradient in the indent
    gutter driven by a `--depth` var; normalized `.group-head` gap 6px→8px so head + row indent (and guides)
    align at every depth. (c) draggable right-edge `#layers-resize` handle; width persisted GLOBALLY
    (`canvas.layersWidth`, clamped 170–520). See NOTE below re: per-project.
  - **Item 9 (export label):** `visibleScreenCount()` uses `childrenOf(project, null).groups` (shared tree
    helper) filtered to visible — TOP-LEVEL only, matching exportProject. Wired into renderEmpty + inspectorSig.
  - NOTE (item 8c): task said "per project"; the two existing view prefs (canvas.layersCollapsed,
    canvas.inspector.collapsed) are GLOBAL, so per the "like other view prefs" clause I made width global
    for consistency + simpler apply-at-init. Flag for lead if per-project is preferred (trivial to key by id).
  - Tests: +9 (patchGroups ×3 in groups.test, addImages ×2 in batched.test, CLI parity ×2, API parity ×2).
    Gates: node --test → 204 pass/0 fail; validate_map --strict → 0 unmapped/0 missing; doc_reference_check → ok.
- 2026-07-11: T0375 status reconciliation: done; all 10 acceptance criteria are checked and the card log contains groups/export UX evidence.
