---
id: T0220
title: "Legacy sweep: delete frozen viewer, close canvas seam, purge stale legacy refs"
status: doing
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-02
---

## What

Lead (2026-07-02, after the image-tools refactor landed): "замороженный viewer
удаляем" + "весь легаси, фаллбек пути, мы можем удалить? просканируй весь
проект, нужно убрать старые ссылки на легаси ассеты". One cleanup increment:
(1) DELETE the legacy asset_tools editor (ai_studio/assets/viewer/**) — canvas
replaced it; first MOVE what survives: the viewport math canvas imports
(asset_tools_viewport.mjs functions used by site/regions.js, actions.js,
dnd.js) moves into the canvas module; clarify fate of viewer/api.mjs (asset
LIBRARY gallery — if it is the source-first keystone surface it MOVES, not
dies). (2) With the viewer gone the frozen HTTP contract
(/api/asset-tools/raster2d/* + tmp prefix) loses its only client — remove or
rename per scan verdict. (3) CANVAS SEAM (T0218 increment 6): ops.mjs imports
-> image/{regions,sources}/api.mjs, bench.mjs repoint, delete raster2d/ shim +
its arch-map seam node. (4) Project-wide purge of stale legacy references
(tools/raster2d, tools/cutout, asset_prep links, key_to_alpha-as-current) in
skills/docs/code — taskboard/PLAN history lines stay (historical record).
Read-only scan dispatched 2026-07-02; execution AFTER the T0209 polygon agent
releases canvas/**.

## Done when

- [ ] ai_studio/assets/viewer/ deleted; canvas imports its own viewport module; asset-library gallery api relocated if it must survive; /asset_prep route + nav gone
- [ ] no client of /api/asset-tools/raster2d/* remains, handler+wiring removed (or renamed with a named client)
- [ ] canvas seam closed: ops.mjs on image/ bridges, bench.mjs repointed, raster2d/ folder + seam arch node deleted
- [ ] repo-wide grep for raster2d|cutout|asset_prep shows only historical records (taskboard/PLAN logs, tmp reports); skills reference image/ paths
- [ ] studio nav links the canvas prominently; the canvas page carries the shared AI Studio panel (nav to home/taskboard/map), no bare standalone page
- [ ] full suites green: canvas tests, image bridge tests, python suite, validate_map --strict, doc_reference_check; studio shell serves canvas + taskboard + map as before

## Open questions

## Log
- 2026-07-02: Created; scan agent dispatched (read-only). Viewer preview WIP already discarded by lead same day.
- 2026-07-02: Lead addition: studio site gets a clear path to the NEW canvas, and the canvas page itself must carry the AI Studio panel (shared studio shell header/nav like other surfaces) - canvas stops being a bare standalone page ("на канвасе нужно сделать мою панель от ии студии"). Fold into the sweep execution (studio_shell + canvas.html header).
- 2026-07-02: Lead clarified SCOPE (terminology collision resolved): the ASSET LIBRARY VIEWER (просмотр ассетов, gallery/source-first browsing) is NEEDED and SURVIVES - it is unrelated to canvas. Delete ONLY the legacy asset_tools EDITOR (asset_prep page: region editing, old alpha preview). If the gallery shares the folder, it MOVES to its own module, never dies.
