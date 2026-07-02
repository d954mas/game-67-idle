import { strict as assert } from "node:assert";
import test from "node:test";
import {
  fitViewport,
  hitRectHandle,
  imageToScreenPoint,
  moveRect,
  resizeRectFromHandle,
  screenToImagePoint,
  zoomViewportAt,
} from "../asset_tools_viewport.mjs";

test("fitViewport centers an image inside the available stage", () => {
  const viewport = fitViewport({ imageWidth: 1000, imageHeight: 500, frameWidth: 500, frameHeight: 500 });

  assert.equal(viewport.scale, 0.5);
  assert.equal(viewport.offsetX, 0);
  assert.equal(viewport.offsetY, 125);
});

test("zoomViewportAt keeps the image point under the cursor stable", () => {
  const viewport = { scale: 1, offsetX: 10, offsetY: 20 };
  const cursor = { x: 110, y: 70 };
  const before = screenToImagePoint(cursor, viewport);
  const zoomed = zoomViewportAt(viewport, 2, cursor);
  const after = screenToImagePoint(cursor, zoomed);

  assert.deepEqual(after, before);
  assert.equal(zoomed.scale, 2);
});

test("imageToScreenPoint and screenToImagePoint round-trip", () => {
  const viewport = { scale: 1.5, offsetX: 20, offsetY: 30 };
  const imagePoint = { x: 50, y: 40 };
  const screenPoint = imageToScreenPoint(imagePoint, viewport);

  assert.deepEqual(screenToImagePoint(screenPoint, viewport), imagePoint);
});

test("hitRectHandle detects corners before body", () => {
  const rect = { x: 10, y: 20, width: 100, height: 80 };

  assert.equal(hitRectHandle({ x: 10, y: 20 }, rect, 6), "nw");
  assert.equal(hitRectHandle({ x: 110, y: 100 }, rect, 6), "se");
  assert.equal(hitRectHandle({ x: 60, y: 60 }, rect, 6), "body");
  assert.equal(hitRectHandle({ x: 5, y: 5 }, rect, 6), "");
});

test("moveRect and resizeRectFromHandle clamp to image bounds", () => {
  const image = { width: 120, height: 100 };
  assert.deepEqual(moveRect({ x: 10, y: 10, width: 30, height: 20 }, 100, 100, image), {
    x: 90,
    y: 80,
    width: 30,
    height: 20,
  });

  assert.deepEqual(resizeRectFromHandle({ x: 10, y: 10, width: 30, height: 20 }, "nw", { x: -5, y: -7 }, image), {
    x: 0,
    y: 0,
    width: 40,
    height: 30,
  });
});
