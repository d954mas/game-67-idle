# T0254 — Canvas API + Ops-Layer Architecture Review (dimension 1 of 3)

Scope: `ai_studio/assets/canvas/{ops,api,store}.mjs` + `tools/*.mjs` spawn sites.
Read-only. Evidence is `file:line` + measured numbers. Date 2026-07-04.

Measured baseline:
- `ops.mjs` — 4923 lines / 254 KB; **64 exported functions** (`export function`), ~50 more file-local helpers.
- `api.mjs` — 905 lines; ~60 route arms; imports ~60 names from ops.
- `store.mjs` — 689 lines (disk layer). `cli.mjs` — 921 lines, only 3 `ops.` references (genuinely thin).
- Largest real project on disk: `YandexDisk/.../canvas_projects/demo-02e8b7/project.json` = **137,344 bytes** (matches the 137 KB the lead cited). Projects live on a **cloud-synced YandexDisk folder** (`canvasProjectsRoot`, studio_config.mjs:51).

---

## Strengths (real assets, keep them)

1. **The parity law is genuinely enforced, not aspirational.** `cli.mjs` has only 3 `ops.` references and zero project-state disk writes — its `readFileSync/writeFileSync` are all input-flag / preview-output marshalling (cli.mjs:310,339,581). `api.mjs` holds no canvas logic; every route arm is `sendMutation(status, opFn(...))`. Both clients + tests go through the same 64 functions. This is the single best property of the module and the split proposal below is designed to preserve it exactly.

2. **The op template is strikingly uniform.** 44 ops build `const startedAt = performance.now()`, 44 `return { project, ... }`, 43 `commitMutation(...)` — the counts line up, meaning nearly every mutating op follows the identical shape: `getProject → mutate → commitMutation({op, args_summary, before, after, startedAt}) → return {project,...}`. `duration_ms`/`args_summary`/`actor` discipline is consistent across all of them. That regularity is what makes a 4900-line file navigable at all.

3. **The journal design is well-thought-out and correctly documented.** Thin append-only metadata lines + fat sidecar snapshots, single `history_seq` pointer, redo-by-greatest-seq-child auto-invalidating stale branches, depth-capped compaction that physically shrinks the journal so per-op scan stays O(cap) not O(session) (commitMutation ops.mjs:320-388). The 40-line header doc (ops.mjs:1-40) is accurate against the code. `writeAtomic` (tmp+rename, store.mjs:77) makes each single file write crash-safe.

4. **Additive-schema discipline is real.** `groups`/`recipe`/`style`/`slice9`/`rotation`/`actor` all landed as tolerated-absent additions (createProject comment store.mjs:150-160); older projects load unchanged. No destructive migrations except the one-time legacy-journal→sidecar migration, which keeps a `.bak`.

5. **Loud errors + content-addressed immutable `files/`.** No silent fallbacks in the Python/codex paths; `files/` are sha256-named, immutable, and served `cache-control: immutable` with the sha as ETag (api.mjs:181-188). Good.

6. **api.mjs's PUT/PATCH/POST split actually follows a coherent rule** (see Finding 6) — it just isn't written down.

---

## Findings (ranked by importance)

### F1 — No cross-client write lock; the single-writer assumption is now violated (robustness, HIGH)
**Evidence:** `store.mjs` and `api.mjs` contain zero lock/flock/mutex primitives (grep clean). `appendJournalLine` comments "atomic for this **single-writer** local tool" (store.mjs:493-495). `nextJournalSeq` = `lastJournalSeq + 1` — a pure read-max-then-increment (store.mjs:484-490). `updateProject` is read-modify-write with no CAS (store.mjs:189-194). The only concurrency control that exists is `site/long_op_queue.mjs`, which its own header calls "a **page** concern only … the CLI / direct ops path is unlimited" — an in-memory, per-browser python-spawn limiter, not a write lock.
**Why:** T0242 added a chat agent (`studio_shell/chat/agent.mjs`) and the CLI is a separate process. Page + chat (same API process) + CLI (separate process) can now all mutate the same `project.json` concurrently. Two ops that both `getProject` then `updateProject` → classic lost update; the losing op's mutation vanishes but its journal line + snapshot persist, so the journal now references a state that never landed. Worse, two concurrent commits compute the **same** `nextJournalSeq` → one snapshot overwrites the other and two journal lines share a seq → undo/redo corruption. `expectHead` (checkExpectHead ops.mjs:2986) guards **only** undo/redo/jump, and the doc says "the page omits it" (api.mjs:52) — so even that guard protects only the agent path.
**Recommendation (M):** Serialize mutations per project. Cheapest correct-enough fix for a local tool: (a) an in-process `Map<projectId, Promise>` async mutex in `createCanvasApi` that chains each mutating request — covers page+chat since both share the one API process; plus (b) a per-project advisory lockfile (`mkdirSync(lock, {recursive:false})` / `O_EXCL`) held across read-modify-commit so the separate CLI process can't interleave. Even just (a) removes the page-vs-chat race, which is the newly-introduced one.

### F2 — Per-op write amplification: project.json is read ~4× and written 2× per mutation, plus a full-project snapshot (performance, HIGH)
**Evidence:** trace one `moveNodes` drag-commit (ops.mjs:773-799):
- `getProject` → read #1 (full 137 KB parse).
- `updateProject({elements,groups})` → **read #2** (updateProject re-reads via readProjectFile, store.mjs:190) + **write #1** (137 KB via writeAtomic).
- `commitMutation` → `snapshotOf(before)` + `snapshotOf(after)` = two `JSON.parse(JSON.stringify(...))` deep clones of elements+groups+tool_runs (ops.mjs:276-287); `JSON.stringify(undoPatch)===JSON.stringify(state)` no-op check = 2 more full serializations; `writeSnapshot({undo_patch,state})` = a sidecar ~**2× project size** (~274 KB); then `updateProject({history_seq})` → **read #3 + write #2** (another full 137 KB read+write just to bump one integer); then `compactJournal` → `getProject` (read #4) + `readJournal`.
Net per single-element nudge: **~4 full parses + 2 full project writes + 1 ~274 KB snapshot write + ~6 whole-project JSON serializations**, all inside a **YandexDisk-synced** folder that then uploads ~0.5 MB per drag-commit.
**Why:** Cost is O(project.json size) per op regardless of how little changed. At 137 KB it's still sub-10ms locally, but it 10×'s with project size and the cloud-sync bandwidth cost is paid on every gesture. The redundant second `updateProject` (write #2, only to set `history_seq`) is pure waste — the op already wrote the state one line earlier.
**Recommendation (M):** Fold `history_seq` into the op's single write. Change the `commitMutation` contract so the op hands it the **not-yet-persisted** next-state object; commitMutation does ONE `writeProjectFile` with `{...after, history_seq}` and passes the in-memory `before`/`after` objects into `snapshotOf` (no re-read). Eliminates read #2, read #3 and write #2 → roughly halves the disk work per op. Mechanical but touches all 43 commit sites; low logical risk (the template is uniform).

### F3 — ops.mjs is a god-module at 4.9k lines / 64 exports (cohesion, HIGH — full split proposal below)
**Evidence:** one file now spans journal machinery, elements, nodes/transform, groups, recipe+style cards, region/slice/alpha pipeline, and export/render (see the export map, ops.mjs:315-4923). It grew this week from transforms + cards + generation + cleanup + slice-9. The seams are clean and already visible in the source ordering.
**Why:** It's still *one coherent layer* semantically (one op per capability), but navigability is gone: finding "the alpha ops" means scrolling past 3400 lines. Merge-conflict surface for the 2 fast-workers who touch it in parallel is the whole file.
**Recommendation (L-M):** Split into `ops/<domain>.mjs` behind a re-export **barrel** `ops.mjs`. See the dedicated section — this is low-risk *because* the barrel keeps every api/cli/test import unchanged.

### F4 — Duplicated element-lookup+guard boilerplate (~35 sites) and the mkdtemp/spec/report/out temp-dance (~6 sites) (DRY, MEDIUM)
**Evidence:** `if (!element) throw new Error(\`element not found: ${elementId}\`)` appears ~25× (ops.mjs:219,261,710,929,1039,1198,2531,2600,2676,3419,3711,3722,3756,3928,3989,4116,4119,…); the image-guard `element.type !== "image" || !element.src` follows it ~8× (3712,3757,3929,4117,4120,…). Separately, the temp-tool dance `mkdtempSync → writeFileSync(spec) → runToolPython(...) → JSON.parse(readFileSync(report)) → readFileSync(out) → finally rmSync` is copied 6× (ops.mjs slice 3484-3545, alpha 3655-3676, cleanup 3941-3950, dualplate 4038-4056, flatcheck 4210-4219, export 4573-4585) and again inside each tool spawn file.
**Why:** ~35+16 lines of identical guard/cleanup logic; a forgotten `rmSync` finally leaks a temp dir, a forgotten image-guard corrupts a text element.
**Recommendation (S):** `requireElement(project, id, {image=false})` → collapses ~35 sites to one call each; `withTempDir(prefix, async dir => …)` + `runSpecTool(root, script, {spec, report, out})` → collapses the 6 dances. Pure helpers, unit-testable, zero API change.

### F5 — Four+ codex/agy spawn sites with subtly divergent handling (DRY vs loud-explicit, MEDIUM)
**Evidence:** five live spawn variants, three families:
- **generate_image.py family (near-identical):** `dual_plate_generate.generatePlate` (dual_plate_generate.mjs:102-114) and `recipe_generate.generateImageCodex` (recipe_generate.mjs:76-92) are byte-for-byte the same wrapper — `mkdtemp → build argv → execFileAsync({timeout:500_000, maxBuffer:16MB}) → readFile(out) → finally rm`. Only the argv builder + mkdtemp prefix differ.
- **codex-exec family (shared CODEX_JS + --output-last-message + stdin footgun):** prompt_assist **text** (closes stdin immediately, prompt_assist.mjs:109-114, timeout 300_000), prompt_assist **vision** (writes instruction to stdin then end, `-i img -`, prompt_assist.mjs:198-205), and `studio_shell/chat/agent.mjs` (spawn, `--json`, `exec resume <id>`, timeout **900_000**). All three depend on the exact same sharp detail: execFile always opens a stdin pipe and `codex exec` stalls if it's left open (prompt_assist.mjs:110-113). Note the cross-module coupling: **chat/agent.mjs imports `CODEX_JS` from `canvas/tools/prompt_assist.mjs`** (agent.mjs:47) — a codex-transport constant living inside a canvas tool.
- **agy family (genuinely different):** `recipe_generate.generateImageGemini` uses `spawn` with stdio all-ignored, **never rejects**, and proves success by file existence + a `.seen.txt` ref-proof guard (recipe_generate.mjs:194-235).
Timeout values drift with no single source: 500s / 300s / 900s.
**Why / both sides:**
- *For a shared helper:* the generate_image.py pair is true copy-paste; the stdin-stall footgun is a correctness landmine currently re-implemented in 3 places and must also be right in any future spawn — a helper encodes it once. Timeout drift (500/300/900) is a symptom of copy-inherit rather than deliberate choice.
- *Against:* the sites differ in load-bearing ways (execFile vs spawn, stdin-empty vs stdin-instruction, resume/--json, never-reject file-proof). The module docs are explicitly proud of being "the ONE place" per tool; a single god-spawn accretes booleans and hides exactly the differences a reviewer needs to see.
**Recommendation (S-M):** Two *narrow* helpers, not one. (a) `runImageGenPy({argv, outPath})` shared by the two generate_image.py sites. (b) A neutral `shared/codex_exec.mjs` owning ONLY `CODEX_JS` resolution + the stdin-pipe discipline + the `--output-last-message` read; text/vision/chat still build their own argv and parse their own output, and `CODEX_JS` moves out of the canvas tool that chat currently reaches into. Leave `generateImageGemini`/agy exactly as-is. Make `timeoutMs` a required explicit arg so 300 vs 900 is a visible decision.

### F6 — Uniform 400 error mapping hides 404/409; the (good) PUT/POST rule is undocumented (api, MEDIUM)
**Evidence:** the single catch-all maps **every** thrown error to `sendJson(res, 400, …)` (api.mjs:902). So project-not-found → 400, and an `expectHead` conflict ("history advanced: head is now…", ops.mjs:2993) → 400. Meanwhile the route method-verbs *do* follow a consistent rule that's just never stated: **PUT** = idempotent replace of a sub-document (`elements/<id>/regions|slice9|export`, api.mjs:828-853), **PATCH** = partial blob update (`projects/<id>`, `groups/<id>`, `recipe-cards/<id>`, `style-cards/<id>`, `elements/<id>`), **POST** = action / mint / tool-run (`cleanup`, `generate`, `slice`, `alpha`, `nodes-*`). The lead's "slice9 PUT vs cleanup POST vs recipe PATCH" is actually principled (replace-subdoc vs run-tool vs partial-patch), not arbitrary.
**Why:** The page can't distinguish "someone else moved the head, reload" (should be 409) from "bad input" (400) — which defeats the purpose of the T0234 expectHead guard. And because the rule is undocumented, the API *reads* inconsistent to a reviewer even though it isn't.
**Recommendation (S):** Add `statusForError(err)`: `/not found/ → 404`, `/history advanced|expectHead/ → 409`, TypeError/unexpected → 500, else 400. ~8 lines. Add one comment block stating the PUT/PATCH/POST rule. Have the page send `expectHead` on undo/redo/jump so page-vs-agent races surface as a reload prompt.

### F7 — Non-atomic multi-file commit; no crash reconciler (robustness, MEDIUM-LOW)
**Evidence:** a commit writes three files in sequence with no transaction (commitMutation ops.mjs:326-336): `writeSnapshot(seq)` → `appendJournalLine` → `updateProject({history_seq})` → `compactJournal`. Each individual write is atomic (writeAtomic / O_APPEND) but the *set* is not. A crash between step 2 and 3 leaves the journal head ahead of `project.json.history_seq`; a crash between 1 and 2 orphans a snapshot.
**Why:** Reads are tolerant (torn tail line dropped, store.mjs:456-482; `snapshotForEntry` returns `{}` if missing, ops.mjs:297-302), so it degrades rather than crashes — but a committed-yet-unpointed mutation is silently lost. Blast radius is small for a local tool, but the lead asked.
**Recommendation (S):** The ordering is already the safe one (snapshot → journal → pointer). Add a boot-time reconciler in `ensureThinJournal`: drop/archive any journal mutation whose `seq > project.history_seq` with no forward redo path, and delete orphan snapshots with no journal line. Cheap insurance; pairs naturally with the F1 lock.

### F8 — Unbounded `files/` growth; snapshot store O(depth × project size) on cloud disk (performance/hygiene, LOW)
**Evidence:** `files/` are content-addressed and immutable (never rewritten) but also **never GC'd** — an undone `addImage`, a replaced alpha cutout, a regenerated recipe output all leave orphan bytes forever. Snapshots: depth 200 (canvasHistoryDepth default) × ~2×137 KB ≈ **~55 MB per project** in `snapshots/`, all on YandexDisk sync.
**Why:** Long-lived projects accrete dead files and a large synced snapshot tree. Not urgent; it's a slow leak, not a correctness bug.
**Recommendation (S, defer):** Optional `vacuum` op that walks live element `src` refs + retained journal snapshots and deletes unreferenced `files/`. Separately consider relocating volatile `snapshots/` + `journal.jsonl` to a **non-synced** local cache (keep `project.json` + `files/` synced) to cut ~90% of per-op cloud-sync churn — but that changes history portability, so flag as a decision not a default.

### F9 — Minor validation-prefix naming drift (consistency, LOW)
**Evidence:** patch-shaping is split between `sanitize*` (sanitizeTextPatch, sanitizeTransformPatch) and `normalize*` (normalizeRecipePatch, normalizeStylePatch, normalizeGroupBackground/Clip) — both mean "clean an incoming patch object"; plus one-offs `cleanExportRows`, `checkExpectHead`, `checkFlatBackground`, `refuseIfTransformed`. Six prefixes for input-shaping/validation.
**Why:** Cosmetic; costs nothing at runtime, mild grep friction.
**Recommendation (S, opportunistic):** Standardize patch-shapers on one prefix (`normalize*`) during the F3 split; leave `refuse*`/`require*` for the throwing guards. Do it only while files are already open.

### F10 — Route docs duplicated across api.mjs header and the 108 KB README (freshness, LOW)
**Evidence:** the full route list lives twice: the api.mjs header block (api.mjs:1-60, authoritative and currently accurate — it lists recipe-cards/style-cards/cleanup/extract/promote/slice9/alpha-dual-generate, all present in the dispatch) and the 108,875-byte `README.md`.
**Why:** Two sources of truth drift; the README is the one that rots.
**Recommendation (S):** Make README point at the api.mjs header for the canonical route list rather than restating it.

---

## Do-differently

### The ops.mjs split (dedicated) — barrel + domain files, parity-preserving

**Verdict: do it, as a re-export barrel.** The key insight that makes this low-risk: **api.mjs imports ~60 names from `./ops.mjs`, cli.mjs and every test import from `./ops.mjs`.** If `ops.mjs` becomes a thin barrel that `export *` from the domain files, *none* of those import sites change — the parity law and the entire client/test surface are untouched. The split is purely internal reorganization.

Proposed file map (line ranges are current ops.mjs):

| New file | Contents (ops + helpers) | ~lines |
|---|---|---|
| `ops/_core.mjs` | snapshotOf, isMutation, snapshotForEntry, setOpsActor, **commitMutation**, compactJournal, recordOpFailure, checkExpectHead, requireElement (new), withTempDir/runSpecTool (new); re-exports getProject/updateProject/store seams | ~400 |
| `ops/project.mjs` | createProject, patchProject, deleteProject | ~60 |
| `ops/elements.mjs` | addImage(s), addImageFromFile, addText, patchElement(s), removeElement(s) + sanitize/normalize patch helpers | ~450 |
| `ops/nodes.mjs` | reorderNode/Element/Nodes, moveNodes, alignNodes, distributeNodes, pasteNodes, duplicateNodes, deleteNodes + applyNodeMoves/findNode | ~550 |
| `ops/groups.mjs` | createGroup, patchGroup(s), fitGroup, assignToGroup, deleteGroup, reparentGroup, ungroupGroup + group helpers | ~550 |
| `ops/cards.mjs` | createRecipeCard/patchRecipe/createStyleCard/patchStyle + generateFromRecipe, expandRecipePrompt, extractFromElement, promoteExtracted* | ~900 |
| `ops/image_pipeline.mjs` | setRegions, setSlice9, detectRegions, sliceRegions, alphaCutout(+single/batch/provenance), cleanupPreview/Apply, alphaDualPlate(+Generate), checkFlatBackground | ~1200 |
| `ops/export.mjs` | parseScaleSpec, resolveExportScale, setExportSettings, exportElements, zipExport, renderGroup, exportProject | ~700 |
| `ops/history.mjs` | undoOp, redoOp, historyFlags, readHistory, opsStats, historyEntryLabel, listHistory, jumpHistory + spine/availability | ~500 |
| `ops.mjs` (barrel) | `export * from "./ops/*"` | ~30 |

**What breaks / risks:** cross-domain calls create potential import cycles — cards generation calls `addImage` (elements), nodes paste/dup may need group logic, everything needs `commitMutation`/`requireElement`. Route all shared machinery through `_core.mjs` (which imports nothing from the domains) and let domains import each other only downward (cards→elements is fine; keep elements→cards from happening). Tests that reach for file-local helpers (not exported) would break — but those are internal; the exported surface is stable. **Sequencing:** land `_core.mjs` + the F4 helpers first (own PR), then peel one domain at a time, running the test suite between each — the barrel means each peel is independently shippable.

### Alternative considered & rejected: leave it as one file
One file *is* still semantically one layer, and the uniform template keeps it readable-ish. Rejected because two fast-workers now edit it in parallel (whole-file merge surface) and the module doubled this week — the growth curve, not the current size, is the problem.

### Alternative considered: split by REST resource to mirror api.mjs routes
Rejected — the op seams (transform math, journal machinery) don't map 1:1 to HTTP resources, and forcing that mapping would scatter `commitMutation`/tree-math across files. Split by *domain cohesion*, keep the barrel matching the flat import surface.

---

## Top-10 fixes

| # | Fix | Dim | Size | Payoff |
|---|---|---|---|---|
| 1 | Per-project mutation serialization: in-process async mutex (page+chat) + advisory lockfile (CLI) | robustness | M | Removes lost-update + duplicate-seq corruption from the new multi-writer reality (F1) |
| 2 | Fold `history_seq` into the op's single write; pass in-memory before/after to commitMutation (no re-read) | perf | M | Halves per-op disk read/write; cuts cloud-sync churn (F2) |
| 3 | `requireElement()` + `withTempDir()`/`runSpecTool()` helpers | DRY | S | Collapses ~35 guard sites + 6 temp-dances; removes leak/guard-forget bugs (F4) |
| 4 | `statusForError()` → 404/409/500 + document PUT/PATCH/POST rule; page sends expectHead | api | S | Page can react to head-conflict vs bad-input; makes the (already-good) verb rule legible (F6) |
| 5 | Split ops.mjs into `ops/<domain>.mjs` behind a re-export barrel | cohesion | L | Restores navigability + shrinks merge surface with ZERO client/test import change (F3) |
| 6 | Two narrow spawn helpers: `runImageGenPy` (dedup dual_plate+recipe) + neutral `codex_exec.mjs` (own CODEX_JS + stdin discipline); leave agy | DRY | S-M | Kills true copy-paste + the stdin-stall footgun's 3 copies; moves CODEX_JS out of the canvas tool chat reaches into (F5) |
| 7 | Boot-time journal/snapshot reconciler in ensureThinJournal | robustness | S | Recovers torn commits; pairs with #1 (F7) |
| 8 | `vacuum` op for orphan `files/`; evaluate moving snapshots/journal off the synced disk | hygiene/perf | S | Stops slow file leak + ~90% of per-op cloud-sync bytes (F8) |
| 9 | Standardize patch-shaper naming on `normalize*`; keep `refuse*`/`require*` for throwing guards | consistency | S | Removes 6-prefix drift (F9) — do during #5 |
| 10 | README points to api.mjs header for the canonical route list | docs | S | One source of route truth (F10) |
