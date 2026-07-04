# T0253 — Frontend-Performance Review (dimension 3/3): canvas site

Scope: `ai_studio/assets/canvas/site/*` paint loop + DOM + the op roundtrip / preview
paths that back it. READ-ONLY. Numbers below are from code tracing plus a Node
microbenchmark of the shared scene-tree helpers (`tree.mjs`), which `render()`,
`hitElement()`, and `layersSignature()` all pay on every call. No live server was booted
(the microbench isolates the load-bearing cost; :8780 untouched).

Honest bottom line: the app's own profile is healthy at the scale it's used today. The
end-of-day slowdown the lead saw is almost certainly the box, not this page — a flat
200-image project costs 0.04 ms of JS per frame. The real, app-owned pain is (a) the
1-2 s quantize/denoise preview, felt at ANY size, and (b) superlinear scaling that only
bites once a project has hundreds of elements spread across dozens of GROUPS.

---

## Strengths (already done right — keep)

- **rAF drag coalescing.** `requestRender()` (workspace.js:122-130) drops burst mousemoves
  to one paint/frame and covers pan/group/selection/element/marquee/region-move/region-resize/
  region-create/scale/rotate (all `requestRender` in `onMouseMove`, :1717-1915).
- **Drag rect caching.** `dragCanvasRect`/`dragStageRect` captured once at grab, cleared on
  mouseup/resize (:92-93, :1238-1247, :2395-2400) — kills the read-after-write reflow that a
  `getBoundingClientRect` in the mousemove path would otherwise flush every frame. This is the
  single most important thing they got right.
- **Backing store realloc guard.** `resizeCanvas` only assigns canvas.width/height when size or
  DPR actually changed (:110-113) — no per-frame realloc+clear.
- **Structure-signature DOM gating.** `inspectorSig` (inspector.js:2276-2356) and
  `layersSignature` (layers_panel.js:372-459): a selection-only change re-applies CSS classes
  instead of rebuilding. No listener leak — `replaceChildren()` detaches old rows and their
  listeners for GC.
- **Panels are NOT rebuilt mid-drag.** Drag frames call `requestRender()` (canvas only);
  `refresh()` (layers+inspector+history) fires only on mouseup/commit. The old
  per-frame marquee panel rebuild was explicitly removed (:1822-1825).
- **thumbCache** reuses `<img>` nodes across rebuilds keyed by id+src, pruned on delete/switch
  (layers_panel.js:57-78) — no re-download/re-decode on an op.
- **Image cache + immutable headers verified still true.** `imageCache` cleared on project
  switch (canvas.js:97); files served `cache-control: public, max-age=31536000, immutable`
  (api.mjs:186, :797).
- **applyMutation fast path.** Mutating responses carry `{project, history}`, so the page skips
  the old double-GET (app.js:443-462). Thin journal + sidecar snapshots.
- **Warm Python worker** serves quantize/denoise too (bridge.mjs:148-171) — the 1-2 s is real
  Pillow compute, not process spawn.
- **No per-keystroke commits.** Inspector text/number/prompt fields commit on `change`;
  quantize/quality use `input` only to drive the 350 ms-debounced preview (inspector.js:797-856).

---

## Findings, ranked by real-world impact

### F1 — Quantize/Denoise preview runs FULL-RES Pillow per slider stop (HIGH; bites at any size)
`runCleanupToolOnElement` quantizes the element's full `src` — a 1-4 Mpx PNG — on every
debounced change (ops.mjs:3939-3966; debounce inspector.js:797-823). The on-canvas preview is
only ever shown at screen scale (usually < 1 Mpx visible), so most of that 1-2 s is wasted on
pixels the user never sees at full detail. **This is the first slowness the lead actually
feels**, and it is independent of project size.
FIX: quantize/denoise a downscaled PROXY for the PREVIEW only (cap longest side ~1024 px), keep
`Apply` full-res. Expected 1-2 s → ~0.2-0.4 s. Size M.

### F2 — Wheel-zoom is the ONE hot path not rAF-batched (HIGH on large projects; trivial fix)
`onWheel` calls `render()` directly (workspace.js:2311) while every drag path uses
`requestRender()`. A high-res wheel/trackpad emits 30-100 events/s; each is a full synchronous
repaint (tree walk + every drawImage + resizeCanvas + per-frame DOM writes). Verifies the task's
suspicion: the rAF pass covered pan/marquee/snap/rotate/scale but **missed wheel zoom**.
FIX: route the paint through `requestRender()` (keep the viewport math + pan grabWorld rebase at
:2304-2310 synchronous). Size S (1-2 lines).

### F3 — Scene-tree helpers re-derive everything from scratch, superlinearly (MEDIUM-HIGH)
`groupMap()` allocates a fresh `Map` on every `isNodeHidden`/`ancestorsOf`/`childrenOf` call;
`orderedChildren` rebuilds `indexById` (O(E)) per scope and calls `subtreeGroupIds` (O(G²)) per
group child (tree.mjs:31-35, 74-129, 144-166). `render()` walks it per scope + `isNodeHidden`
per node (workspace.js:211-231); `hitElement` walks it too — **twice per idle mousemove**
(`updateHoverGroup` :2228-2242 and `updateCursorAt` :2164-2211, both via `onHover` :2213-2224);
`layersSignature` walks it on every `refresh`. Measured pure-JS walk (no drawImage yet):

| project | render walk/frame | hitElement (×2 per idle mousemove) |
|---|---|---|
| 200 el, 0 groups | 0.04 ms | 0.015 ms |
| 200 el, 20 groups | 0.34 ms | 0.22 ms |
| 500 el, 40 groups | 1.45 ms | 0.98 ms |
| 800 el, 60 groups | 3.35 ms | 2.72 ms |

Groups are the multiplier (0.04 → 0.34 ms at the same 200 elements). At 500+ el / 40+ groups,
merely moving the mouse over the canvas burns ~2 ms/event, and drag frames add ~1.5 ms of pure
bookkeeping on top of drawImage → dropped frames on a mid box. This is the "app runs warm" tax.
FIX: memoize per-project derived structure (one `groupMap`, per-scope `orderedChildren`, element
index) computed once per project version, invalidated in `ingestProject` (app.js:421-441).
Expected: back toward the 0.04 ms floor. Size M.

### F4 — Every mutation re-serializes + re-transfers + re-renders the WHOLE project (MEDIUM; scales with N)
Server per op: `snapshotOf(before)` + `snapshotOf(after)` each deep-clone the whole
elements+groups+tool_runs via `JSON.parse(JSON.stringify(...))` (ops.mjs:276-287), then TWO more
full `JSON.stringify` for the no-op check (:324), then `writeSnapshot` writes `{undo_patch,state}`
= ~2× project to disk (:326, store.mjs:507-511), then `writeProjectFile` pretty-prints the whole
project (~1.3×) to disk (store.mjs:83-86). Response returns the full project (~137 KB observed →
300-500 KB at 500+ el with regions/polygons); client parses it + full `refresh()`. So ~4-5 full
serializations + ~450 KB disk + ~137 KB down PER op. One op per gesture, so fine now; end-to-end
op latency grows linearly and is ~100-250 ms at 500 el.
FIX (incremental): drop the redundant no-op double-stringify; store snapshots as an undo
diff/patch, not two full copies. Longer term: delta responses. Size M-L. Watch-it, not urgent.

### F5 — render() does unconditional per-frame DOM writes (LOW-MEDIUM)
`updateZoomIndicator` (:1155), `updateScopeBreadcrumb` (walks ancestors + writes textContent,
:183-202), `updateBreadcrumb`/`updateRegionTools`/`updateEmptyHint`/`syncTextEditor` all run
every frame (:172-177). Cheap individually; the rect-cache already prevents the reflow these
would otherwise trigger. FIX: guard each behind a changed-value check. Size S.

### F6 — Chat rebuilds the whole transcript per SSE tick + reloads the project per op (LOW-MEDIUM)
`renderStream()` does `replaceChildren` + full rebuild of all turns on every `progress` event
(chat_panel.js:244-263, 338-341); `op-committed` calls a full `reloadProject()` (GET+refresh) per
committed op (:342-343, :355). Only during an active agent turn, not the interactive-edit path.
FIX: patch the streaming turn instead of rebuilding; coalesce `reloadProject` to turn-end. Size S-M.

### F7 — No dirty-rect / offscreen cache; full repaint of all N images every frame (LOW now, ceiling later)
`render()` clears and redraws every element each frame even when one is dragging (:140-178);
slice9 elements are ×9 drawImage (:318-330); `imageSmoothingEnabled` downsamples full 1-4 Mpx
sources with no mipmap at zoom < 2 (:145). Standard for a canvas editor, and modern HW eats 200
drawImages; but this becomes the dominant per-frame cost once F3 is fixed. FIX: cache static
(non-dragged) content to an offscreen canvas, or pre-downscale sources to a zoom-appropriate
proxy bitmap. Size L — do-differently, not a quick win.

### Startup (dim 5) — fine
Plain ES modules, ~20 files fetched individually on cold load (no bundler); negligible on
localhost. First paint is NOT font-gated — `render()` runs before `document.fonts.ready`, text
just skips until fonts resolve then repaints (fonts.js:42-82, paintTextElement :424). Minor:
`openProject` double-renders (`fit()` then `refresh()`, canvas.js:111-112) and does 3 sequential
GETs before first paint. Not a bottleneck.

### Memory (dim 4) — no leaks
`imageCache` cleared on switch (canvas.js:97); `thumbCache` pruned (layers_panel.js:75-78);
cleanupPreview is a single replaced slot; journal capped at 200 (`compactJournal`); chat turns
reset on switch/clear. One transient: each preview holds a 1-3 MB base64 data-URL Image until the
next replaces it (GC pressure, not a leak). Disk snapshots grow ~274 KB/op × ≤200 = tens of
MB/project (disk, bounded), not memory.

**N at which it hurts:** flat 200 images = fine. Pain starts ~400-500 elements across 30-40+
groups/screens (the actual use case). **First bottleneck the lead feels, any size:** the 1-2 s
quantize/denoise preview (F1). Next, on big grouped projects: wheel-zoom jank (F2) and
idle-hover warmth (F3).

---

## Do-differently (architecture)
1. Treat the scene tree as a per-project-version DERIVED index built once and read many times,
   not recomputed inside every helper call (F3). This one change de-risks scaling more than any
   micro-opt.
2. Previews should operate on a screen-scale proxy, not source pixels (F1) — the preview is a
   view artifact, full-res belongs only to Apply.
3. Undo snapshots as diffs, mutation responses as deltas (F4) — the "resend the whole project on
   every 1-px move" model is the linear tax that will eventually be felt.
4. A static-content offscreen layer is the real fix for high-N paint (F7), once F3 removes the JS
   walk from the critical path.

---

## Top-10 fixes

| # | Fix | Expected gain | Size | Files |
|---|---|---|---|---|
| 1 | Downscale proxy for quantize/denoise PREVIEW (full-res only on Apply) | 1-2 s → ~0.3 s/slider stop | M | ops.mjs:3939-3966, tools/image/{quantize,denoise}, inspector.js:800-823 |
| 2 | Route `onWheel` paint through `requestRender()` | kills zoom jank; N renders → 1/frame | S | workspace.js:2300-2312 |
| 3 | Memoize per-project tree (groupMap / per-scope orderedChildren / element index), invalidate in ingestProject | render+hover back toward 0.04 ms floor | M | tree.mjs, app.js:421-441, workspace.js, layers_panel.js |
| 4 | Compute `hitElement` once per mousemove, share between updateHoverGroup + updateCursorAt | halves idle-hover JS | S | workspace.js:2164-2242 |
| 5 | Return cleanup preview as binary PNG (blob URL), not base64-in-JSON | −33% payload + no multi-MB JSON.parse | S-M | api.mjs:697-704, inspector.js loadCleanupBitmap |
| 6 | Drop no-op double-stringify; snapshots as diff not 2 full copies | ~½-⅔ less server serialize+disk/op | M | ops.mjs:320-340, store.mjs:507-511 |
| 7 | Guard render()'s per-frame DOM writes behind changed-value checks | trims constant per-frame tax | S | workspace.js:1155-1177, 183-202 |
| 8 | Coalesce chat reloadProject to turn-end; patch (not rebuild) streaming turn | fewer full reloads/rebuilds per agent turn | S-M | chat_panel.js:244-263, 342-355 |
| 9 | Offscreen static-content cache or zoom-proxy bitmaps for drawImage | cuts per-frame paint at high N | L | workspace.js render/paintElement |
| 10 | Delta mutation responses (changed nodes only) | bounds per-op down-transfer + client parse at large N | L | api.mjs, ops.mjs, app.js:449-462 |
