---
id: T0229
title: "Canvas export: save-file dialog with name (single) / zip archive (multi), drop suffix"
status: backlog
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

- [ ] single export output → save-file dialog with editable name (suggested = element/screen name); works into Downloads
- [ ] 2+ outputs → one zip via the same dialog (suggested <name>.zip); zip opens correctly on Windows
- [ ] suffix column gone from the panel; automatic name disambiguation for multi-row/multi-scale; legacy suffix field ignored on read, rejected loudly on new writes
- [ ] FSA-absent fallback = plain browser download (single file or zip)
- [ ] CLI export --to unchanged (agents keep directory output); tests + gates green
- [ ] directory-picker code path removed (export_dest.mjs repurposed/cleaned)

## Open questions

## Log
- 2026-07-03: Created from lead live-verify (export section). Replaces T0206 destination design (dir picker every time) after Chrome blocked system folders.
