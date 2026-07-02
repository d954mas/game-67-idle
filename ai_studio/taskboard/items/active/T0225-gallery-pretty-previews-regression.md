---
id: T0225
title: "Asset viewer: pretty previews regressed after gallery module move"
status: backlog
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Found by the lead during the 2026-07-02 live verification pass, right after
the T0220 sweep moved the gallery module to `ai_studio/assets/gallery/`
(routes `/asset_viewer/` + `/viewer/` kept): "В asset viewer пропали красивые
превью. Там теперь генерация а не красивое превью" - the gallery used to show
the curated/pretty preview images; now it shows the generation(-ish) image
instead. Suspect the module move broke a preview-resolution path (preview
file lookup relative to the old module location, or the preview route/asset
path not repointed in the T0220 move).

Investigate: how the gallery resolved "pretty previews" before commit
`494ae3dd` (T0220) vs now; check preview-related routes/paths in
`ai_studio/assets/gallery/` against the pre-move `ai_studio/assets/viewer/`
history. Restore the previews exactly as they were; add a regression test if
the resolution logic is testable.

## Done when

- [ ] root cause identified and written in this task's log (what the move broke)
- [ ] pretty previews show again in /asset_viewer/ exactly as before the move
- [ ] regression guard where testable; gallery test suite + gates green
- [ ] lead re-verified live

## Open questions

## Log
- 2026-07-02: Created during lead's live verification (checklist item: gallery works as before - FAILED on previews).
