---
id: T0204
title: "Canvas history panel: Photoshop-like hideable list + jump-to-step op"
status: done
project: P001
epic: E010
priority: P2
tags: []
created: 2026-07-02
updated: 2026-07-10
---

## What

Photoshop-style history panel over the existing journal: a hideable panel listing operations (icon + label + time), current position marked; clicking an entry jumps the project to that step (new jumpHistory op applying undo/redo steps to reach the target seq - CLI parity: history-jump <id> --seq N). Grayed redo-tail entries stay clickable. Depends on T0201 compaction semantics for what is listed.

## Done when

- [x] panel toggles from the toolbar (and a hotkey), hidden by default, remembers its state
- [x] clicking any listed step restores that project state; canvas/layers/inspector re-render consistently
- [x] CLI history-jump reaches the same states as the panel
- [x] journal tests extended for jumpHistory (incl. jumping into the redo tail)

## Open questions

## Log
- 2026-07-02: Scoped in ai_studio/assets/canvas/PLAN.md (2026-07-02); perf items anchored to bench tmp/canvas_bench_2026-07-02.json + perf research
- 2026-07-03: Built. Backend (ops.mjs): `jumpHistory({projectId, seq})` + `listHistory` + exported pure `historyEntryLabel` + a `historySpine` helper. Design record — jumpHistory is history NAVIGATION, not a new mutation: it moves the applied head along the current linear spine (undo chain ∪ redo tail, computed by mirroring undoOp's parent-walk + redoOp's greatest-seq-child walk, so stale branches are never reachable) by restoring the target seq's EXISTING sidecar snapshot (`state` for a real entry; the oldest retained entry's `undo_patch` for base=0) and repointing `history_seq`. That makes an N-step jump provably identical to N undos/redos with ZERO recomputation, in ONE HTTP call. It appends only a `{op:"jump",target_seq,from_seq}` nav marker (like undo/redo markers — no snapshot, `isMutation`=false), so no parent pointers change and no compaction runs (jump never grows depth); undo/redo/jump from the new head stay coherent and a jump is itself reversible. Loud on non-integer/negative/off-spine seq; no-op (no marker) when already at head. listHistory returns `{seq,label,summary,current,undone}` rows (Base + applied undo chain + dimmed redo tail) so both clients render with no journal parsing. Clients: `GET /history-list`, `POST /history-jump` (folds history flags via sendMutation); CLI `history-list` + `history-jump --seq N`. Page: new `site/history_panel.js` — a floating Photoshop palette toggled from a top-bar **History** button + the backtick key, hidden by default, open-state in localStorage (view-state, never journaled); renders through the refresh bus (instant head re-highlight from the folded op response, then a structure-signature-guarded list re-fetch); row click → one `jumpToHistory` op (quiet like undo/redo). Undo/redo keyboard shortcuts untouched; no context-menu items added. Gates green: canvas tests 223 (215 baseline + 8), `validate_map.mjs --strict` (unmapped_ai_studio=0), `doc_reference_check.mjs` ok. Map + canvas README updated.
- 2026-07-11: T0375 status reconciliation: done; all 4 acceptance criteria are checked and the card log contains history-panel verification evidence.
