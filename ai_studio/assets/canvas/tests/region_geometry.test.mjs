// Pure region geometry helpers (site/regions.js): polygon detection, integer bbox,
// the proportional transform used by move/resize commits, and even-odd point-in-polygon
// hit-testing. DOM-free (the viewport math it imports is pure), so it runs in plain node.
//   node --test ai_studio/assets/canvas/tests/region_geometry.test.mjs
import test from "node:test";
import assert from "node:assert/strict";
import { hitRegion, polygonBBox, regionHasPolygon, transformPolygon } from "../site/regions.js";

test("regionHasPolygon needs a >=3-point polygon", () => {
  assert.equal(regionHasPolygon({ polygon: [[0, 0], [1, 0], [0, 1]] }), true);
  assert.equal(regionHasPolygon({ polygon: [[0, 0], [1, 0]] }), false);
  assert.equal(regionHasPolygon({ rect: [0, 0, 4, 4] }), false);
  assert.equal(regionHasPolygon(null), false);
});

test("polygonBBox is the floor-min / ceil-max integer bounding box", () => {
  assert.deepEqual(polygonBBox([[8, 8], [28, 8], [8, 28]]), [8, 8, 20, 20]);
  assert.deepEqual(polygonBBox([[2.4, 2.6], [10, 3], [4, 9]]), [2, 2, 8, 7]);
});

test("transformPolygon translates on move (same w/h) and rescales on resize", () => {
  const poly = [[10, 10], [30, 10], [10, 30]];
  // Move: new bbox shifted by (+5, +7), same size => pure translation of every point.
  assert.deepEqual(transformPolygon(poly, [10, 10, 20, 20], [15, 17, 20, 20]), [[15, 17], [35, 17], [15, 37]]);
  // Resize: double the bbox from the same origin => points scale about that origin.
  assert.deepEqual(transformPolygon(poly, [10, 10, 20, 20], [10, 10, 40, 40]), [[10, 10], [50, 10], [10, 50]]);
  // Below 3 points there is nothing to transform.
  assert.equal(transformPolygon([[0, 0], [1, 1]], [0, 0, 1, 1], [0, 0, 2, 2]), null);
});

test("hitRegion uses even-odd point-in-polygon for polygonal regions (not the bbox)", () => {
  // Element at origin with source == display (sx = sy = 1): a right triangle over the
  // bbox [8, 8, 20, 20] with the hypotenuse x + y = 36.
  const element = {
    x: 0,
    y: 0,
    w: 64,
    h: 48,
    source_w: 64,
    source_h: 48,
    regions: [{ id: "tri", rect: [8, 8, 20, 20], polygon: [[8, 8], [28, 8], [8, 28]] }],
  };
  // Inside the triangle: hit.
  assert.equal(hitRegion({ x: 12, y: 12 }, element) && hitRegion({ x: 12, y: 12 }, element).id, "tri");
  // Inside the bbox but OUTSIDE the triangle (x + y = 48 > 36): miss (a rect hit would match).
  assert.equal(hitRegion({ x: 24, y: 24 }, element), null);
});

test("hitRegion falls back to rect AABB for plain regions", () => {
  const element = {
    x: 0,
    y: 0,
    w: 64,
    h: 48,
    source_w: 64,
    source_h: 48,
    regions: [{ id: "box", rect: [8, 8, 20, 20] }],
  };
  assert.equal(hitRegion({ x: 24, y: 24 }, element) && hitRegion({ x: 24, y: 24 }, element).id, "box");
  assert.equal(hitRegion({ x: 40, y: 40 }, element), null);
});
