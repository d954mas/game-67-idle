# T0232 — Canvas object transforms for screen building (align / scale / rotate)

Design phase (deep-reasoner, 2026-07-03). READ-ONLY: no source touched. Designed against
committed canvas semantics (`ai_studio/assets/canvas/`); another agent is concurrently
adding a history guard to ops/cli/api — nothing below conflicts (all additive ops + UI).

---

## 1. Рекомендация v1 (для лида — 5 строк)

1. **Выравнивание (сразу, самое быстрое):** панель из 8 кнопок при выборе 2+ объектов —
   лево/центр/право, верх/центр/низ + распределить по горизонтали/вертикали. Один объект
   внутри экрана-группы выравнивается **по рамке экрана** (центрировать кнопку в экране).
2. **Масштаб (увеличение):** тянешь угловые/боковые ручки прямоугольника выделения прямо на
   холсте. Спрайты по умолчанию **сохраняют пропорции** (Shift — свободно). Работает и для
   нескольких объектов как единый блок.
3. **Поворот v1 = шаги 90° + отражение (flip H/V)**, кнопками. Пиксельно точно, без потерь,
   экспорт совпадает с холстом 1-в-1. **Свободный угол (произвольный наклон) — отдельный
   большой этап позже**, он дорогой и рискует рассинхроном холст/экспорт.
4. Всё это — одна операция = один Undo; доступно и на странице, и агенту через CLI.
5. **Нужно твоё «да» по 4 пунктам в разделе 7** (в первую очередь: нужен ли произвольный
   угол поворота или хватит шагов 90°).

---

## 2. Data model + exact op signatures

**Zero schema migration.** Align/scale/90°-rotate need **no new persisted field** — they only
write existing `x/y/w/h` (and `source_w/source_h/src` for the pixel op). Free-angle rotation
(deferred) is the only feature that would add a field (`element.rotation`, additive, absent = 0).

Element record today: `{ id, type:"image"|"text", x, y, w, h, src, source_w, source_h,
groupId?, order?, visible?, regions?, meta?, style? }`. Group: `{ id, name, x, y, w, h,
visible, parentId?, order?, background?, clip? }`. Both carry a frame box `x/y/w/h`, so every
op below treats "node" = element or group uniformly.

### 2a. Align / Distribute (increment 1)

Pure geometry helpers in `tree.mjs` (shared by site preview + op — same parity contract as
`blockReorder`/`buildNodesSpec`); the op applies the moves through the **same overlap-safe
subtree cascade `moveNodes` already uses** (extract its core into a private
`applyNodeMoves(before, moves)` reused by both `moveNodes` and `alignNodes`).

```
// tree.mjs (pure, exported)
alignMoves(project, nodeIds, align, reference)   -> [{nodeId, x, y}]   (only nodes that move)
distributeMoves(project, nodeIds, axis)          -> [{nodeId, x, y}]

// ops.mjs
alignNodes({ projectId, nodeIds, align, reference })
  align     : "left" | "hcenter" | "right" | "top" | "vcenter" | "bottom"
  reference : "auto" (default) | "selection" | "parent"
distributeNodes({ projectId, nodeIds, axis })
  axis      : "h" | "v"    // equalize gaps between adjacent boxes; needs >=3 nodes
```

**Reference box (recommended default `"auto"`, Figma semantics):**
- `nodeIds.length >= 2` → align to the **union bounding box** of the selected nodes.
- `nodeIds.length === 1` **and** the node has a parent group → align to the **parent group
  frame** (this is the "center this widget inside the screen" case — directly serves screen
  assembly).
- 1 node at root with no parent → loud error ("select 2+ objects, or one object inside a
  screen").

Per-node target (each node uses its own frame box `w/h`): `left`→`x=ref.minX`;
`hcenter`→`x=ref.cx - w/2`; `right`→`x=ref.maxX - w`; top/vcenter/bottom analogous on `y`.
A moved **group** carries its delta across its whole subtree (via the shared cascade), exactly
like a group drag. Distribute: sort by box min along axis, `free = span - Σsize`,
`gap = free/(n-1)`, place each box after the previous with that gap (endpoints fixed).

Loud + atomic: unknown id, `<2` nodes for align / `<3` for distribute, unknown `align`/`axis`
→ throw before any write. Empty result (already aligned) → no-op (no journal entry). ONE
`commitMutation` → one undo restores every position.

### 2b. Scale (increment 2) — **no new op, reuse existing**

Scale is already fully modeled: `element.w/h` drives both the canvas `drawImage` and PIL
`image.resize((w,h))` (`render_group.py` line 65-68). So interactive scale = a **page-only
drag affordance that commits existing ops**:
- single element → `patchElement(elementId, {x,y,w,h})`
- multi-element block → `patchElements({patches:[{elementId,x,y,w,h}...]})` (one entry)
- **text** → `patchElement(elementId, {style:{fontSize}})` (proportional; PIL re-measures, stays
  crisp — do NOT stretch the text box).
- **group frame** → `patchGroup(groupId, {w,h})` (frame-only resize; children unchanged).

Parity is intact because both clients bottom out on `patchElement/patchElements` (the agent
scales by computing `w/h` and calling the same ops — same pattern as marquee-move → `moveNodes`).
No `scaleNodes` op is needed for v1. (A `scaleNodes({nodeIds, sx, sy, originX, originY})`
convenience that also proportionally scales a **group subtree** is the natural follow-up — see
§7 Q2 and increment note.)

### 2c. Pixel rotate / flip (increment 3) — 90° steps + mirror, `alphaCutout` twin

New op mirrors `alphaCutout` exactly (content-addressed src swap, byte-exact undo, batch):

```
// ops.mjs
transformImage({ projectId, elementId | elementIds, op })
  op : "rotateCW" | "rotateCCW" | "rotate180" | "flipH" | "flipV"
```

- New Python tool `tools/rotate_image.py` (PIL `Image.transpose(ROTATE_90/180/270 /
  FLIP_LEFT_RIGHT / FLIP_TOP_BOTTOM)` — **lossless**, no resampling). Spawned once through the
  shared warm worker, spec-file pattern, same as `alpha_cutout.py`/`crop_regions.py`.
- Result written as a **new immutable content-addressed file**; element `src` swapped in ONE
  journal entry; previous file stays in `files/` → undo restores exact bytes.
- `rotateCW`/`rotateCCW` **swap** `source_w↔source_h` **and** display `w↔h`, keeping the box
  **center fixed** (so a wide sprite becomes tall in place). `rotate180`/`flipH`/`flipV` keep dims.
- **Regions:** transform each stored region rect/polygon in source space by the same exact
  integer 90°/flip remap (deterministic, ~15 lines) so a sliced/keyed sprite survives a turn.
  (Alternative to shrink the packet: refuse `transformImage` when `element.regions` is non-empty
  — loud "flatten regions first". Recommend the remap; flag in §7 Q3.)
- Image-only (loud error on a text element, like alpha). `elementIds` batches a multi-selection
  into ONE entry (atomic — all-or-nothing), same contract as `alphaCutout` batch.

**Free-angle rotation is explicitly OUT of v1** (see §4 cost + §7 Q1).

---

## 3. UI surfaces

- **Align/distribute (inc 1):** an **Align** row at the top of the multi-select inspector
  (`inspector.js` `renderMulti` / a new `renderMultiGroup`-peer for mixed) — 6 align icon
  buttons + 2 distribute, Figma layout. Also shown for a **single node that has a parent
  group** (align-to-frame). Buttons call one action in `actions.js` → one API call. (Context
  menu entry optional; inspector only for v1 to stay lean.)
- **Scale (inc 2):** 8 resize handles drawn on the **selection AABB** in the `workspace.js`
  chrome pass (same place selection strokes draw, `paintElement`/a new `drawSelectionHandles`).
  Handle hit-test in screen space (constant ~8px, DPR-aware); drag math mirrors the existing
  `dragRegionResize` (anchor = opposite handle; corner = proportional for images by default,
  Shift = free; edge = 1-axis; Alt = scale-from-center). Live preview mutates in-memory
  `w/h`(+`x/y`); `onMouseUp` commits one `patchElement`/`patchElements` (mirrors
  `commitElementDrag`).
- **Rotate/flip (inc 3):** a **Transform** section (rotate ⟳/⟲, flip ⇋/⇅ buttons) in the
  element inspector + the element **context menu**; multi-select shows a batched "Rotate/Flip N
  images" like the existing multi Alpha section. Long-op via the queue + progress toast (Python
  spawn), exactly like Alpha/Slice.

---

## 4. Parity notes (PIL / CLI / cost)

- **Align/scale need ZERO PIL change.** `render_group.py` already paints at the element's
  `x/y/w/h`; align only moves `x/y`, scale only changes `w/h`. Canvas↔export stay identical by
  construction.
- **Pixel rotate/flip: PIL parity is trivial and exact.** The pixels are rotated once at op
  time and stored; the renderer still just pastes them at `w/h`. No render/hit-test/marquee
  change at all. `Image.transpose` for 90° multiples is lossless (a pixel permutation), so
  there is **no resample drift** — this is why 90° is the safe v1.
- **CLI (tool parity):**
  - `nodes-align <id> --nodes n1,n2 --align left|hcenter|right|top|vcenter|bottom [--reference auto|selection|parent]`
  - `nodes-distribute <id> --nodes n1,n2,n3 --axis h|v`
  - `transform <id> --element <eid> --op rotateCW|rotateCCW|rotate180|flipH|flipV` and
    `transform <id> --elements e1,e2 --op ...` (batch) — parallels `alpha`.
  - Scale: **no new verb** — agents already scale via `elements-set`/`element-set` (`w/h`).
  - API routes parallel the batched ones: `POST .../nodes-align`, `POST .../nodes-distribute`,
    `POST .../transform-image` (see `api.mjs` `nodes-move`/`nodes-reorder`/`alpha` handlers).
- **Free-angle rotation cost (why deferred):** it touches schema (`rotation`) **and** canvas
  `ctx.rotate` render **and** `Image.rotate(expand, resample)` export (**resample drift → real
  canvas/PIL mismatch risk on already-alpha sprite edges**) **and** rotated-rect point-in-poly
  hit-test **and** rotated selection chrome + resize handles **and** marquee visible-box math
  **and** a regions/slice guard. That is 2-3 packets and the one genuine parity hazard in the
  whole task. Recommend a separate epic once the lead confirms he needs it.

---

## 5. Hit-test / undo / journal notes

- **Hit-test / marquee / selection chrome are UNCHANGED** for all three v1 features: every node
  stays an axis-aligned box (`hitElement`'s `inBox`, `visibleBox`, `strokeRect`). 90°-rotate
  only swaps `w/h` — still an AABB. This is the core reason v1 is cheap. Only free-angle
  rotation would break the AABB assumption (hence deferred).
- **Scale handles** add screen-space hit-testing *in front of* the existing element hit-test
  (grab a handle before falling through to move), same precedence `dragRegionResize` uses over
  `dragRegionMove`.
- **Undo / journal:** every op is ONE `commitMutation`. Align/distribute/scale ride the
  metadata snapshot (positions/sizes) — instant undo, no files. `transformImage` swaps `src` to
  an immutable new file and keeps the old one → undo is byte-exact (identical to `alphaCutout`).
  Batched multi-selection = one entry = one undo restoring all N. No-op gestures (already
  aligned / zero-delta drag) write no entry (existing `commitMutation` no-op guard).
- **History-jump safety:** all additive; the concurrently-landing history guard is orthogonal.

---

## 6. Increment packets (each = one Sonnet fast-worker packet)

### Increment 1 — Align & Distribute  *(ship first: fastest win, no pixels, no Python)*
- **Files:** `tree.mjs` (add `alignMoves`, `distributeMoves`, `unionBBox`); `ops.mjs` (extract
  `applyNodeMoves` from `moveNodes`; add `alignNodes`, `distributeNodes`); `api.mjs`
  (`nodes-align`, `nodes-distribute` POST routes, folded-history response); `cli.mjs`
  (`nodes-align`, `nodes-distribute` verbs + usage); `site/actions.js` (two action fns);
  `site/inspector.js` (Align section on multi-select + single-with-parent); README ops list +
  CLI table.
- **Op signatures:** §2a exactly.
- **UI:** inspector Align row (6+2 icon buttons); one API call per click.
- **Tests (`tests/align.test.mjs` new):** each align target math; distribute equal-gap; group
  in selection cascades its subtree; single-node-in-parent aligns to frame; `<2`/`<3` loud
  errors; already-aligned no-op writes no entry; one-entry undo restores all; cross-scope
  selection; CLI + API round-trip.
- **Done when:** align/distribute correct for elements, groups, and mixed; one journal entry
  per gesture; page buttons + CLI both drive `alignNodes`/`distributeNodes`; tests green;
  README updated.

### Increment 2 — Interactive scale handles  *(page-only; reuse patchElement/patchElements)*
- **Files:** `site/workspace.js` (draw 8 handles on selection AABB; handle hit-test; `scale`
  drag mode + math; `onMouseUp` commit); `site/inspector.js` (optional live W/H reflect);
  README Page/Shortcuts.
- **Ops:** none new — commit `patchElement` (single) / `patchElements` (multi block) /
  `patchGroup` (group frame) / `patchElement{style.fontSize}` (text).
- **Behavior:** corner = proportional-by-default for images (Shift = free distort), edge =
  1-axis, Alt = from center; anchor = opposite handle; group-in-selection = frame-only resize
  (no subtree scale in v1).
- **Tests (`tests/scale_handles.test.mjs` new, DOM-free where possible):** pure handle/anchor
  math module unit-tested (extract the resize math like `blockReorder`); proportional lock;
  multi-block scale produces one `patchElements`; text fontSize mapping. (Handle rendering is
  visually verified on the page.)
- **Done when:** dragging a handle resizes the sprite live and commits one entry; Shift/Alt
  modifiers work; multi-selection scales as a block in one undo; text scales font size crisply.

### Increment 3 — Pixel rotate / flip (90° steps + mirror)  *(alphaCutout twin)*
- **Files:** `tools/rotate_image.py` (new, PIL transpose + spec/report); `ops.mjs`
  (`transformImage` single + batch, region remap, `source_w/h`+`w/h` swap, provenance
  `meta.transform` + `tool_runs`); `api.mjs` (`transform-image` route); `cli.mjs` (`transform`
  verb, single + `--elements` batch); `site/actions.js`; `site/inspector.js` (Transform
  section) + `site/context_menu.js`; README (op + tool + CLI + Page).
- **Op signature:** §2c exactly.
- **Tests (`tests/transform.test.mjs` new):** dims swap on rotateCW/CCW, unchanged on
  180/flip; region rect/polygon remap correctness; undo restores exact prior bytes; batch
  atomicity (one failure mutates nothing, one entry on success); text element loud error;
  render parity (rendered PNG unchanged vs pre-rotated pixels).
- **Done when:** rotate/flip swap pixels losslessly, geometry/regions stay consistent, one
  journal entry (batch = one undo), page buttons + CLI both drive it, tests green.

---

## 7. Decisions needing lead confirmation (question — my default)

1. **Rotation angle:** Нужен ли произвольный угол наклона (любые градусы), или для сборки
   экранов хватит шагов 90° + отражения? — **Default: 90° + flip в v1**, произвольный угол =
   отдельный этап позже (дорогой, риск рассинхрона холст/экспорт).
2. **Scale of a GROUP:** Когда тянешь ручку у выбранной группы-виджета — масштабировать всё
   содержимое пропорционально, или только менять рамку? — **Default v1: только рамка**
   (frame-only); пропорциональный масштаб поддерева = быстрый follow-op `scaleNodes`.
3. **Rotate an element that has regions/slices:** Пересчитывать регионы при повороте на 90°,
   или запрещать поворот пока есть регионы? — **Default: пересчитывать точно** (integer
   remap); запрет — запасной вариант если хотим меньше кода.
4. **Align reference frame:** Выравнивать по рамке выделения (2+ объектов) и по рамке
   экрана-группы (1 объект внутри)? — **Default: да, авто (Figma-семантика)**; альтернатива —
   всегда по выделению.

(Minor, safe defaults — implement unless the lead objects: sprite corner-scale = proportional
by default with Shift to free-distort; distribute = equal-gap spacing needing 3+ nodes.)

---

## REVISION 2026-07-03 — free rotation required (lead override)

Lead answered §7. Overrides Q1: **"отражать нужно, поворот нужен любой"** — flip required,
**free-angle rotation required; 90°-only v1 is REJECTED.** Also: **"Есть ли режим в котором я
таскаю, могу скейлить, и вращать?"** — he wants ONE interactive transform mode (drag body +
corner-scale + rotate handle, Figma select gizmo). And **"нужна кнопка чтобы вернуть размер к
source"** — an explicit reset-to-source-size affordance. Q2 stays (group handle = frame-only),
Q4 stays (align auto). Q3 re-evaluated below (block source-space ops while transformed — loud).

This SUPERSEDES §2b's "no interactive gizmo" framing and §2c's 90°-pixel-op. The
`transformImage`/`rotate_image.py` pixel op is **dropped** — free rotation covers 90° too, and
flip is now a non-destructive flag (justified below), so **no new Python tool is needed**;
rotation+flip live in the existing `render_group.py` paint path.

### R1. Data model (additive, absent = current behavior — zero migration)

`element.rotation` (degrees, finite; absent/`0` = unrotated), `element.flipH`/`element.flipV`
(booleans; stored **absent** when false, like `group.clip`). All three are new **whitelisted
fields on `patchElement`/`patchElements`** (store must accept them) — **no new op**: the rotate
handle commits `patchElement({rotation})`, flip buttons commit `patchElement({flipH})`, reset
commits `patchElement({w:source_w, h:source_h})`. Tool parity holds because the agent sets the
same fields via `element-set`/`elements-set`. Validation: `rotation` non-finite → loud error;
normalize to `[0,360)`. Text elements: `rotation` allowed (rotates the box); `flip`/reset-size
are image-only (loud/hidden for text).

**Rotation convention (load-bearing — both renderers MUST agree):** the canvas is Y-down image
space (not game world; AGENTS Y-up rule is for game/world logic, N/A here). Define
`rotation` = **degrees clockwise on screen**, about the element **box center**
`(x+w/2, y+h/2)`. This one sentence is the parity contract; document it in code + README.

### R2. Render parity — exact canvas ↔ PIL compositing (THE risk I flagged)

Same composition on both sides: **resize → flip → rotate → paste centered.** Flip is innermost
(applied to the image first), rotation outer, so world transform = `Rotate(θ) ∘ Flip`.

**Canvas** (`workspace.js` `paintElement`), `s = vp.scale`:
```js
const w = element.w*s, h = element.h*s;
const c = imageToScreenPoint({x: element.x+element.w/2, y: element.y+element.h/2}, vp);
ctx.save(); ctx.translate(c.x, c.y);
if (rotation) ctx.rotate(rotation*Math.PI/180);      // CW+ (Y-down canvas)
ctx.scale(flipH?-1:1, flipV?-1:1);                   // mirror in the rotated local frame
ctx.drawImage(img, -w/2, -h/2, w, h);
ctx.restore();
```
**PIL** (`render_group.py` `paint_element`), device scale `s`, origin `(ox,oy)`:
```python
img = Image.open(src).convert("RGBA").resize((box_w, box_h), LANCZOS)  # box=round(w*s),round(h*s)
if flipH: img = img.transpose(Image.FLIP_LEFT_RIGHT)
if flipV: img = img.transpose(Image.FLIP_TOP_BOTTOM)
if rotation: img = img.rotate(-rotation, resample=Image.BICUBIC, expand=True)  # PIL + = CCW → NEGATE
cx = round((element.x+element.w/2 - ox)*s); cy = round((element.y+element.h/2 - oy)*s)
layer.paste(img, (cx - img.width//2, cy - img.height//2), img); out.alpha_composite(layer)
```
Three parity traps, spelled out so the packet closes them:
1. **Sign:** `ctx.rotate(+θ)` is CW; `PIL.rotate(+θ)` is CCW → PIL uses **`-rotation`**.
2. **Center:** both rotate about the box center; PIL `expand=True` yields a centered AABB whose
   center is pasted onto the box center in device space → positions coincide exactly.
3. **AA drift:** canvas internal resample vs PIL `BICUBIC` differ ~1px on the rotated edge only.
   This is the **declared acceptable approximation** — identical stance to text ("PIL is the
   single source of rendered truth; the canvas is a faithful same-transform approximation").
   Geometry (center, angle, size) is exact by construction; only edge AA differs.
**Clip is automatic:** rotation/flip happen before the paste, so a `clip:true` group crops the
already-composited rotated pixels (canvas clip-region push; PIL box-sized sub-layer) with no
extra work. `buildRenderNodes` (ops.mjs) just forwards `rotation`/`flipH`/`flipV` on element
paint nodes. The clip **ghost hint** for a rotated overflowing element is best-effort/deferred.

### R3. Hit-test, bbox, marquee (rotation-aware; flip is geometry-neutral)

One shared pure helper set (`tree.mjs`, node + site — parity): `rotatedCorners({x,y,w,h,rotation})`
→ 4 world points; `nodeAABB(node)` → AABB of those corners (flip irrelevant to extent). Consumers:
- **Hit-test** (page, `pointInElement`): inverse-rotate the world point into local frame and AABB-
  test: `lx = dx·cosθ + dy·sinθ; ly = -dx·sinθ + dy·cosθ; |lx|<=w/2 && |ly|<=h/2` (`dx,dy` = world −
  center). Flip ignored (same box).
- **Marquee / `contentBounds` / `elementsBBox`** (createGroup pad, `fitGroup`): use `nodeAABB` so
  groups/fit wrap rotated children. `elementsBBox` in ops.mjs switches to per-node `nodeAABB`.
- **Selection outline** (single rotated node): the rotated quad (`rotatedCorners`), not an AABB.

### R4. The one transform gizmo (Select tool, single node selected)

Move body (existing) + 8 scale handles at `rotatedCorners`/edge-mids + **1 rotate handle**
(top-center knob on a short stem, unambiguous hit-test; Figma's outside-corner rotate zones are a
later refinement). Rotate drag: `rotation = atan2(pointer − center)`; **Shift snaps to 15°**;
**double-click the knob (or an inspector "Reset rotation" / 0° button) → rotation 0**. Scale drag
becomes **rotation-aware**: inverse-rotate the pointer delta into the element's local frame, adjust
`w/h`, recompute `x/y` so the rotated **opposite corner** stays put in world space (corner =
proportional for images by default, Shift = free; edge = 1-axis; Alt = from center). Commit one
`patchElement` on mouseup. **Multi-selection = move + AABB block-scale only; per-element rotate is
single-selection in v1** (rotate-as-a-block deferred — flag). Group in selection = frame-only
resize (Q2).

### R5. Reset-to-source-size

A **"Reset to source size"** button in the inspector **Position & Size** section (`renderElement`,
near the W/H grid), enabled when `w!==source_w || h!==source_h`; image-only. Commits
`patchElement({w:source_w, h:source_h})` (keeps `x/y` = top-left fixed, consistent with W/H edits).
Rotation reset is separate (the knob double-click / 0° button). No new op; CLI: `element-set
--w <source_w> --h <source_h>` already covers agents.

### R6. Flip = schema flags, NOT a baked pixel op (justification)

Flags (`flipH`/`flipV`) chosen over a `transpose`→new-file bake because: (a) they **compose
correctly with free rotation** — `ctx.scale(-1,1)` inside the rotate transform makes a horizontal
flip visibly negate the apparent angle, which is true Figma "flip object" semantics; a baked mirror
+ separate `rotation` field would show the wrong result; (b) **non-destructive, no file churn**;
(c) they **don't change the box**, so hit-test/marquee/handles are untouched (geometry-neutral).
Cost is only render composition (R2) + the region guard (R7).

### R7. Regions / slice / alpha / detect under transform (Q3 — lead-approved refusal)

Source-space ops read UNtransformed source pixels, so a rotated/flipped element's display no longer
matches its source crops. v1 **refuses `detectRegions` / `sliceRegions` / `alphaCutout` / entering
region-edit** when `rotation!==0 || flipH || flipV` — **loud** error ("reset rotation/flip to edit
regions or slice — the source is untransformed"). Guard in the four ops (loud) AND the page
(disable region-edit entry + gray the inspector controls with the reason). A future "bake transform
into pixels" op would re-enable them; out of v1 scope.

### R8. Revised increment packets (align still first — I agree with the presumption)

1. **Align & distribute** — UNCHANGED from §6 Increment 1. Ship first: independent of the gizmo,
   pure geometry, highest immediate screen-assembly value.
2. **Transform gizmo skeleton: move + scale handles + reset-to-source** — the one mode WITHOUT
   rotation. Files: `workspace.js` (8 handles on the AABB, handle hit-test, `scale` drag mode +
   pure anchor math module, mouseup commit), `inspector.js` (Reset-to-source button). Ops: reuse
   `patchElement`/`patchElements`/`patchGroup`. Tests: pure handle/anchor math, proportional lock,
   multi-block one-entry, reset button. Done: drag-scale live + one undo; reset works.
3. **3a — Rotation + flip DATA + RENDER + PARITY (headless-verifiable, no gizmo).** Files:
   `store.mjs`+`ops.mjs` (whitelist `rotation`/`flipH`/`flipV` on patchElement/patchElements +
   validation), `tree.mjs` (`rotatedCorners`, `nodeAABB`; `elementsBBox`→nodeAABB), `ops.mjs`
   `buildRenderNodes` (forward fields), `render_group.py` `paint_element` (R2 exact math),
   `workspace.js` `paintElement` (R2 canvas), region guard in the 4 source-space ops (R7),
   `inspector.js` (numeric **Rotation** input + **Flip H/V** buttons + region-controls-disabled
   reason), `context_menu.js` (Flip), `cli.mjs`/`api.mjs` (`element-set --rotation/--flipH/--flipV`
   already flow through patchElement; add flags + usage). Tests (`tests/transform.test.mjs`):
   rotation persists/normalizes; **render parity** — a rotated+flipped fixture rendered by
   `render_group.py` matches expected geometry (center/angle/size), AA-tolerant pixel check;
   flip composition; region ops refuse loudly when transformed; batch. **This lands the parity
   risk with a headless test BEFORE any interactive code depends on it.**
4. **3b — Rotation in the gizmo (interactive).** Files: `workspace.js` (rotate handle at
   top-center, `rotate` drag mode, Shift 15° snap, double-click reset; upgrade scale handles to
   rotation-aware local-frame math from R3/R4; `pointInElement` local-point hit-test; marquee via
   `nodeAABB`; rotated selection quad), `inspector.js` (Reset-rotation/0° button). Ops: reuse
   `patchElement`. Tests: pure local-point hit-test + rotation-aware anchor math; snap. Done: the
   single gizmo drags/scales/rotates a sprite, Shift-snaps, resets; export matches (via 3a).

Order rationale: risk-sequenced — align (none) → gizmo skeleton (no parity risk) → rotation
data+render (the parity hazard, proven headless) → interactive rotation (builds on proven render).
3a before 3b is the important cut: the fiddly gizmo never blocks on unverified parity.

### R9. Load-bearing parity decisions (revised)

- Rotation = **CW-positive degrees about box center**, canvas `ctx.rotate(+θ)` ↔ PIL
  `rotate(-θ, expand=True)`; center-pasted; **PIL is source of truth, ~1px edge-AA drift accepted**.
- Composition **flip-then-rotate** on both sides (flip innermost).
- **Flip = additive flags** (compose with rotation, geometry-neutral), NOT a pixel bake.
- **No new op, no new Python tool**: rotation/flip are `patchElement` fields; render_group.py gains
  the transform. Reset-size = `patchElement({w,h=source})`.
- One shared rotation-aware `nodeAABB`/`rotatedCorners` (tree.mjs) feeds bbox/marquee/handles;
  hit-test uses the inverse-rotate local-point (page). Additive schema, one-gesture-one-entry
  (each gizmo commit = one `patchElement`), loud errors, strict tool parity — all preserved.

### R10. Remaining confirmations (small, safe defaults — proceed unless lead objects)

- Multi-selection **rotate-as-a-block** deferred (v1 rotate = single node). Default: defer.
- Rotate handle = **top-center knob** (v1) vs Figma outside-corner zones. Default: knob.
- Region/slice/alpha **refused** while rotated/flipped (R7), not auto-baked. Default: refuse+loud.

---

## Key file references
- Op layer + patterns: `C:\projects\game-67-idle\ai_studio\assets\canvas\ops.mjs`
  (`moveNodes` 645-697, `patchElements` 507-530, `alphaCutout`/batch 2415-2563,
  `commitMutation` 256-276, `elementsBBox` 1098-1110).
- Pure shared math: `C:\projects\game-67-idle\ai_studio\assets\canvas\tree.mjs`
  (`orderedChildren`, `descendantsOf`, `ancestorsOf`, `blockReorder` precedent).
- PIL parity: `C:\projects\game-67-idle\ai_studio\assets\canvas\tools\render_group.py`
  (`paint_element` 55-73 already resizes to `w/h`).
- Interaction/hit-test: `C:\projects\game-67-idle\ai_studio\assets\canvas\site\workspace.js`
  (`hitElement` 688-713, `dragRegionResize` 1041-1068, `commitElementDrag` 1155-1183).
- UI sections: `C:\projects\game-67-idle\ai_studio\assets\canvas\site\inspector.js`
  (`renderMulti` 882, `boxGrid` 124-134).
- Wiring precedent: `api.mjs` (`nodes-move`/`alpha` handlers), `cli.mjs` (`nodes-move`/`alpha`).
