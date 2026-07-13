---
id: T0223
title: "Canvas: multi-gesture op integrity - batched mixed move, batched reorder, true ungroup op"
status: done
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-10
---

## What

Pay down the journal-integrity compromises documented during T0219 (lead
2026-07-02: "хочу чтобы было хорошо, чисто, без техдолга и хаков"). The law is
absolute: EVERY user gesture = exactly ONE journal entry = one undo = one HTTP
call. Three violations exist today, all documented in the T0219 log:

1. **Mixed marquee move** (groups + loose elements selected together) commits
   N+1 entries (one elements-set batch + one patchGroup per group). Fix: op
   `moveNodes({projectId, moves:[{nodeId, x, y}...]})` - elements and groups
   in one call, group deltas cascade their subtrees, one commitMutation. Page
   marquee-move commit rewires to it; single-node paths stay on their ops.
2. **Multi-select Ctrl+[ / Ctrl+]** is a no-op because reorderNode is
   single-node (inc2 rejected a per-node loop = N entries, correctly). Fix: op
   `reorderNodes({projectId, nodeIds, direction|index})` - moves the selected
   same-scope siblings as a block preserving relative order (Figma semantics),
   one entry; cross-scope selection applies per-scope but still ONE entry
   (single commitMutation over the whole change).
3. **Ungroup is page-composed** (assign children up + delete empty group -
   multiple calls) and drops children at the parent scope FRONT instead of the
   group's former z-slot. Fix: op `ungroupGroup({projectId, groupId})` - one
   entry, children (elements AND subgroups) land in the parent scope AT the
   group's former sibling position preserving their internal relative order;
   one undo restores the group exactly.

All three: HTTP routes + CLI parity (nodes-move / nodes-reorder / group-
ungroup), loud validation, tests incl. undo-restores-everything and z-slot
exactness. After this lands, grep the page for any remaining multi-call
gesture and list what's left (should be none).

## Done when

- [x] moveNodes: mixed element+group move = 1 entry, 1 call; group subtrees cascade; page marquee-move commit uses it; undo restores all positions
- [x] reorderNodes: multi-selection Ctrl+[/] and Order actions work on blocks preserving relative order; 1 entry; render honors it
- [x] ungroupGroup op: 1 entry; children keep the group's former z-slot and internal order; undo restores the group exactly; page Ungroup uses it
- [x] HTTP + CLI parity for all three; loud errors on invalid input
- [x] audit: no user gesture on the page produces more than one journal entry (report the grep/walkthrough in the task log) — one exception found & deferred (multi-file image drop)
- [x] tests + gates green

## Open questions

## Log
- 2026-07-02: Created from T0219 documented compromises (inc2 log: multi-nudge rejected to protect one-entry law; inc3 log: mixed move N+1, Ungroup front-not-slot). Lead directive: clean, no tech debt.
- 2026-07-02 (build): Landed all three ops in `ops.mjs` + HTTP routes (`api.mjs`) + CLI
  commands (`cli.mjs` `nodes-move`/`nodes-reorder`/`group-ungroup`). Pure ordering math
  added to `tree.mjs` as `blockReorder` (front/back/forward/backward + absolute index),
  not in ops.
  - **moveNodes({projectId, moves:[{nodeId,x,y}]})**: absolute targets; a group carries its
    delta across its full descendant closure exactly like the single patchGroup path.
    Overlap-safe design decision (within latitude): a node's shift = the delta of the
    TOPMOST moved node in its ancestor-or-self chain (via `ancestorsOf`), so selecting a
    parent frame AND a node inside it moves the child ONCE, never twice. Page
    `commitSelectionDrag` (workspace.js) rewired to it; single-element/single-group drags
    stay on `patchElements`/`patchGroup` (already one entry each).
  - **reorderNodes({projectId, nodeIds, direction|index})**: selected same-scope siblings
    move as one block (relative order preserved); cross-scope applies per scope in ONE
    commitMutation; `index` restricted to single-scope (throws cross-scope). Turned ON
    multi-select Ctrl+[/] (canvas.js — removed the deliberate no-op `soloNodeId`, now routes
    2+ selection to `reorderNodesBy`) and made the context-menu Order submenu block-aware
    (context_menu.js), incl. showing Order for a 2+ element selection alongside Group.
  - **ungroupGroup({projectId, groupId})**: one entry; direct children (elements AND
    subgroups) replace the group's node at its former z-slot in the parent scope, internal
    order preserved via contiguous re-order; grandchildren untouched; undo deep-restores the
    group. Replaced the page-composed `ungroup` (was N calls, children→front).
  - Loud validation everywhere (empty moves/nodeIds, unknown node/group, non-finite x/y,
    both/neither direction|index, index cross-scope) — atomic, no partial writes.
  - Tests: new `tests/multi_gesture.test.mjs` (13 tests: ops + HTTP adapter + CLI parity,
    incl. undo-restores-everything, z-slot exactness, overlap-safety, cross-scope one-entry).
  - **Gate results**: `node --test` = **194 pass / 0 fail** (was 181 baseline + 13 new);
    `validate_map.mjs --strict` = mapped=302 scanned=412 unmapped=0 missing=0 duplicates=0;
    `doc_reference_check.mjs` = ok, 10 md files checked. Docs: canvas README updated (ops +
    CLI + Ungroup semantics); no new files, so architecture map needed no change.
  - **Gesture audit (walkthrough)** — each gesture → journal entries → op:
    add-image(single)→1 addImage; add-image(multi-file drop)→**N addImage** (one per file,
    OUT OF SCOPE — see below); add-text→1 addText; drag single element→1 patchElements;
    drag element-only marquee→1 patchElements; drag single group→1 patchGroup; drag mixed
    /2+group→**1 moveNodes** (was N+1); delete single/multi→1 removeElements; reorder single
    (Ctrl+[/], Order, layers drag)→1 reorderNode/reorderElement/reparentGroup; reorder multi
    (Ctrl+[/], Order)→**1 reorderNodes** (was a no-op); group (Ctrl+G/menu)→1 createGroup;
    **ungroup→1 ungroupGroup** (was N); delete group→1 deleteGroup; slice→1 slice; region
    add/move/resize/delete + polygon→1 setRegions each; text edit→1 patchElement; export
    settings→1 setExportSettings (export itself not journaled); reparent (layers drag)→1
    reparentGroup/assignToGroup; group bg/clip/fit/rename/visibility→1 patchGroup/fitGroup;
    rename element/project + visibility→1 patchElement/patchProject.
  - **Discovered / DEFERRED (out of the three ops' scope)**: multi-file image drop
    (`addImageFiles`, actions.js) still issues one `POST /images` per file = N journal
    entries for one drop gesture. Fixing needs a new batched `addImages` op (not in
    moveNodes/reorderNodes/ungroupGroup scope), so left for the lead to schedule.
- 2026-07-11: T0375 status reconciliation: done; all 6 acceptance criteria are checked and the card log contains multi-gesture ops verification evidence.
