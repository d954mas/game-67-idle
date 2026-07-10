---
id: T0227
title: "Canvas: copy/paste/duplicate of nodes + batched deleteNodes (Delete key completeness)"
status: done
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-10
---

## What

Two gaps found by the lead during live verification (2026-07-03):

1. **"не работает копирование и вставку элементов и групп"** — Ctrl+C /
   Ctrl+V for canvas objects was never built (the page's Ctrl+V only pastes
   OS-clipboard IMAGES via dnd.js). Build Figma-like clipboard for nodes:
   - Ctrl+C captures the selection (elements AND groups as units) as a
     serialized spec snapshot (deep subtree: group defs + member element defs,
     style/meta/regions preserved; image file refs stay valid — files/ is
     immutable and content-addressed, so paste works even after the source
     was deleted).
   - Ctrl+V instantiates the snapshot via ONE op (new ids, pasted into the
     current scope, small offset or viewport-center placement — design call),
     one journal entry, one undo. Repeat pastes offset again.
   - Ctrl+D = duplicate selection in place (+offset), same op path.
   - OS-image paste must keep working: deterministic rule — if the paste
     event's clipboardData carries an image/file, the existing image path
     wins; the internal node buffer is used only otherwise. No ambiguity, no
     silent fallback.
   - Op + HTTP + CLI parity (agent can duplicate/paste nodes too). Copy
     buffer itself is page view-state (never journaled); the PASTE is the op.
   - Out of scope v1: cross-project paste, OS-clipboard interchange of node
     specs.

2. **Delete key completeness** — the handler ignored selected groups
   entirely (lead: "не работает удаление группы через delete кнопку").
   Quick fix landed same day: single selected group → deleteGroupAction
   (one entry). REMAINING: multi-group and MIXED (elements + groups)
   selections need batched op `deleteNodes({projectId, nodeIds})` — one
   commitMutation deleting elements and group subtrees together, one undo
   restores everything (exact z-slots per T0223's standard). Wire the
   Delete key mixed/multi branches to it; the single-element and
   single-group paths stay on their existing ops.

Laws: tool parity, thin page, one gesture = one journal entry, no silent
fallbacks, non-destructive (deep-restore on undo).

## Done when

- [x] Ctrl+C on any selection (elements/groups/mixed) captures a snapshot; Ctrl+V pastes it as ONE journal entry (new ids, offset placement, repeat-paste offsets again); works after the source is deleted
- [x] Ctrl+D duplicates the selection in place (+offset), one entry
- [x] OS-image paste unaffected: clipboard image wins over the internal buffer, deterministically
- [x] deleteNodes op: mixed/multi selection Delete = one entry; undo deep-restores groups+elements at exact z-slots; Delete key wired for all selection shapes
- [x] HTTP + CLI parity for paste/duplicate and deleteNodes; loud validation
- [x] tests (incl. undo-exactness + paste-after-delete) + gates green

## Open questions

## Log
- 2026-07-03: Created from lead live-verify reports. Delete-single-group quick fix landed by orchestrator (canvas.js wiring to deleteGroupAction).
- 2026-07-03: Built both items (deep-reasoner). 215 tests green (204 baseline + 11 new); map --strict + doc_reference_check green.
  - **Spec format** (`tree.buildNodesSpec`, pure, shared by page copy buffer + CLI): `{schema:"ai_studio.canvas.nodes_spec.v1", nodes:[...]}`. Each node is `{kind:"element", element:<deep-clone minus id/groupId/order>}` or `{kind:"group", group:<clone minus id/parentId/order>, children:[...]}`; group children are captured in `orderedChildren` (back→front) so internal z-order round-trips. Image element clones keep `src` (immutable content-addressed file) — paste stays valid after the source is deleted. Roots emitted back→front within each scope (grouped by first-seen scope).
  - **Ops** (ops.mjs, one commitMutation each): `pasteNodes({projectId, spec, dx, dy, scopeId})` validates the WHOLE spec loudly before any mint/write (unknown file ref / malformed node / empty spec throws atomically), mints ids server-side, shifts every node by (dx,dy), instantiates into `scopeId` (null=root). `duplicateNodes({projectId, nodeIds, dx, dy, scopeId})` = build spec from live ids + delegate to pasteNodes (still one entry); default `+16,+16`, default scope = originals' common scope. `deleteNodes({projectId, nodeIds})` deletes loose elements + whole group subtrees (deduped) in one entry; undo deep-restores at exact z-slots (snapshot).
  - **Placement/offset rule**: page holds `state.clipboard={spec, pastes}` (view-state, never journaled). Each Ctrl+V increments `pastes` and offsets `dx=dy=16*pastes` (first paste +16 from source; repeat pastes step again from the last). Ctrl+D uses a fixed +16 via duplicateNodes. Pasted/duplicated ROOTS are auto-selected (Figma).
  - **Paste-event ownership**: the window `paste` listener in dnd.js is the SINGLE owner of Ctrl+V. Deterministic: OS image FILE in clipboardData → existing image path wins; else (not typing, not region-edit) the internal node buffer pastes. canvas.js keydown deliberately does NOT handle Ctrl+V (only Ctrl+C capture + Ctrl+D duplicate), so no double-paste.
  - **Delete key**: region-edit unchanged; elements-only → `deleteSelectedElements` (elements-remove); single group → `deleteGroupAction`; MIXED or MULTI-group → new `deleteNodes`. Every shape = one journal entry.
  - **Z-order on paste**: top-level roots use the frontOrder hook (explicit scope → front orders in spec order = on top preserving relative order; implicit scope → array-append order, same effect); nested pasted scopes are fresh so children get explicit contiguous 0..N-1 (exact internal z).
  - **Parity**: HTTP `POST nodes-paste`/`nodes-duplicate`/`nodes-delete`; CLI `nodes-paste --spec file.json`, `nodes-duplicate --nodes id1,id2`, `nodes-delete --nodes id1,id2` (all `[--dx --dy] [--group gid|none]` where relevant).
  - **Deliberately deferred**: NO context-menu Copy/Paste entries added (menu diet respected; allowed but not required by done-when — keyboard + CLI/HTTP cover it). The empty-canvas menu's existing "Paste" stays OS-image-only. Cross-project paste + OS-clipboard node-spec interchange remain out of scope v1 as specified.
- 2026-07-11: T0375 status reconciliation: done; all 6 acceptance criteria are checked and the card log contains clipboard/delete verification evidence.
