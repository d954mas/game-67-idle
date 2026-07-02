import { strict as assert } from "node:assert";
import test from "node:test";
import {
  fitViewport,
  imageToScreenPoint,
  screenToImagePoint,
  zoomViewportAt,
} from "../site/viewport.mjs";

test("fitViewport centers an image inside the available stage", () => {
  const viewport = fitViewport({ imageWidth: 1000, imageHeight: 500, frameWidth: 500, frameHeight: 500 });

  assert.equal(viewport.scale, 0.5);
  assert.equal(viewport.offsetX, 0);
  assert.equal(viewport.offsetY, 125);
});

test("fitViewport returns identity when a dimension is missing", () => {
  assert.deepEqual(
    fitViewport({ imageWidth: 0, imageHeight: 500, frameWidth: 500, frameHeight: 500 }),
    { scale: 1, offsetX: 0, offsetY: 0 },
  );
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
