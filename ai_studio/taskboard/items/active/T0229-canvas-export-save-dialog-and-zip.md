---
id: T0229
title: "Canvas export: save-file dialog with name (single) / zip archive (multi), drop suffix"
status: review
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-03
updated: 2026-07-03
---

## What

Lead live-verify 2026-07-03, export section — three calls that REPLACE the
T0206 destination design (directory picker every export):

1. **BROKEN**: Chrome's showDirectoryPicker refuses system folders — the
   lead cannot save into Загрузки/Downloads ("ругается что системная
   папка"). The directory-picker flow is a dead end for the most natural
   destination; drop it as the primary path.
2. **Single output** → a SAVE-FILE dialog where the lead can type/change
   the FILE NAME (showSaveFilePicker with suggestedName = element/screen
   name + format extension; browser download with the same suggested name
   as fallback when FSA is absent/denied).
3. **Multiple outputs** → ONE ZIP archive (Figma behavior), saved via the
   same save-file dialog (suggestedName = <project or selection>.zip).
   Server builds the zip: export files already materialize server-side
   under <project>/export/<stamp>/ — add a zip step (STORE-mode zip, no
   compression: PNG/JPG/WebP are already compressed; hand-rolled minimal
   writer or node built-ins only — NO new npm deps).
4. **Suffix is dropped** ("suffix не нужен") — remove the suffix column
   from export rows (UI + op validation); file naming becomes automatic:
   element/screen name, with automatic disambiguation when several rows
   target the same element (Figma-style scale marker, e.g. "name@2x.png",
   only when needed — single 1x row = clean name). Existing stored rows
   with a suffix: ignore the field on read (additive schema change,
   journal untouched); op stops accepting it loudly? — NO: accept-and-drop
   would be a silent fallback; decide explicitly: setExportSettings
   rejects suffix in NEW writes (loud), readers ignore legacy field.
   Record the decision in the log.

Laws: tool parity (CLI export --to <dir> path stays for agents — the zip/
dialog flow is the PAGE destination; CLI keeps writing files/dirs
directly), thin page, one gesture one entry (export settings ops
unchanged), no silent fallbacks, non-destructive. The
<project>/export/<stamp>/ automation default stays for CLI/agents.

## Done when

- [x] single export output → save-file dialog with editable name (suggested = element/screen name); works into Downloads
- [x] 2+ outputs → one zip via the same dialog (suggested <name>.zip); zip opens correctly on Windows
- [x] suffix column gone from the panel; automatic name disambiguation for multi-row/multi-scale; legacy suffix field ignored on read, rejected loudly on new writes
- [x] FSA-absent fallback = plain browser download (single file or zip)
- [x] CLI export --to unchanged (agents keep directory output); tests + gates green
- [x] directory-picker code path removed (export_dest.mjs repurposed/cleaned)

## Open questions

## Log
- 2026-07-03: Created from lead live-verify (export section). Replaces T0206 destination design (dir picker every time) after Chrome blocked system folders.
- 2026-07-03: BUILT (code-complete; all three gates green — 232 canvas tests pass incl. Python-backed; validate_map --strict exit 0, unmapped_ai_studio=0; doc_reference_check ok). Browser live-verify (real Downloads write + Windows unzip) is deferred to the orchestrator's live session. Design record:
  - **Item 1 — single output → save-FILE dialog.** New `site/export_dest.mjs` = `saveBlobToFile(blob, suggestedName, types)`: `showSaveFilePicker` seeded with the file's own name. Abort (`AbortError`) = quiet cancel → info toast "Отмена в диалоге — экспорт отменён."; any OTHER picker/write error THROWS (loud → error toast, NO fallback); plain browser download runs ONLY when `showSaveFilePicker` is absent. `actions.js deliverExport` fetches the one image over the existing `/export/<stamp>/<file>` route and calls it.
  - **Item 2 — multiple outputs → one STORE zip.** New `zip.mjs` (`zipStore`, node built-ins only, no npm): STORE method 0 (no compression — PNG/JPG/WebP already compressed); local + central-dir headers + EOCD; UTF-8 name flag; CRC-32 from `zlib.crc32` (Node 24.15 has it — verified) with a table-based fallback. `ops.zipExport({projectId, stamp})` reads the run's `manifest.json`, gathers the produced image files (not the manifest/specs), bundles them. Served over new **`GET /api/canvas/projects/<id>/export-zip/<stamp>`** (`application/zip`); the page fetches it and saves via the same dialog, suggested `<element-or-project title>.zip`. Verified by unzipping in-test (EOCD count + per-entry CRC + verbatim bytes).
  - **Item 3 — suffix removed + automatic naming.** `cleanExportRows(rows, {rejectSuffix})`: `setExportSettings` passes `rejectSuffix:true` → any row carrying a `suffix` field (even "") is rejected LOUDLY; the export readers (rowsForElement / override rows) leave it false and simply DROP a legacy stored suffix (no write-back). **Naming rule** (documented): base = slug(element/screen name); a SINGLE row = clean `name.<ext>`; SEVERAL rows on one element get a Figma **scale marker** — `1x` → "" (stays clean), else `@<token>` (`@2x`, `@0.5x`, `@512w`); any remaining collision (same scale+format twice, or two elements sharing a name) gets a deterministic numeric `_NN` against the run-wide used-set (order-stable).
  - **Item 4 — directory-picker path removed.** `export_dest.mjs` rewritten (no IndexedDB, no `showDirectoryPicker`, no `writeFilesToDir`/`pickDestination`/`lastDestinationName`); `inspector.js` drops the Suffix column, the destination-hint line + its 3 call sites, and the now-unused `textInput` `allowEmpty` opt-out; dead `.insp-export-dest` CSS removed.
  - **Parity / laws.** CLI `export --to <dir>` and the confined `<project>/export/<stamp>/` default UNCHANGED; added optional `--zip <path>` (same `zipExport` archive) for parity; `--suffix` flag removed. Export runs stay un-journaled. No silent fallbacks. Files touched: `ops.mjs`, `api.mjs`, `cli.mjs`, `zip.mjs` (new), `site/{export_dest.mjs, actions.js, inspector.js, canvas.css}`, `tests/{export.test.mjs, zip.test.mjs (new)}`, architecture map, canvas README.
