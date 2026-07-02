import { strict as assert } from "node:assert";
import test from "node:test";
import {
  applyAlphaPreviewMatte,
  applyGenerationAlphaDiagnostic,
  applyPolygonPreviewMask,
  parseKeyColor,
} from "../asset_tools_alpha_preview.mjs";

function imageData(width, height, pixels) {
  return {
    width,
    height,
    data: new Uint8ClampedArray(pixels.flat()),
  };
}

function pixel(result, x, y) {
  const index = (y * result.width + x) * 4;
  return Array.from(result.data.slice(index, index + 4));
}

test("parseKeyColor accepts CSS hex values", () => {
  assert.deepEqual(parseKeyColor("#ff00ff"), [255, 0, 255]);
  assert.deepEqual(parseKeyColor("00ff80"), [0, 255, 128]);
});

test("alpha preview removes exact key pixels and clears their RGB", () => {
  const preview = imageData(2, 1, [
    [255, 0, 255, 255],
    [40, 80, 120, 255],
  ]);

  const stats = applyAlphaPreviewMatte(preview, { keyColor: "#ff00ff" });

  assert.equal(stats.transparentPixels, 1);
  assert.deepEqual(pixel(preview, 0, 0), [0, 0, 0, 0]);
  assert.deepEqual(pixel(preview, 1, 0), [40, 80, 120, 255]);
});

test("alpha preview decontaminates purple edge spill next to transparent key", () => {
  const preview = imageData(3, 1, [
    [255, 0, 255, 255],
    [236, 62, 228, 255],
    [80, 60, 40, 255],
  ]);

  const stats = applyAlphaPreviewMatte(preview, { keyColor: "#ff00ff" });

  assert.equal(pixel(preview, 0, 0)[3], 0);
  assert.equal(stats.despilledPixels, 1);
  assert.ok(pixel(preview, 1, 0)[0] < 236);
  assert.ok(pixel(preview, 1, 0)[2] < 228);
  assert.deepEqual(pixel(preview, 2, 0), [80, 60, 40, 255]);
});

test("alpha preview keeps isolated purple art that is not touching the matte", () => {
  const preview = imageData(3, 3, [
    [20, 30, 40, 255],
    [20, 30, 40, 255],
    [20, 30, 40, 255],
    [20, 30, 40, 255],
    [170, 80, 190, 255],
    [20, 30, 40, 255],
    [20, 30, 40, 255],
    [20, 30, 40, 255],
    [20, 30, 40, 255],
  ]);

  applyAlphaPreviewMatte(preview, { keyColor: "#ff00ff" });

  assert.deepEqual(pixel(preview, 1, 1), [170, 80, 190, 255]);
});

test("alpha preview applies polygon masks in region-local preview space", () => {
  const preview = imageData(3, 3, Array.from({ length: 9 }, () => [40, 120, 200, 255]));

  const stats = applyAlphaPreviewMatte(preview, {
    keyColor: "#ff00ff",
    rect: { x: 10, y: 10, width: 3, height: 3 },
    polygon: [
      [10, 10],
      [13, 10],
      [10, 13],
    ],
  });

  assert.ok(stats.polygonMaskedPixels > 0);
  assert.equal(pixel(preview, 0, 0)[3], 255);
  assert.equal(pixel(preview, 2, 2)[3], 0);
});

test("source preview can apply polygon masks without key matte", () => {
  const preview = imageData(3, 3, Array.from({ length: 9 }, () => [90, 120, 200, 255]));

  const stats = applyPolygonPreviewMask(preview, {
    rect: { x: 10, y: 10, width: 3, height: 3 },
    polygon: [
      [10, 10],
      [13, 10],
      [10, 13],
    ],
  });

  assert.ok(stats.polygonMaskedPixels > 0);
  assert.deepEqual(pixel(preview, 0, 0), [90, 120, 200, 255]);
  assert.deepEqual(pixel(preview, 2, 2), [0, 0, 0, 0]);
});

test("generation alpha diagnostic shows math matte output and marks changed edge pixels", () => {
  const preview = imageData(3, 1, [
    [255, 0, 255, 255],
    [236, 62, 228, 255],
    [40, 80, 200, 255],
  ]);

  const stats = applyGenerationAlphaDiagnostic(preview, { keyColor: "#ff00ff" });

  assert.equal(pixel(preview, 0, 0)[3], 0);
  assert.equal(stats.diagnosticPixels, 1);
  assert.ok(pixel(preview, 1, 0)[0] > 145);
  assert.ok(pixel(preview, 1, 0)[1] > 62);
  assert.deepEqual(pixel(preview, 2, 0), [40, 80, 200, 255]);
});
