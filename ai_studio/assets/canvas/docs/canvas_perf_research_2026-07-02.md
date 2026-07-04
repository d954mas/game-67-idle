# Canvas module performance research (2026-07-02)

READ-ONLY research. Base commit 0c036d00. All file:line anchors are from that base.
Empirical numbers from `tmp/canvas_bench_2026-07-02.json` (peer agent) plus three of
my own micro-experiments (fs cloud-vs-local, journal-append growth, Python import
breakdown). No repo files edited; the scoped `.bench-tmp` under the canvas root was
created and fully removed (verified).

## TL;DR

Two independent bottlenecks, each an order of magnitude bigger than the ops the bench
already measures cheap:

1. **Python cold-spawn per pipeline step** — detect = 2 spawns (~542 ms), slice = 3
   spawns (~828 ms). ~165 ms/spawn is *just* `import numpy; import PIL` before any pixel
   work. This is the "slice feels sluggish".
2. **Journal grows O(project) per line and is re-parsed whole on every op** — each
   mutation writes TWO full-project snapshots (`undo_patch` + `state`); `appendJournal`
   *and* `readHistory` both read+parse the entire file every op. Measured: append rises
   1 ms → 108 ms as the journal reaches 1000 ops (28 MB). This is O(n²) over a session
   and is the "delete feels sluggish, and worse the longer I work" — the part the bench
   under-reports because its fixtures use fresh, empty journals.

Frontend churn (full `reloadProject` GET + second `/history` GET + full layers rebuild
that recreates every thumbnail `<img>`, + per-frame canvas backing-store realloc during
drag) sits on top and multiplies perceived jank on *every* op.

**YandexDisk is NOT a primary cause** (confirmed by experiment): atomic write+rename on
the cloud folder is only ~10 % slower than `os.tmpdir` (0.50 ms vs 0.46 ms median).

---

## 1. End-to-end cost model per hot action

### Server op layer, per mutating op (the shared cost)

Every journaled mutation (`removeElement`, `patchElement`, `createGroup`, …) runs this
sequence. Using `removeElement` (`ops.mjs:138-148`) as the example:

| # | Step | Code | Cost O() |
|---|------|------|----------|
| 1 | `getProject(before)` | ops.mjs:139 → store.mjs:177-179 | read+parse project.json — O(P) |
| 2 | `storeRemoveElement` reads project again | store.mjs:273-279 | O(P) read |
| 3 | …then `updateProject` reads a 3rd time + writes | store.mjs:183-188, 274,277 | O(P) read + O(P) atomic write |
| 4 | `commitMutation`: `snapshotOf(before)` + `snapshotOf(after)` | ops.mjs:98-100, 82-93 | 2× deep clone `JSON.parse(JSON.stringify(...))` — O(P) each |
| 5 | equality guard `JSON.stringify(a)===JSON.stringify(b)` | ops.mjs:101 | 2× serialize — O(P) |
| 6 | `appendJournal`: **reads+parses the WHOLE journal** to get max seq | store.mjs:336-343 (esp. 339) | **O(J)** where J = total journal bytes |
| 7 | append one line = `{undo_patch(P), state(P)}` | store.mjs:341 | writes ~2P bytes |
| 8 | `updateProject` (set history_seq): read 4th time + write 2nd time | ops.mjs:109 → store.mjs:183-188 | O(P) read + O(P) write |

So one op = **4 reads + 2 writes of project.json, 4 deep O(P) clone/serialize passes, one
O(J) full-journal parse, and one ~2P-byte journal append.** `P` = project.json size (grows
with elements + regions + tool_runs); `J` = journal size (grows ~2P per op → O(n·P)).

Measured (bench, in-proc, fresh journal): `removeElement` 5 el 3.1 ms / 25 el 4.3 ms /
100 el 14.6 ms; `patchElement` 100 el 11 ms; `undo` 100 el 15.4 ms. The super-linear jump
25→100 is the multiple O(P) JSON passes. **These numbers omit step 6's O(J) growth** —
see §3.

### Then the frontend, per op (`reloadProject`, app.js:231-240)

1. `GET /projects/<id>` — full project.json (grows with regions/tool_runs). Payload O(P).
2. `refreshHistory` → **second** `GET /projects/<id>/history` (app.js:216-227, 237) →
   `readHistory` re-reads+parses the whole journal server-side — O(J).
3. `refresh()` (app.js:38-43) fans to renderCanvas + renderLayers + renderInspector +
   syncTopBar — all full rebuilds (§5).

So **2 HTTP round trips + a second full-journal parse on every single op**, even a 1-px
move, and the op's own return value (which already contains the updated project) is
discarded.

### Per-action specifics

- **Element delete** (`deleteElements`, actions.js:97-106): loops `for (id of ids) await
  api("DELETE", …)` — **N sequential HTTP calls**, each the full server op above (each an
  O(J) journal append + N separate journal entries), then ONE `reloadProject`. Deleting a
  20-element multi-select = 20 sequential atomic project.json rewrites + 20 journal
  appends, each re-parsing the growing journal.
- **Drag-move** (workspace.js): `onMouseMove` calls `render()` per mouse event with no rAF
  (workspace.js:324) — and `render()` reallocates the canvas backing store every frame
  (§5). On release, `onMouseUp` (workspace.js:335-351) does **N sequential PATCH** (one per
  selected element, each journaled) + `refreshHistory` + `reloadProject` (2 more GETs). A
  mid-drag pause >300 ms also fires `scheduleMoveSave` (workspace.js:238-255, 325) → extra
  N PATCHes → extra journal entries for a single drag.
- **Detect** (`detectRegions`, ops.mjs:441-493): read element bytes → base64 →
  `uploadRaster2dSource` (writes a session source file) → `detectRaster2dRegions` which
  spawns Python **twice** (normalize_background.py, then detect_regions.py; raster2d
  api.mjs:318, 321-332) → then the standard 4-read/2-write/journal op. Measured **542 ms**.
- **Slice** (`sliceRegions`, ops.mjs:505-594): re-reads bytes → base64 →
  `uploadRaster2dSource` again → `detectRaster2dRegions` **again** (normalize+detect, 2
  spawns) *just to obtain the normalized keyed image* → `exportRaster2dRegions` (3rd spawn,
  slice_regions.py) → **per-slice loop** `storeAddImage` (ops.mjs:550-569), each = 2 reads
  + 1 write of the growing project.json → extra `updateProject` for tool_runs (579-581) →
  commitMutation. So **3 Python spawns + N sequential project rewrites + a redundant
  re-detect**. Measured **828 ms**.
- **Undo/redo** (ops.mjs:376-414): `getProject` + `readJournal` (full parse, O(J)) to
  find the head/candidate entry + `updateProject` (read+write) + `appendJournal` (another
  O(J) parse for the marker). Two full-journal parses per undo. Measured 100 el 15.4 ms
  (fresh journal); grows with J.

---

## 2. Python bridge — spawn cost vs useful work, and the fix

**How it spawns.** Both the raster2d bridge (`tools/raster2d/api.mjs:110-131`, `runPython`)
and the canvas render tool (`ops.mjs:692-713`) spawn a fresh interpreter per call via
`execFile`, walking a candidate list (`pythonCandidates`, raster2d api.mjs:86-108 /
ops.mjs:670-688). No session reuse, no warm process — every step is a cold `python.exe`.

**Cost breakdown (my experiment, py 3.12, median of 5 cold spawns):**

| Import | Cold total |
|---|---|
| bare `python -c pass` | 51.5 ms |
| `+ from PIL import Image` | 82.8 ms |
| `+ import numpy` | 130.5 ms |
| `import numpy; from PIL import Image,ImageDraw` | **164.8 ms** |

So each `normalize_background.py` / `detect_regions.py` spawn pays ~165 ms of interpreter
+ numpy + PIL startup *before* touching a pixel. Mapping to the bench:

- detect = normalize + detect = ~330 ms imports of 542 ms total → **~60 % is spawn/import**.
- slice = normalize + detect + slice(PIL-only ~83 ms) ≈ 413 ms imports of 828 ms → **~50 %**.

**Recommended design: one persistent Python worker (JSON-RPC over stdio), behind the
single op layer.** Fits "one op layer, thin clients" because it lives *inside* the
raster2d bridge — the site and CLI keep calling the same ops functions and simply get
faster; they never learn a worker exists.

- **Process.** Replace `runPython`'s per-call `execFile` with a lazily-spawned long-lived
  `child_process.spawn(python, [worker.py], {windowsHide:true})`. The worker imports
  numpy+PIL+the tool modules once at startup, then loops reading newline-delimited JSON
  requests on stdin: `{id, method:"normalize"|"detect"|"slice"|"render_group", params}`.
  It dispatches to the *existing* tool functions (refactor each `*.py` to expose a callable
  `run(params)->dict`; keep the `argparse` `main()` for CLI parity + tests). Replies
  `{id, ok, result}` / `{id, ok:false, error}` on stdout (one JSON per line; route all logs
  to stderr so stdout stays clean-framed — important on Windows).
- **Lifecycle / crash handling.** Spawn on first request; serialize requests through one
  in-flight queue (tools are CPU-bound; the GIL makes a single worker fine — add a 2-3
  worker pool only if you want parallel slice batching). Ping/heartbeat; on process exit or
  a malformed/timeout reply, respawn and retry the request **once**; on repeated failure,
  **fall back to the current per-call `execFile` path** so correctness never depends on the
  worker being alive. Idle-timeout kill (e.g. 5 min) to release memory. Windows: stdio
  pipes + JSON framing is robust; `windowsHide:true`; ensure the worker `sys.stdout.flush()`
  after each reply.
- **Expected win.** Warm calls drop the ~165 ms import to ~0. Detect ~542 → ~250 ms; slice
  benefits from both the worker AND removing the redundant re-detect (below).
- **Also: kill slice's redundant re-detect.** `sliceRegions` re-runs normalize+detect
  (ops.mjs:529-530) purely to get `detected.normalizedPath`. `detectRegions` already
  produced that normalized image for the same content-addressed bytes. Cache the normalized
  PNG + key_color from detect (keyed by the element's file hash) and have slice reuse it →
  slice drops from **3 spawns to 1** (export only). Combined with the warm worker, slice
  ~828 ms → ~200 ms.
- **Rejected alternatives:** warm *pool of one-shot* interpreters (still pays import each
  call — no); batching regions into one spawn (slice already batches its regions in one
  export call; detect is a single image — batching doesn't apply). The persistent worker is
  the right call.

---

## 3. Journal / history growth (the O(n²) session cost)

**The design (`ops.mjs:13-27`, `commitMutation` 98-110, `snapshotOf` 82-93):** each mutation
line stores `undo_patch` (full `{title,elements,groups,tool_runs}` snapshot BEFORE) **and**
`state` (full snapshot AFTER). Files are immutable so a metadata snapshot fully restores —
correct, but each line is ~2× the whole project. Then:

- `appendJournal` (store.mjs:336-343) computes the next seq by `readJournal(...).reduce(max)`
  — it **reads and JSON-parses the entire journal on every append**.
- `readHistory` (ops.mjs:418-433) also reads+parses the whole journal every call, then
  throws away the giant `undo_patch`/`state` blobs it parsed, keeping only seq/at/op/
  args_summary. And it's called after **every** op (frontend `refreshHistory`).

**My experiment (each line = 2× a 100-element snapshot ≈ 28 KB):**

| Journal lines | append (median) | readHistory | file size |
|---|---|---|---|
| 0 | 1.0 ms | 0.7 ms | 0.14 MB |
| 50 | 6.2 ms | 6.6 ms | 1.56 MB |
| 100 | 11.4 ms | 11.5 ms | 2.98 MB |
| 250 | 26.6 ms | 28.3 ms | 7.2 MB |
| 500 | 52.7 ms | 54.3 ms | 14.3 MB |
| 1000 | **107.9 ms** | **109.0 ms** | **28.5 MB** |

Both curves are linear in journal size → **each op pays ~2×O(J)** (append + the
frontend's readHistory), and J itself grows ~2P per op → **O(n²) total over a session, and
the per-op cost climbs without bound.** At 500 accumulated ops, *every* subsequent delete
pays ~53 ms to append + ~54 ms readHistory = ~107 ms of pure journal I/O before any render.

> Note: the peer bench shows `readHistory` 1000 entries = 2.5 ms — but that fixture used
> *tiny* entries (no real snapshots). With realistic 2×-snapshot lines it is **109 ms**, a
> ~40× gap. The bench under-reports delete/undo cost for the same reason (fresh journals).

**Fixes (with correctness notes for the existing undo/redo pointer semantics):**

- **(M, biggest) Split snapshots into a sidecar; keep journal.jsonl lines tiny.** Write
  `undo_patch`/`state` to `snapshots/<seq>.json` (or `.before/.after`) and keep the journal
  line to `{seq, at, op, args_summary, parent, has_snapshot}`. Then `appendJournal`,
  `readHistory`, and undo/redo's `find` all scan only small lines (O(n) over KB, not MB);
  undo/redo load the single snapshot they need by seq. Pointer semantics unchanged:
  `history_seq` still the head, undo restores `snapshots/<head>.before` and moves head to
  `parent` (ops.mjs:376-395), redo picks greatest-seq child and loads `.after`
  (397-414). No behavioral change; just moves the bytes out of the hot line-scan.
- **(M) O(1) seq via tail-read or a counter.** seq is monotonic (append-only), so
  `appendJournal` can read just the **last non-empty line** to get max seq instead of the
  whole file (store.mjs:339), or persist `journal_seq` in project.json alongside
  `history_seq`. Correctness: seq must stay globally increasing even after undo (redo
  appends a *new* higher seq) — tail-read preserves this since the physical last line
  always has the max seq.
- **(S) Cached canUndo/canRedo counters.** `readHistory` recomputes canUndo/canRedo by
  scanning all entries (ops.mjs:423-424). Derive them from `history_seq` + the sidecar
  index (does `snapshots/<head>.before` exist? is there any child with `parent==head`?), or
  maintain a tiny `history_meta` (max child seq per parent). Lets the mutating op response
  carry the flags so the frontend never needs the separate `/history` GET (§5).
- **(S) tool_runs cap.** `detect/slice/export/render` each append to `project.tool_runs`
  unbounded (ops.mjs:483, 579, 654, 796), and it rides inside every snapshot. Cap to the
  last K (e.g. 50) or move provenance to a sidecar `tool_runs.jsonl`. Shrinks P → shrinks
  every snapshot, every project.json read/write, and the reload payload.
- **(L, optional) Delta undo_patch.** Store only changed element(s) per op instead of the
  full array. Smaller still, but group-move/assign touch many elements and correctness is
  fiddlier than the sidecar approach — do sidecar first.
- **Journal compaction on open.** When a project is opened, if the journal exceeds a
  threshold, rewrite it keeping only the reachable chain (entries on the path to
  `history_seq` + live redo children) and drop orphaned/undone branches + old markers.
  One-time O(J) at open, bounds steady-state J.

---

## 4. YandexDisk factor — measured, largely killed

`canvasProjectsRoot = C:/Users/ROG/YandexDisk/gamedev/ai_studio/canvas_projects`
(studio.config.json). Experiment: 40 cycles of `writeFileSync(tmp)+renameSync` over an
11 KB project.json, and 40 `appendFileSync` journal lines, in `.bench-tmp` under the cloud
root vs under `os.tmpdir` (`.bench-tmp` created then fully removed — verified gone):

| | atomic write+rename (median / p95) | journal append (median / p95) |
|---|---|---|
| YandexDisk (cloud) | 0.504 ms / 0.712 ms | 0.124 ms / 0.301 ms |
| os.tmpdir (local) | 0.456 ms / 0.682 ms | 0.103 ms / 0.185 ms |

**~10 % slower on write+rename, ~20 % on append — not a bottleneck.** The sync client
uploads asynchronously in the background; it does not block the synchronous fs call. The
atomic temp+rename pattern (store.mjs:71-75) is actually the *correct* choice for a synced
folder (rename is atomic; half-written files never sync).

Caveat (not a contradiction): cloud sync + antivirus can occasionally grab a just-written
file (hash/scan/upload) and cause a rare EBUSY/EPERM rename stall of hundreds of ms; my 40
cycles hit none, but the p95 tails are the place it would show. The mitigation is the same
as everything else — **reduce the write count** (slice does N writes, multi-delete does N
writes; batch them) so there are fewer files for the sync/AV to trip over.

---

## 5. Frontend perceived performance — ranked by jank contribution

1. **Full layers rebuild recreates every thumbnail `<img>` (highest).** `renderLayers`
   (layers_panel.js:136-154) does `list.replaceChildren()` then builds each row with a
   **fresh** `<img>` `thumb.src = fileUrl(element)` (layers_panel.js:45-48). Nodes are
   destroyed+recreated every op, so the browser re-decodes (and, since `serveFile` sends
   **no cache headers** — api.mjs:100-101, only content-type — often re-requests) *every*
   thumbnail. For a 100-element project that's 100 image loads per op. The canvas image
   cache (`imageFor`, app.js:151-158) is NOT used by the layers panel. **Fix (S):**
   (a) content-addressed files are immutable → send `Cache-Control: public, max-age=
   31536000, immutable` + ETag in `serveFile`; (b) reuse `<img>` nodes keyed by element.id
   (diff instead of replaceChildren) or route thumbs through the `imageFor` cache. Win:
   removes N fetch+decode per op.
2. **`reloadProject` full-GET + second `/history` GET on every op.** app.js:231-240,
   216-227. Every op ignores the returned project and re-fetches it, plus a second GET whose
   only purpose is canUndo/canRedo (an O(J) journal parse). **Fix (S-M):** every mutating op
   already returns `{project, …}` (e.g. removeElement ops.mjs:147, patchElement 135) — have
   the page apply that returned project to `state.project` and re-render, and fold
   canUndo/canRedo into the op response (§3). Removes **2 round trips + one O(J) parse per
   op**. This is the single biggest structural frontend win and stays within the op layer.
3. **Per-frame canvas backing-store realloc + forced reflow during drag.** `render()`
   (workspace.js:59-95) calls `resizeCanvas()` every time (61), which calls
   `getBoundingClientRect()` (forced layout) and assigns `canvas.width/height` (52-53) —
   assigning width/height **reallocates and clears the backing store even when the size is
   unchanged**. `onMouseMove` calls `render()` synchronously per mouse event (workspace.js:
   306/316/324) with no rAF. **Fix (S):** only resize when the CSS size actually changed;
   coalesce drag renders with `requestAnimationFrame`. Win: smooth drag.
4. **Full inspector rebuild every op** (`renderInspector`, inspector.js:202-222,
   `replaceChildren`) via `refresh()` (app.js:38-43). Cheaper than layers (no images) but
   still tears down inputs on every op. Low priority — fix after 1-3.
5. **Sequential per-element HTTP for multi-delete/move** (actions.js:100, workspace.js:
   338-347). N round trips + N journal entries. **Fix (S-M):** one batch op (§summary).

**Optimistic local apply + reconcile (L)** for delete/move: apply to `state.project`
immediately, render, fire the API in the background, reconcile on response (roll back +
status on error). Highest perceived win but needs care against undo/history — do items 1-3
first (they remove most of the pain at S effort).

---

## 6. External patterns worth stealing (applicable to thin-page + op-layer)

- **Excalidraw / tldraw:** in-memory scene is the source of truth for *rendering*; every
  edit mutates local state synchronously and paints immediately — persistence is async and
  debounced, never a round-trip in the interaction path. Applicable slice: stop discarding
  op responses / stop the reload-GET-after-every-op; the durable op layer stays the system
  of record, but the page renders from the op's returned project (item 5.2), reloading only
  on open. History is an in-memory stack of deltas, not re-read from disk each op — mirrors
  the "don't re-parse the whole journal every op" fix (§3).
- **Rendering:** both use dirty-rect / layered canvas (static content cached, only the
  active/dragged element repainted) and rAF-batched frames — directly maps to items 5.1/5.3
  (don't realloc the backing store; coalesce with rAF; a cheap "static layer vs drag layer"
  split is a later M).
- **Figma engineering:** immediate-mode local scene graph + a separate async persistence/
  ops channel; local apply first, server confirms. Same conclusion: keep the single op
  layer for durability, but let the client apply optimistically and reconcile.

Only these thin-page-compatible pieces apply; the multiplayer/CRDT machinery does not
(single-user local tool).

---

## Appendix — optimization designs ranked by win/effort

| # | Fix | Effort | Win | Anchor |
|---|-----|--------|-----|--------|
| A | Consume op's returned project instead of `reloadProject` GET; fold canUndo/canRedo into op responses → drop 2 round trips + O(J) parse/op | S-M | High (every op) | app.js:231-240, 216-227; ops responses e.g. ops.mjs:147,135 |
| B | Immutable `Cache-Control`+ETag on `/files`; reuse layer `<img>` nodes / use imageFor cache | S | High (every op render) | api.mjs:100-101; layers_panel.js:45-48,142 |
| C | Journal snapshots → sidecar `snapshots/<seq>.json`; tiny journal lines | M | High (kills O(n²) session cost) | ops.mjs:82-110; store.mjs:336-343; readHistory ops.mjs:418-433 |
| D | O(1) append seq (tail-read last line / persisted counter) | M | High | store.mjs:339 |
| E | Persistent Python worker (JSON-RPC/stdio) importing numpy+PIL once | M-L | High (detect ~2×, slice basis) | raster2d api.mjs:110-131; ops.mjs:692-713 |
| F | Skip slice's redundant re-detect (reuse cached normalized image) | M | High (slice 3→1 spawn) | ops.mjs:529-530 |
| G | Guard resizeCanvas + rAF-coalesce drag render | S | Med (drag) | workspace.js:43-55,306-324 |
| H | Batch multi-delete/move into ONE op (one journal entry, one write, one reload) | S-M | Med (multi-select) | actions.js:97-106; workspace.js:335-351 |
| I | tool_runs cap / sidecar | S | Med (shrinks P everywhere) | ops.mjs:483,579,654,796 |
| J | Journal compaction on open | M | Med (bounds J) | store.mjs:314-343 |
| K | Optimistic local apply + reconcile for delete/move | L | High perceived, higher risk | actions.js:97-106; app.js:231-240 |

### Fix-first order if bench confirms
1. **A + B** (S): remove the reload-GET churn and the per-op N-thumbnail reload — helps
   *every* op including delete, immediately, at low risk.
2. **G** (S): smooth drag.
3. **C + D + I** (M): kill the journal O(n²) — the "gets sluggish the longer I work" and
   the real delete/undo cost.
4. **E + F** (M-L): warm Python worker + drop slice's redundant re-detect — the slice/detect
   sluggishness.
5. **H**, then **K** if still wanted.

### Hypotheses confirmed / killed by my experiments
- **KILLED:** "YandexDisk cloud sync adds per-op fs latency." Measured 0.50 ms vs 0.46 ms
  (write+rename) and 0.124 ms vs 0.103 ms (append) — ~10-20 %, negligible. Not a driver.
- **CONFIRMED:** "Journal is O(n) per op and grows ~2P per line → O(n²) session cost."
  append 1 ms→108 ms and readHistory 0.7 ms→109 ms as journal 0→1000 ops (0.14→28.5 MB).
- **CONFIRMED:** "Python spawn/import dominates detect/slice." ~165 ms/spawn is numpy+PIL
  import alone; detect (2 spawns) ~60 % import, slice (3 spawns) ~50 % import.
