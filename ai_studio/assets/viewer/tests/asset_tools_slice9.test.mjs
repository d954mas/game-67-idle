import { strict as assert } from "node:assert";
import test from "node:test";
import { buildNineSliceDraws, clampInsets, clampNineSliceSize } from "../asset_tools_slice9.mjs";

test("buildNineSliceDraws maps source and destination patches", () => {
  const draws = buildNineSliceDraws({
    sourceWidth: 100,
    sourceHeight: 60,
    targetWidth: 220,
    targetHeight: 140,
    insets: { left: 10, right: 20, top: 8, bottom: 12 },
  });

  assert.equal(draws.length, 9);
  assert.deepEqual(draws[0], {
    key: "top-left",
    source: { x: 0, y: 0, width: 10, height: 8 },
    destination: { x: 0, y: 0, width: 10, height: 8 },
  });
  assert.deepEqual(draws[1], {
    key: "top",
    source: { x: 10, y: 0, width: 70, height: 8 },
    destination: { x: 10, y: 0, width: 190, height: 8 },
  });
  assert.deepEqual(draws[4], {
    key: "center",
    source: { x: 10, y: 8, width: 70, height: 40 },
    destination: { x: 10, y: 8, width: 190, height: 120 },
  });
  assert.deepEqual(draws[8], {
    key: "bottom-right",
    source: { x: 80, y: 48, width: 20, height: 12 },
    destination: { x: 200, y: 128, width: 20, height: 12 },
  });
});

test("buildNineSliceDraws can offset source patches into a selected region", () => {
  const draws = buildNineSliceDraws({
    sourceX: 30,
    sourceY: 20,
    sourceWidth: 40,
    sourceHeight: 24,
    targetWidth: 120,
    targetHeight: 72,
    insets: { left: 6, right: 8, top: 5, bottom: 7 },
  });

  assert.deepEqual(draws[0].source, { x: 30, y: 20, width: 6, height: 5 });
  assert.deepEqual(draws[4].source, { x: 36, y: 25, width: 26, height: 12 });
  assert.deepEqual(draws[8].source, { x: 62, y: 37, width: 8, height: 7 });
  assert.deepEqual(draws[4].destination, { x: 6, y: 5, width: 106, height: 60 });
});

test("clampInsets keeps a center pixel available on each axis", () => {
  assert.deepEqual(clampInsets({ left: 80, right: 80, top: 30, bottom: 50 }, { width: 100, height: 60 }), {
    left: 50,
    right: 49,
    top: 22,
    bottom: 37,
  });
});

test("clampNineSliceSize preserves fixed edges and a stretchable center", () => {
  assert.deepEqual(
    clampNineSliceSize({
      targetWidth: 20,
      targetHeight: 10,
      insets: { left: 12, right: 12, top: 8, bottom: 8 },
    }),
    {
      width: 25,
      height: 17,
    },
  );
});
