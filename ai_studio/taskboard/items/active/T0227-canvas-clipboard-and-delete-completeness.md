---
id: T0227
title: "Canvas: copy/paste/duplicate of nodes + batched deleteNodes (Delete key completeness)"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
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

- [ ] Ctrl+C on any selection (elements/groups/mixed) captures a snapshot; Ctrl+V pastes it as ONE journal entry (new ids, offset placement, repeat-paste offsets again); works after the source is deleted
- [ ] Ctrl+D duplicates the selection in place (+offset), one entry
- [ ] OS-image paste unaffected: clipboard image wins over the internal buffer, deterministically
- [ ] deleteNodes op: mixed/multi selection Delete = one entry; undo deep-restores groups+elements at exact z-slots; Delete key wired for all selection shapes
- [ ] HTTP + CLI parity for paste/duplicate and deleteNodes; loud validation
- [ ] tests (incl. undo-exactness + paste-after-delete) + gates green

## Open questions

## Log
- 2026-07-03: Created from lead live-verify reports. Delete-single-group quick fix landed by orchestrator (canvas.js wiring to deleteGroupAction).
