---
id: T0223
title: "Canvas: multi-gesture op integrity - batched mixed move, batched reorder, true ungroup op"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
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

- [ ] moveNodes: mixed element+group move = 1 entry, 1 call; group subtrees cascade; page marquee-move commit uses it; undo restores all positions
- [ ] reorderNodes: multi-selection Ctrl+[/] and Order actions work on blocks preserving relative order; 1 entry; render honors it
- [ ] ungroupGroup op: 1 entry; children keep the group's former z-slot and internal order; undo restores the group exactly; page Ungroup uses it
- [ ] HTTP + CLI parity for all three; loud errors on invalid input
- [ ] audit: no user gesture on the page produces more than one journal entry (report the grep/walkthrough in the task log)
- [ ] tests + gates green

## Open questions

## Log
- 2026-07-02: Created from T0219 documented compromises (inc2 log: multi-nudge rejected to protect one-entry law; inc3 log: mixed move N+1, Ungroup front-not-slot). Lead directive: clean, no tech debt.
