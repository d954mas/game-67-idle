---
id: T0220
title: "Legacy sweep: delete frozen viewer, close canvas seam, purge stale legacy refs"
status: done
project: P001
epic: E010
priority: P1
tags: []
created: 2026-07-02
updated: 2026-07-10
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

- [x] ai_studio/assets/viewer/ deleted; canvas imports its own viewport module; asset-library gallery api relocated if it must survive; /asset_prep route + nav gone
- [x] no client of /api/asset-tools/raster2d/* remains, handler+wiring removed (or renamed with a named client)
- [x] canvas seam closed: ops.mjs on image/ bridges, bench.mjs repointed, raster2d/ folder + seam arch node deleted
- [x] repo-wide grep for raster2d|cutout|asset_prep shows only historical records (taskboard/PLAN logs, tmp reports); skills reference image/ paths
- [x] studio nav links the canvas prominently; the canvas page carries the shared AI Studio panel (nav to home/taskboard/map), no bare standalone page
- [x] full suites green: canvas tests, image bridge tests, python suite, validate_map --strict, doc_reference_check; studio shell serves canvas + taskboard + map as before

## Open questions

## Log
- 2026-07-02: Created; scan agent dispatched (read-only). Viewer preview WIP already discarded by lead same day.
- 2026-07-02: Lead addition: studio site gets a clear path to the NEW canvas, and the canvas page itself must carry the AI Studio panel (shared studio shell header/nav like other surfaces) - canvas stops being a bare standalone page ("на канвасе нужно сделать мою панель от ии студии"). Fold into the sweep execution (studio_shell + canvas.html header).
- 2026-07-02: Lead clarified SCOPE (terminology collision resolved): the ASSET LIBRARY VIEWER (просмотр ассетов, gallery/source-first browsing) is NEEDED and SURVIVES - it is unrelated to canvas. Delete ONLY the legacy asset_tools EDITOR (asset_prep page: region editing, old alpha preview). If the gallery shares the folder, it MOVES to its own module, never dies.
- 2026-07-02: EXECUTED (deep-reasoner agent, tmp/t0220_legacy_sweep_plan_2026-07-02.md) and ACCEPTED: I re-ran all gates green (canvas 91/0, image bridge 4/0, gallery 37/0, python 30 OK, validate_map --strict clean, doc_reference_check ok), footprint clean, committed 494ae3dd (-4368 lines), live :8780 restarted. 61 files: 19 deleted, 16 git-mv viewer/->gallery/, viewport.mjs extracted into canvas. Agent deviations accepted: /asset_prep replaced (not just purged) with /canvas in all 5 shared navs; extra stale doc refs fixed.
- 2026-07-02: Lead REJECTED the agent's ad-hoc top strip ("у меня есть левое меню... все страницы его добавляют") - reworked to the standard collapsible AI Studio left sidebar (studio_shell.css/.js, same markup as taskboard/quality pages, Canvas item active) in 59eb9df0. -> review.
- 2026-07-11: T0375 status reconciliation: done; all 6 acceptance criteria are checked and the card log contains legacy-sweep verification evidence.
