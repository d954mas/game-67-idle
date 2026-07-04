# T0244 — Figma-style Smart Guides + Snap while dragging (canvas module)

Design doc only. No production files changed. Scope: `ai_studio/assets/canvas/`.

Lead's ask: "я таскаю другую [кнопку] и мне канвас снапом и направляющими подсказывает и
помогает поставить" — while dragging an element, alignment guides appear against
already-placed objects (edges + centers) and the drag snaps to them.

---

## 1. Parity stance (read first — nobody invent an op)

Snapping is a **UI drag-gesture aid**, not an operation. The commit path is unchanged:
`onMouseUp` still rounds the final fractional world positions once and calls the **existing**
ops (`elements-set` / `moveNodes` / `patchGroup`). Ops receive **final positions only** and
have no idea a snap happened.

Therefore T0244 adds **NO CLI verb, NO HTTP route, NO ops.mjs surface, NO journal change**.
Tool parity is preserved by construction: an agent that calls `moveNodes({moves:[{nodeId,x,y}]})`
lands exactly where it asked; the human page merely *helps the human pick* those x/y before it
commits them. Undo/redo semantics are untouched (one entry at mouseup, same as today).

This is the single most important thing to state to the implementer: do not add a
`snapNodes` op or a `--snap` flag. The feature lives entirely in `site/` (the page) plus one
pure math module.

---

## 2. Where it plugs into the existing pipeline

Verified against the current code:

- **Drag math** — `site/viewport.mjs::dragWorldDelta(grabWorld, screen, viewport)` returns the
  fractional world `{dx,dy}` (T0236, world-anchored, zoom-stable). `site/workspace.js`
  `onMouseMove` uses it in three move cases: `element` (L1176), `group` (L1137), `selection`
  (L1155). Each writes `item.x = item.origX + dx` live (fractional) and calls `requestRender()`.
  Positions round **once** at commit (`commitElementDrag` L1208, `commitGroupDrag` L1238,
  `commitSelectionDrag` L1264). **Snap inserts here**: adjust `dx`/`dy` before writing them.
- **Drag-lifetime caching** — `beginDragGeometryCache()` (L684) caches DOM rects for the drag;
  `beginSelectionDrag()` (L785) builds the `drag` object with per-node `origX/origY`. We add the
  snap precompute alongside, stored on the `drag` object (same lifetime; cleared when `drag=null`
  at mouseup L1412). This is the "precompute once at drag start" requirement.
- **Overlay pass** — `render()` pass 2 chrome (L139-147): group frames, clip ghosts,
  `drawGestureOverlay()` (marquee/region-create rubber-bands). We add `drawSnapGuides(vp)` in the
  same pass, drawn on top. Guides read `drag.activeGuides` set by the last move.
- **Scene-tree math** — `tree.mjs` already exports `childrenOf`, `nodeScope`, `descendantsOf`,
  `isNodeHidden`, `unionBBox`. The new `snap.mjs` imports these; **tree.mjs is not modified.**
- **Modifiers** (audited): during a *move* drag, `onMouseMove` reads **no** modifier today.
  `Shift` is read only at mousedown (selection add / marquee base — L969/981/1003). `Alt` is a
  global clip-ghost peek (`canvas.js` L138, `state.clipGhostPeek`). `Space` = pan. `Ctrl/Cmd`
  at mousedown = deep-select-into-scope (L959). There is **no axis-lock feature**. See §6.

---

## 3. New pure module: `site/snap.mjs`

Pure, dependency-free except `tree.mjs` (mirrors `viewport.mjs`). No DOM. Headless-testable
with plain project objects. Served statically over `/ai_studio/` like the other site `.mjs`.

```js
import { childrenOf, descendantsOf, isNodeHidden, nodeScope, unionBBox } from "../tree.mjs";

// Figma-typical screen tolerance (5-8px). Divided by viewport scale at call time to get
// the WORLD tolerance (zoom-aware): tight when zoomed in, generous when zoomed out.
export const SNAP_SCREEN_PX = 6;

// A candidate alignment line.
//   vertical  : { pos = world X, min/max = world Y extent of the source box }
//   horizontal: { pos = world Y, min/max = world X extent of the source box }
// min/max are carried ONLY so the guide can be drawn spanning the source box (§5).

// collectSnapCandidates(project, draggedIds) -> { vertical: Line[], horizontal: Line[] }
//   Called ONCE at drag start. draggedIds = the TOP-LEVEL dragged nodes (element ids +
//   group ids). Excludes the dragged nodes AND every descendant of a dragged group (they
//   move together, so they are never snap targets). Candidates =
//     - visible siblings in each dragged node's scope (childrenOf(scope) minus excluded), PLUS
//     - the parent group frame of each non-root scope (its own 6 lines: the "center in
//       screen" / "flush to frame edge" targets).
//   Each candidate box emits 3 vertical lines (left / hcenter / right) + 3 horizontal
//   (top / vcenter / bottom). Hidden nodes (isNodeHidden) are skipped.
export function collectSnapCandidates(project, draggedIds) { /* … */ }

// snapDelta(dragBBox, candidates, toleranceWorld) -> { dx, dy, guides }
//   dragBBox = { x, y, w, h } — the RAW (unsnapped) union bbox of the dragged selection
//     THIS frame (origBBox translated by the raw dragWorldDelta).
//   Probes X = [minX, cx, maxX]; probes Y = [minY, cy, maxY].
//   For each axis independently: over all (probe × candidate-line-on-that-axis) pairs,
//   keep the one with the smallest |line.pos - probe| that is <= toleranceWorld
//   (NEAREST wins; boundary delta == tol snaps). dx/dy = that signed correction (may be
//   0 when already exactly aligned — a match still emits a guide). No match on an axis =>
//   that axis contributes 0 and no guide.
//   guides = 0..2 entries: { axis:"x"|"y", pos, min, max } where min/max is the source
//   box extent UNIONED with the SNAPPED dragged-bbox extent on the perpendicular axis
//   (so the line visibly connects target and dragged object).
export function snapDelta(dragBBox, candidates, toleranceWorld) { /* … */ }
```

**Complexity / allocation.** `collectSnapCandidates` runs once (O(nodes)). `snapDelta` runs per
mousemove: O(V + H) simple comparisons over the precomputed number arrays, zero
`getBoundingClientRect`, zero `refresh()`, zero DOM. Its only per-frame allocation is the
returned `{dx,dy,guides}` with ≤2 tiny guide objects — negligible, and coalesced to one call per
animation frame by the existing `requestRender()` gate. Probe values are three locals per axis
(no array churn). This directly answers the T0236 stepped-drag perf complaint: the hot path stays
a handful of arithmetic comparisons.

**Zoom tolerance bound.** `viewport.scale` is clamped to `[0.05, 12]`, so world tolerance ranges
`6/12 = 0.5px` (max zoom-in, near-exact only) … `6/0.05 = 120px` (max zoom-out, generous). Both
sensible; no special-casing needed.

---

## 4. Increment 1 — pure snap math + tests (one fast-worker)

**Files owned (NEW only):**
- `ai_studio/assets/canvas/site/snap.mjs` — the module in §3.
- `ai_studio/assets/canvas/tests/snap.test.mjs` — `node:test`, pure, plain-object projects
  (no store, no DOM), in the style of `tests/viewport_drag.test.mjs`.

No other file is touched in increment 1. `tree.mjs` is imported, not modified.

**Test plan (all headless, plain `{elements:[…], groups:[…]}` fixtures):**
- `collectSnapCandidates`: line count = 3V+3H per candidate box; positions correct
  (left/hcenter/right, top/vcenter/bottom).
- Exclusion: a dragged id and every descendant of a dragged group are absent from candidates.
- Visibility: `visible:false` node excluded; node under a hidden ancestor group excluded.
- Scope: dragging a node inside group G yields G's siblings + G's frame as candidates, and a
  node in a *different* scope is NOT a candidate.
- Root drag: root-level siblings are candidates, no frame line (no parent).
- `snapDelta`: left-edge snap; hcenter snap; right snap; top/vcenter/bottom on Y; **x and y snap
  independently** (one axis matches, the other does not); **nearest wins** among 2+ in-tolerance
  candidates; tolerance boundary (`delta == tol` snaps, `delta > tol` does not); **already-aligned
  (delta 0) still emits a guide**; **no candidate → `{dx:0, dy:0, guides:[]}`**; multi-node union
  bbox snaps by its outer edges/center (not per-member).
- Zoom: same geometry, `toleranceWorld` small vs large flips whether a far candidate snaps.

**Return contract for the worker:** the two exported signatures above must be exact so
increment 2 can import them without adaptation.

---

## 5. Increment 2 — drag integration + guide overlay + bypass (one fast-worker)

**File owned (EDIT only):** `ai_studio/assets/canvas/site/workspace.js`. Nothing else — no
ops.mjs, api.mjs, cli.mjs, tree.mjs, actions.js change.

**5a. Imports.** Add `import { SNAP_SCREEN_PX, collectSnapCandidates, snapDelta } from "./snap.mjs";`
and `unionBBox` from `../tree.mjs` (already importing other tree exports on L65).

**5b. Precompute at drag start** (in `beginSelectionDrag`, L785). Compute once:
- `draggedIds = [...state.selectedIds, ...state.selectedGroupIds]` (top-level dragged nodes).
- `origFrames` = the frame boxes `{x,y,w,h}` of those nodes (elements by their box, groups by
  their frame — both carry x/y/w/h).
- `drag.snapCandidates = collectSnapCandidates(state.project, draggedIds)`.
- `drag.snapBBox = unionBBox(origFrames)` (the selection's ORIGINAL union bbox; w/h are constant
  during a translate).
- `drag.activeGuides = []`.
Attach these to whichever `drag` object the branch builds (`element` / `group` / `selection`).

**5c. Apply snap in `onMouseMove`** — the three move cases (`element` L1176, `group` L1137,
`selection` L1155). After the existing `const { dx, dy } = dragWorldDelta(drag.grabWorld, screen, vp);`:

```js
const bypass = event.ctrlKey || event.metaKey;            // §6 — Figma's ignore-snap modifier
let sdx = dx, sdy = dy;
if (!bypass && drag.snapCandidates) {
  const tol = SNAP_SCREEN_PX / vp.scale;                  // screen px -> world tolerance
  const raw = { x: drag.snapBBox.x + dx, y: drag.snapBBox.y + dy, w: drag.snapBBox.w, h: drag.snapBBox.h };
  const s = snapDelta(raw, drag.snapCandidates, tol);
  sdx = dx + s.dx; sdy = dy + s.dy;
  drag.activeGuides = s.guides;
} else {
  drag.activeGuides = [];
}
```
Then write `sdx/sdy` to the items exactly as the code writes `dx/dy` today (group frame + members
+ subgroups; selection elItems + grpItems; element items). Commit paths are unchanged — they read
the live fractional positions and round once.

**5d. Guide overlay.** Add `drawSnapGuides(vp)` and call it in `render()` pass 2, after the
`drawGroupFrame` loop and `drawClipGhosts`, alongside `drawGestureOverlay()` (L145-147). It reads
`drag?.activeGuides`; draws each as a 1px Figma-pink (`#ff2d78`) line via `imageToScreenPoint`:
- axis `"x"` → vertical line at screen-x `pos`, from screen-y `min` to `max`;
- axis `"y"` → horizontal line at screen-y `pos`, from screen-x `min` to `max`.
Returns immediately when no drag / empty guides, so idle frames pay nothing. Guides vanish
automatically at mouseup (`drag=null` → the post-commit render draws none) and on any frame with no
active snap or with bypass held.

**5e. Untouched by design:** `marquee`, `region-move`, `region-create`, `region-resize`, and `pan`
cases in `onMouseMove` get no snap (marquee is selection, region gestures are source-pixel edits in
a different coordinate space — see open Q2).

**Verification (manual, no unit test — DOM/canvas bound):** on the running page (`:8780`), drag an
element near a sibling's left edge / center / right edge and confirm a pink guide appears and the
box locks; confirm x and y lock independently; confirm holding Ctrl/Cmd suppresses both; confirm a
group drag snaps by its frame; confirm dragging inside a group snaps to the frame + inner siblings;
confirm marquee shows no guides; confirm one Ctrl+Z restores the pre-drag position (commit
unchanged). Zoom to 800% and to 10% and confirm tolerance feels right at both.

---

## 6. Bypass modifier — recommend Ctrl/Cmd

Figma uses Ctrl/Cmd to temporarily ignore snapping. Audit of our move-time bindings:
- `Shift` — free during a move, BUT it is the universal Figma *axis-constrain* key and our likely
  future axis-lock. Using it for bypass would fight muscle memory and pre-collide with that
  feature. **Reject.**
- `Alt` — held-Alt is the global clip-ghost peek (`state.clipGhostPeek`); reusing it would also
  flash clip ghosts. **Reject.**
- `Ctrl/Cmd` — not read during a move. Pressing it mid-drag hits `onKeyDown`'s `if (meta) return;`
  (no state change, no clip ghost). It matches Figma exactly. **Recommend.**

Caveat: `Ctrl/Cmd+click` at mousedown already deep-selects into scope, so a Ctrl-*initiated*
drag both deep-selects and bypasses snap. This composes cleanly (Figma behaves similarly) — and a
user who wants only bypass can press Ctrl *after* the drag starts. Confirm with lead (open Q3).

---

## 7. Edge cases

- **Zoomed way in / out** — handled by `tol = SNAP_SCREEN_PX / vp.scale`; bounded 0.5–120 world px.
- **Multi-select drag** — snaps by the SELECTION union bbox (`drag.snapBBox`); members keep their
  relative offsets because a single `sdx/sdy` shifts them all.
- **Dragging inside a group scope** — candidates are the group's own visible children + the group
  frame; outside nodes are excluded (Figma scopes snapping to the frame you're editing in).
- **Text elements** — valid candidate and draggee via their (nominal/measured) `w/h` box. The box
  is reconciled on paint, so during a drag the last painted size is used (~1-2px drift acceptable,
  consistent with the module's text parity stance).
- **Hidden elements** — excluded as candidates (`isNodeHidden`); a hidden node isn't draggable
  anyway.
- **Marquee** — NOT affected (no snap, no guides). Explicitly excluded in §5e.
- **First object on an empty scope** — `collectSnapCandidates` returns empty arrays →
  `snapDelta` returns `{0,0,[]}`; the drag behaves exactly as today (no cost, no guides).
- **Dragged group's own members** — excluded via `descendantsOf`, so a group never snaps to its
  own contents.
- **Clipped-out siblings** — v1 uses their raw geometry box as a candidate (ignores clip); minor,
  Figma also snaps to geometry not visible pixels.

---

## 8. Open questions for the lead (only the design-changing few)

1. **Parent-frame center lines.** Constraint says the frame contributes edges + center. Center
   snapping gives the very common "center this widget in the screen," but a mid-frame vcenter/hcenter
   can feel *sticky* while dragging past it. Ship all 6 frame lines (recommended), or frame **edges
   only** + let the inspector Align row own centering?
2. **Region-edit drags.** Should moving/resizing regions *inside* an image (mode B, source-pixel
   space) also get snap+guides against other regions / the image bounds? v1 excludes them (different
   coordinate space, separate work). Confirm out-of-scope for T0244.
3. **Bypass key.** Confirm Ctrl/Cmd (Figma-native, §6) despite it also being the deep-select
   modifier at mousedown, or prefer another key.

(Deliberately NOT asked — decided in-doc: guide color `#ff2d78`; guide length spans the matched
candidate box unioned with the dragged bbox on the perpendicular axis; nearest-candidate wins.)

---

## 9. Conclusion

**Recommended order:** Increment 1 (pure `snap.mjs` + `snap.test.mjs`) first — it is fully headless,
locks the two function signatures, and de-risks the geometry before any UI wiring. Then Increment 2
(workspace.js integration + guide overlay + Ctrl/Cmd bypass), verified manually on `:8780`. Two
fast-workers, sequential (2 depends on 1's exports); no parallelism benefit.

**v1 cut line (explicit):** edge/center **line guides + snap only**. Out to v2: equal-spacing (gap)
badges, distance labels, snap-to-grid.

**Risks:**
- *Low* — perf: the hot path is O(candidates) arithmetic on precomputed arrays, no layout/refresh;
  aligns with the T0236 fix. Watch only pathological scenes (hundreds of same-scope siblings) — if
  ever an issue, dedupe candidate lines by rounded `pos` in `collectSnapCandidates` (noted, not
  needed for v1).
- *Low* — the guide's perpendicular span depends on the snapped bbox computed after both axes
  resolve; get the ordering right in `snapDelta` (resolve dx and dy, then build guide extents).
- *Design* — Q1 (frame center) is the only choice that visibly changes feel; safe default is all 6,
  reversible.
