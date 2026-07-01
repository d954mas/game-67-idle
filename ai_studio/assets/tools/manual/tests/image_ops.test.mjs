import assert from "node:assert/strict";
import test from "node:test";

import {
  buildStoredZip,
  createSliceRects,
  cropPlan,
  crc32,
  safeAssetBaseName,
  sliceFileName,
  toHexColor,
  transformPixels,
  trimTransparentRect,
} from "../image_ops.mjs";

test("createSliceRects splits a grid with gaps in row-major order", () => {
  const rects = createSliceRects(100, 50, {
    mode: "grid",
    columns: 2,
    rows: 2,
    gapX: 4,
    gapY: 2,
  });

  assert.deepEqual(rects, [
    { index: 0, row: 0, column: 0, x: 0, y: 0, width: 48, height: 24 },
    { index: 1, row: 0, column: 1, x: 52, y: 0, width: 48, height: 24 },
    { index: 2, row: 1, column: 0, x: 0, y: 26, width: 48, height: 24 },
    { index: 3, row: 1, column: 1, x: 52, y: 26, width: 48, height: 24 },
  ]);
});

test("createSliceRects tile mode ignores incomplete edge tiles", () => {
  const rects = createSliceRects(70, 40, {
    mode: "tile",
    tileWidth: 24,
    tileHeight: 16,
    gapX: 2,
    gapY: 4,
  });

  assert.equal(rects.length, 4);
  assert.deepEqual(rects.at(-1), { index: 3, row: 1, column: 1, x: 26, y: 20, width: 24, height: 16 });
});

test("slice filenames are stable and safe", () => {
  const rect = { index: 7, row: 2, column: 3 };

  assert.equal(safeAssetBaseName(" raw sheet!.png "), "raw_sheet");
  assert.equal(sliceFileName(" raw sheet!.png ", rect), "raw_sheet_008_r03_c04.png");
});

test("transformPixels flattens semi-transparent pixels onto a background", () => {
  const pixels = new Uint8ClampedArray([200, 100, 0, 128]);
  const result = transformPixels(pixels, 1, 1, { mode: "flatten", background: "#0000ff" });

  assert.deepEqual([...result.data], [100, 50, 127, 255]);
  assert.equal(result.changedPixels, 1);
});

test("transformPixels thresholds existing alpha", () => {
  const pixels = new Uint8ClampedArray([
    10, 20, 30, 127,
    40, 50, 60, 128,
  ]);
  const result = transformPixels(pixels, 2, 1, { mode: "threshold", threshold: 128 });

  assert.deepEqual([...result.data], [
    10, 20, 30, 0,
    40, 50, 60, 255,
  ]);
});

test("transformPixels removes only border-connected key color", () => {
  const magenta = [255, 0, 255, 255];
  const green = [0, 255, 0, 255];
  const pixels = new Uint8ClampedArray([
    ...magenta, ...magenta, ...magenta,
    ...magenta, ...green, ...magenta,
    ...magenta, ...magenta, ...magenta,
  ]);
  const result = transformPixels(pixels, 3, 3, { mode: "key", keyColor: "#ff00ff", tolerance: 0 });

  assert.equal(result.changedPixels, 8);
  assert.deepEqual([...result.data.slice(16, 20)], green);
  assert.deepEqual([...result.data.slice(0, 4)], [0, 0, 0, 0]);
});

test("trimTransparentRect returns the visible bounds with padding", () => {
  const pixels = new Uint8ClampedArray(4 * 4 * 4);
  pixels[(1 * 4 + 2) * 4 + 3] = 255;
  pixels[(2 * 4 + 1) * 4 + 3] = 255;

  assert.deepEqual(trimTransparentRect(pixels, 4, 4, 1), { x: 0, y: 0, width: 4, height: 4 });
  assert.deepEqual(trimTransparentRect(pixels, 4, 4, 0), { x: 1, y: 1, width: 2, height: 2 });
});

test("buildStoredZip writes local headers and central directory", () => {
  const zip = buildStoredZip([
    { name: "a.png", data: new Uint8Array([1, 2, 3]) },
    { name: "b.png", data: new Uint8Array([4, 5]) },
  ], new Date("2026-01-02T03:04:05Z"));
  const view = new DataView(zip.buffer);
  const text = new TextDecoder().decode(zip);

  assert.equal(view.getUint32(0, true), 0x04034b50);
  assert.equal(view.getUint32(zip.length - 22, true), 0x06054b50);
  assert.match(text, /a\.png/);
  assert.match(text, /b\.png/);
  assert.equal(crc32(new Uint8Array([1, 2, 3])), 0x55bc801d);
});

test("cropPlan records source, alpha settings, and crop outputs", () => {
  const rects = createSliceRects(32, 16, { mode: "grid", columns: 2, rows: 1 });
  const plan = cropPlan({
    sourceName: "sheet.png",
    width: 32,
    height: 16,
    alpha: { mode: "key", keyColor: "#00ff00" },
    slice: { mode: "grid", columns: 2, rows: 1 },
    rects,
    prefix: "button",
  });

  assert.equal(plan.schema, "ai_studio.manual_asset_prep.v1");
  assert.equal(plan.crops.length, 2);
  assert.deepEqual(plan.crops[1].rect, [16, 0, 16, 16]);
  assert.equal(plan.crops[1].output, "button_002_r01_c02.png");
  assert.equal(toHexColor(0, 255, 0), "#00ff00");
});
