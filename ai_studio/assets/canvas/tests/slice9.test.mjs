// T0233 — 9-slice element support: shared math (slice9.mjs), the setSlice9 op, and
// render-pixel parity through the REAL render_group.py. Run:
//   node --test ai_studio/assets/canvas/tests/slice9.test.mjs
//
// Lead ask (verbatim): «Сделать что-то слайс9 картинкой (чтобы проверить а
// работает ли слайс9)» — corners stay a fixed size, edges stretch one axis, the
// center stretches both, when the element box is resized. `scale` (T0233 scope
// addition, lead: «важно чтобы я мог скейлить края, иногда мне нужно больше или
// меньше») multiplies the DESTINATION corner/edge band only; the source crop never
// moves.
//
// The two RENDER-PIXEL tests drive the real render_group.py through the warm
// worker (renderGroup — same harness pattern as transform.test.mjs lines 6-39) and
// skip cleanly when the studio venv / Pillow is unavailable. Every other test here
// is pure/store-level and never touches Python.
import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { resolve } from "node:path";
import {
  addImage,
  addText,
  assignToGroup,
  createGroup,
  createProject,
  getProject,
  patchElement,
  renderGroup,
  setSlice9,
  undoOp,
} from "../ops.mjs";
import { slice9Patches, validateSlice9 } from "../slice9.mjs";
import { decodePng, encodePng } from "./png_fixture.mjs";

// Returns REPO_ROOT (not the temp dir) — renderGroup needs the REAL repo root to
// find tools/render_group.py; project storage is redirected via
// CANVAS_PROJECTS_ROOT, exactly like ops.test.mjs / transform.test.mjs.
const REPO_ROOT = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

function tempProjects(t) {
  const dir = mkdtempSync(join(tmpdir(), "canvas-slice9-"));
  const previous = process.env.CANVAS_PROJECTS_ROOT;
  process.env.CANVAS_PROJECTS_ROOT = dir;
  t.after(() => {
    if (previous === undefined) delete process.env.CANVAS_PROJECTS_ROOT;
    else process.env.CANVAS_PROJECTS_ROOT = previous;
    rmSync(dir, { recursive: true, force: true });
  });
  return REPO_ROOT;
}

function img(root, pid, name, w, h) {
  return addImage(root, pid, { name, bytes: encodePng(w, h, () => [10, 20, 30]) }).element;
}

function byId(project, id) {
  return (project.elements || []).find((item) => item.id === id);
}

// ---- validateSlice9 (pure, no Python) ------------------------------------------

test("validateSlice9 normalizes valid insets and omits scale when it is 1 (absent = default, matches rotation:0/flipH:false elsewhere)", () => {
  assert.deepEqual(validateSlice9({ left: 4, top: 6, right: 10, bottom: 8 }, 40, 32), {
    left: 4,
    top: 6,
    right: 10,
    bottom: 8,
  });
  assert.deepEqual(validateSlice9({ left: 4, top: 6, right: 10, bottom: 8, scale: 1 }, 40, 32), {
    left: 4,
    top: 6,
    right: 10,
    bottom: 8,
  });
  assert.deepEqual(validateSlice9({ left: 0, top: 0, right: 0, bottom: 0, scale: 2 }, 40, 32), {
    left: 0,
    top: 0,
    right: 0,
    bottom: 0,
    scale: 2,
  });
});

test("validateSlice9 rejects a negative or non-integer inset", () => {
  assert.throws(() => validateSlice9({ left: -1, top: 0, right: 0, bottom: 0 }, 40, 32), /non-negative integer/);
  assert.throws(() => validateSlice9({ left: 1.5, top: 0, right: 0, bottom: 0 }, 40, 32), /non-negative integer/);
  assert.throws(() => validateSlice9({ left: "x", top: 0, right: 0, bottom: 0 }, 40, 32), /non-negative integer/);
});

test("validateSlice9 rejects left+right >= source_w and top+bottom >= source_h (the engine's sl+sr < source_w invariant)", () => {
  assert.throws(
    () => validateSlice9({ left: 20, top: 0, right: 20, bottom: 0 }, 40, 32),
    /left\+right \(40\) must be < source width 40/,
  );
  assert.throws(
    () => validateSlice9({ left: 0, top: 16, right: 0, bottom: 16 }, 40, 32),
    /top\+bottom \(32\) must be < source height 32/,
  );
  // Exceeding (not just touching) the bound is loud too.
  assert.throws(() => validateSlice9({ left: 25, top: 0, right: 20, bottom: 0 }, 40, 32), /must be < source width/);
});

test("validateSlice9 rejects a non-object insets and a malformed insets shape", () => {
  assert.throws(() => validateSlice9(null, 40, 32), /must be an object/);
  assert.throws(() => validateSlice9("nope", 40, 32), /must be an object/);
});

test("validateSlice9 rejects a zero/negative/non-finite/over-cap scale", () => {
  const base = { left: 1, top: 1, right: 1, bottom: 1 };
  assert.throws(() => validateSlice9({ ...base, scale: 0 }, 40, 32), /scale must be a finite number in \(0, 16\]/);
  assert.throws(() => validateSlice9({ ...base, scale: -2 }, 40, 32), /scale must be a finite number/);
  assert.throws(() => validateSlice9({ ...base, scale: NaN }, 40, 32), /scale must be a finite number/);
  assert.throws(() => validateSlice9({ ...base, scale: Infinity }, 40, 32), /scale must be a finite number/);
  assert.throws(() => validateSlice9({ ...base, scale: 16.0001 }, 40, 32), /scale must be a finite number in \(0, 16\]/);
  // Exactly the cap is allowed.
  assert.deepEqual(validateSlice9({ ...base, scale: 16 }, 40, 32), { ...base, scale: 16 });
});

// ---- slice9Patches shape (pure, no Python) -------------------------------------

test("slice9Patches returns 9 patches for a normal box; corner dst sizes == insets at scale 1; edges stretch one axis; center stretches both", () => {
  const patches = slice9Patches({ left: 4, top: 6, right: 10, bottom: 8 }, 40, 32, 100, 70);
  assert.equal(patches.length, 9);

  const at = (sx, sy) => patches.find((p) => p.sx === sx && p.sy === sy);
  // Corners: dst size == the raw inset (scale 1), independent of dstW/H.
  assert.deepEqual({ dw: at(0, 0).dw, dh: at(0, 0).dh }, { dw: 4, dh: 6 }, "top-left corner");
  assert.deepEqual({ dw: at(30, 0).dw, dh: at(30, 0).dh }, { dw: 10, dh: 6 }, "top-right corner");
  assert.deepEqual({ dw: at(0, 24).dw, dh: at(0, 24).dh }, { dw: 4, dh: 8 }, "bottom-left corner");
  assert.deepEqual({ dw: at(30, 24).dw, dh: at(30, 24).dh }, { dw: 10, dh: 8 }, "bottom-right corner");
  // Top/bottom edges stretch X only (dh fixed = the inset); left/right edges stretch Y only.
  assert.deepEqual({ dw: at(4, 0).dw, dh: at(4, 0).dh }, { dw: 86, dh: 6 }, "top edge stretches X only");
  assert.deepEqual({ dw: at(0, 6).dw, dh: at(0, 6).dh }, { dw: 4, dh: 56 }, "left edge stretches Y only");
  // Center stretches both.
  assert.deepEqual({ dw: at(4, 6).dw, dh: at(4, 6).dh }, { dw: 86, dh: 56 }, "center stretches both axes");
  // Source rects are exactly the raw insets/middle band, untouched by dst size.
  assert.deepEqual({ sx: at(4, 6).sx, sy: at(4, 6).sy, sw: at(4, 6).sw, sh: at(4, 6).sh }, { sx: 4, sy: 6, sw: 26, sh: 18 });
});

test("slice9Patches drops zero-area patches (a squished corner-sum collapses the middle band to 0 and it is omitted)", () => {
  // left+right == dstW exactly (4+4 == 8): the center/vertical-edge COLUMN collapses to
  // 0-width; top/bottom are both nonzero and dstH is generous, so all 3 ROWS survive.
  const patches = slice9Patches({ left: 4, top: 2, right: 4, bottom: 2 }, 20, 10, 8, 10);
  assert.equal(patches.length, 6, "3 rows x 2 columns survive (the middle COLUMN collapses to 0-width and is dropped)");
  assert.ok(!patches.some((p) => p.sx === 4), "the middle column's source band (sx=4) never appears — its patches were dropped, not zero-width");
  assert.ok(!patches.some((p) => p.dw <= 0 || p.dh <= 0), "no zero/negative-area patch is emitted");
});

test("slice9Patches proportionally clamps (never negative) when the box is smaller than the corner sum on an axis", () => {
  // left+right = 30 > dstW = 12: both corners squeeze to fit, proportionally (2:1 ratio kept).
  const patches = slice9Patches({ left: 20, top: 0, right: 10, bottom: 0 }, 40, 10, 12, 10);
  const left = patches.find((p) => p.sx === 0);
  const right = patches.find((p) => p.sx === 30);
  assert.ok(left.dw > 0 && right.dw > 0, "both corners keep positive width");
  assert.ok(left.dw + right.dw <= 12 + 1e-9, "corners never overlap/exceed the box");
  assert.ok(Math.abs(left.dw / right.dw - 2) < 1e-9, "the 20:10 ratio is preserved under the clamp");
  assert.ok(!patches.some((p) => p.dw < 0 || p.dh < 0), "no patch ever goes negative");
});

test("slice9Patches scale=2 doubles the corner/edge DESTINATION dims while the SOURCE rects stay identical to scale=1", () => {
  const base = slice9Patches({ left: 4, top: 6, right: 10, bottom: 8 }, 40, 32, 100, 70);
  const scaled = slice9Patches({ left: 4, top: 6, right: 10, bottom: 8, scale: 2 }, 40, 32, 100, 70);
  assert.equal(scaled.length, base.length);
  for (let i = 0; i < base.length; i += 1) {
    const b = base[i];
    const s = scaled.find((p) => p.sx === b.sx && p.sy === b.sy);
    assert.deepEqual({ sx: s.sx, sy: s.sy, sw: s.sw, sh: s.sh }, { sx: b.sx, sy: b.sy, sw: b.sw, sh: b.sh }, "source crop is untouched by scale");
    const isCornerCol = b.sx === 0 || b.sx === 30; // left/right bands
    const isCornerRow = b.sy === 0 || b.sy === 24; // top/bottom bands
    if (isCornerCol) assert.equal(s.dw, b.dw * 2, `dw doubles at sx=${b.sx}`);
    else assert.ok(s.dw < b.dw, "the stretched center/edge column shrinks to make room for the fatter corners");
    if (isCornerRow) assert.equal(s.dh, b.dh * 2, `dh doubles at sy=${b.sy}`);
    else assert.ok(s.dh < b.dh, "the stretched center/edge row shrinks to make room for the fatter corners");
  }
});

test("slice9Patches scale=0.5 halves the corner/edge DESTINATION dims", () => {
  const base = slice9Patches({ left: 4, top: 6, right: 10, bottom: 8 }, 40, 32, 100, 70);
  const scaled = slice9Patches({ left: 4, top: 6, right: 10, bottom: 8, scale: 0.5 }, 40, 32, 100, 70);
  const bCorner = base.find((p) => p.sx === 0 && p.sy === 0);
  const sCorner = scaled.find((p) => p.sx === 0 && p.sy === 0);
  assert.equal(sCorner.dw, bCorner.dw * 0.5);
  assert.equal(sCorner.dh, bCorner.dh * 0.5);
});

// ---- setSlice9 op: journal / undo / clear / loud (pure, no Python) -------------

test("setSlice9 sets insets, is journaled, and undo restores the previous (absent) slice9", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "Slice9Op" }).id;
  const element = img(root, pid, "panel", 40, 32);
  const seq0 = getProject(root, pid).history_seq;

  const result = setSlice9(root, { projectId: pid, elementId: element.id, insets: { left: 4, top: 6, right: 10, bottom: 8 } });
  assert.deepEqual(result.element.slice9, { left: 4, top: 6, right: 10, bottom: 8 });
  assert.equal(getProject(root, pid).history_seq, seq0 + 1, "one journal entry");

  const undone = undoOp(root, { projectId: pid }).project;
  assert.equal("slice9" in byId(undone, element.id), false, "undo restores the pre-set (absent) slice9");
});

test("setSlice9 with insets:null clears a previously-set slice9 (one more journal entry, undoable)", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "Slice9Clear" }).id;
  const element = img(root, pid, "panel", 40, 32);
  setSlice9(root, { projectId: pid, elementId: element.id, insets: { left: 4, top: 6, right: 10, bottom: 8 } });
  const seq1 = getProject(root, pid).history_seq;

  const cleared = setSlice9(root, { projectId: pid, elementId: element.id, insets: null });
  assert.equal("slice9" in cleared.element, false);
  assert.equal(getProject(root, pid).history_seq, seq1 + 1);

  const undone = undoOp(root, { projectId: pid }).project;
  assert.deepEqual(byId(undone, element.id).slice9, { left: 4, top: 6, right: 10, bottom: 8 }, "undo restores the cleared slice9");
});

test("setSlice9 surfaces validateSlice9's loud error and writes nothing", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "Slice9Loud" }).id;
  const element = img(root, pid, "panel", 40, 32);
  const seq0 = getProject(root, pid).history_seq;

  assert.throws(
    () => setSlice9(root, { projectId: pid, elementId: element.id, insets: { left: 30, top: 0, right: 30, bottom: 0 } }),
    /left\+right \(60\) must be < source width 40/,
  );
  assert.equal(getProject(root, pid).history_seq, seq0, "no journal entry on a rejected set");
  assert.equal("slice9" in byId(getProject(root, pid), element.id), false);
});

test("setSlice9 is image-only — a text element throws (mirrors the flipH/flipV image-only guard)", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "Slice9TextGuard" }).id;
  const text = addText(root, pid, { content: "Hi" }).element;
  assert.throws(
    () => setSlice9(root, { projectId: pid, elementId: text.id, insets: { left: 1, top: 1, right: 1, bottom: 1 } }),
    /slice9 is image-only/,
  );
});

test("setSlice9 requires projectId/elementId/insets (undefined insets is loud, not a silent clear)", (t) => {
  const root = tempProjects(t);
  const pid = createProject(root, { title: "Slice9Required" }).id;
  const element = img(root, pid, "panel", 40, 32);
  assert.throws(() => setSlice9(root, { elementId: element.id, insets: {} }), /requires projectId/);
  assert.throws(() => setSlice9(root, { projectId: pid, insets: {} }), /requires elementId/);
  assert.throws(() => setSlice9(root, { projectId: pid, elementId: element.id }), /requires insets/);
});

// ---- render-pixel parity: the ASYMMETRIC 9-color fixture through the REAL -------
// ---- render_group.py (the "does slice9 actually work" proof, T0233 section 5.3) --
//
// A 9-color source (distinct RGB per zone) with ASYMMETRIC insets (left != top !=
// right != bottom) sliced into a non-square box. Expected destination band
// boundaries are computed HERE, independently of slice9Patches/slice9_patches (the
// same "don't test the code under test with itself" stance transform.test.mjs's
// transformOffset helper takes) — this is what catches an L<->R / T<->B swap or a
// corner/edge/center mixup that a JS==Python patch-equality golden (Packet 2) would
// let straight through.

const SRC_W = 40;
const SRC_H = 32;
const INSETS = { left: 4, top: 6, right: 10, bottom: 8 }; // all 4 distinct
const DST_W = 100;
const DST_H = 70;
// [row][col], row 0 = top, col 0 = left — 9 visually-distinct colors.
const ZONE_COLORS = [
  [
    [230, 25, 25],
    [25, 200, 25],
    [25, 25, 230],
  ],
  [
    [230, 200, 25],
    [200, 25, 200],
    [25, 200, 200],
  ],
  [
    [140, 70, 10],
    [10, 140, 70],
    [70, 10, 140],
  ],
];

function zoneOf(x, y) {
  const col = x < INSETS.left ? 0 : x < SRC_W - INSETS.right ? 1 : 2;
  const row = y < INSETS.top ? 0 : y < SRC_H - INSETS.bottom ? 1 : 2;
  return { row, col };
}

function nineColorFixtureBytes() {
  return encodePng(SRC_W, SRC_H, (x, y) => {
    const { row, col } = zoneOf(x, y);
    return ZONE_COLORS[row][col];
  });
}

function closeColor(actual, expected, tol = 20) {
  return Math.abs(actual[0] - expected[0]) <= tol && Math.abs(actual[1] - expected[1]) <= tol && Math.abs(actual[2] - expected[2]) <= tol;
}

// Build the fixture project: a slice9 panel at DST_W x DST_H inside a generously
// sized group at origin (0,0), so world coords == element-local coords.
function buildFixture(root, insets) {
  const pid = createProject(root, { title: "Slice9Parity" }).id;
  const element = addImage(root, pid, { name: "panel9.png", bytes: nineColorFixtureBytes(), x: 0, y: 0 }).element;
  setSlice9(root, { projectId: pid, elementId: element.id, insets });
  patchElement(root, pid, element.id, { w: DST_W, h: DST_H });
  const group = createGroup(root, { projectId: pid, name: "Screen", x: 0, y: 0, w: 160, h: 120 }).group;
  assignToGroup(root, { projectId: pid, elementIds: [element.id], groupId: group.id });
  return { pid, elementId: element.id, groupId: group.id };
}

test("render-pixel parity: an asymmetric 9-color fixture renders with fixed corners / stretched edges / stretched center at scale 1 AND 2 (catches an L/R or T/B swap)", async (t) => {
  const root = tempProjects(t);
  const { pid, groupId } = buildFixture(root, INSETS);

  // Independently-reasoned destination band boundaries (hand-derived from the
  // design's algorithm description, NOT by calling slice9Patches).
  const dxs = [0, INSETS.left, DST_W - INSETS.right, DST_W];
  const dys = [0, INSETS.top, DST_H - INSETS.bottom, DST_H];
  const centerOf = (lo, hi) => (lo + hi) / 2;
  const colCenters = [centerOf(dxs[0], dxs[1]), centerOf(dxs[1], dxs[2]), centerOf(dxs[2], dxs[3])];
  const rowCenters = [centerOf(dys[0], dys[1]), centerOf(dys[1], dys[2]), centerOf(dys[2], dys[3])];

  for (const scale of [1, 2]) {
    let result;
    try {
      result = await renderGroup(root, { projectId: pid, groupId, scale });
    } catch (error) {
      t.skip(`render_group.py / PIL unavailable: ${error.message}`);
      return;
    }
    const png = decodePng(readFileSync(result.path));
    for (let row = 0; row < 3; row += 1) {
      for (let col = 0; col < 3; col += 1) {
        const px = Math.round(colCenters[col] * scale);
        const py = Math.round(rowCenters[row] * scale);
        const actual = png.at(px, py);
        const expected = ZONE_COLORS[row][col];
        assert.ok(
          closeColor(actual, expected),
          `scale ${scale} zone (row ${row}, col ${col}) at (${px},${py}): expected [${expected}], got [${actual}]`,
        );
      }
    }
  }
});

test("render-pixel parity: slice9.scale=2 fattens corners/edges — sample points recomputed from the ACTUAL slice9Patches output, not hardcoded pixels", async (t) => {
  const root = tempProjects(t);
  const insets = { ...INSETS, scale: 2 };
  const { pid, groupId } = buildFixture(root, insets);

  // Per the T0233 scope addition: the fatter/thinner band sizes come from the patch
  // math itself (slice9Patches), not a hand re-derivation — this test's job is to
  // confirm render_group.py's OWN (independently-written) scale handling agrees
  // with where the shared JS math says each patch should land.
  const patches = slice9Patches(insets, SRC_W, SRC_H, DST_W, DST_H);
  assert.ok(patches.length === 9, "scale=2 still fits without triggering the clamp for this fixture");

  let result;
  try {
    result = await renderGroup(root, { projectId: pid, groupId, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const png = decodePng(readFileSync(result.path));
  for (const p of patches) {
    const px = Math.round(p.dx + p.dw / 2);
    const py = Math.round(p.dy + p.dh / 2);
    const { row, col } = zoneOf(p.sx + 1, p.sy + 1); // +1: nudge off a sx=0/sy=0 boundary into the band
    const expected = ZONE_COLORS[row][col];
    const actual = png.at(px, py);
    assert.ok(
      closeColor(actual, expected),
      `patch sx=${p.sx} sy=${p.sy} centered at (${px},${py}): expected [${expected}], got [${actual}]`,
    );
  }
});

// ---- transform composition: slice9 + rotation/flip (T0233 design section 4.0) --

test("transform composition: a slice9 element with rotation=90 + flipH renders through render_group.py without error, and the box-center pixel (fixed by rotation about that same center) still shows the MM zone color", async (t) => {
  const root = tempProjects(t);
  const { pid, elementId, groupId } = buildFixture(root, INSETS);
  patchElement(root, pid, elementId, { rotation: 90, flipH: true });

  let result;
  try {
    result = await renderGroup(root, { projectId: pid, groupId, scale: 1 });
  } catch (error) {
    t.skip(`render_group.py / PIL unavailable: ${error.message}`);
    return;
  }
  const png = decodePng(readFileSync(result.path));
  // A point at the element's own box CENTER is invariant under rotation/flip about
  // that center (rotating/mirroring about a point never moves the point itself) —
  // so the MM (center) zone must still be the color sampled there, regardless of
  // the transform. AA-tolerant: a generous box center well inside the large MM band.
  const cx = Math.round(DST_W / 2);
  const cy = Math.round(DST_H / 2);
  const actual = png.at(cx, cy);
  assert.ok(
    closeColor(actual, ZONE_COLORS[1][1]),
    `expected the MM zone color [${ZONE_COLORS[1][1]}] at the rotation-invariant box center (${cx},${cy}), got [${actual}]`,
  );
});
