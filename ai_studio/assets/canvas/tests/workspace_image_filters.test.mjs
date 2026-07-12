import assert from "node:assert/strict";
import test from "node:test";

import {
  clearImageFilterCache,
  cssFilterFor,
  imageFilterCacheSize,
  tintedOffscreenFor,
} from "../site/workspace/image_filters.js";

function fakeDocument(created) {
  return {
    createElement(tag) {
      assert.equal(tag, "canvas");
      const operations = [];
      const context = {
        filter: "none",
        globalAlpha: 1,
        globalCompositeOperation: "source-over",
        drawImage(...args) { operations.push(["drawImage", ...args]); },
        fillRect(...args) { operations.push(["fillRect", ...args]); },
      };
      const canvas = { operations, getContext: () => context };
      created.push(canvas);
      return canvas;
    },
  };
}

test("image filter CSS omits identity values and preserves explicit transforms", () => {
  assert.equal(cssFilterFor(null), "");
  assert.equal(cssFilterFor({ brightness: 1, saturation: 1, contrast: 1 }), "");
  assert.equal(
    cssFilterFor({ brightness: 1.2, saturation: 0.5, contrast: 0.9 }),
    "brightness(1.2) saturate(0.5) contrast(0.9)",
  );
});

test("tinted offscreens cache by source and filter signature and apply tint", () => {
  const previousDocument = globalThis.document;
  const created = [];
  globalThis.document = fakeDocument(created);
  clearImageFilterCache();
  try {
    const image = { src: "hero.png", naturalWidth: 16, naturalHeight: 12 };
    const filters = { brightness: 0.8, tint: { color: "#ff00ff", strength: 0.25 } };
    const first = tintedOffscreenFor(image, filters);
    const second = tintedOffscreenFor(image, filters);
    assert.equal(first, second);
    assert.equal(first.width, 16);
    assert.equal(first.height, 12);
    assert.deepEqual(first.operations.map(([name]) => name), ["drawImage", "fillRect"]);
    assert.equal(created.length, 1);
    assert.equal(imageFilterCacheSize(), 1);
  } finally {
    clearImageFilterCache();
    globalThis.document = previousDocument;
  }
});

test("image filter cache evicts the oldest entry at its fixed bound", () => {
  const previousDocument = globalThis.document;
  const created = [];
  globalThis.document = fakeDocument(created);
  clearImageFilterCache();
  try {
    const filters = { tint: { color: "#fff", strength: 0.1 } };
    const firstImage = { src: "frame-0.png", naturalWidth: 1, naturalHeight: 1 };
    const first = tintedOffscreenFor(firstImage, filters);
    for (let index = 1; index <= 96; index += 1) {
      tintedOffscreenFor({ src: `frame-${index}.png`, naturalWidth: 1, naturalHeight: 1 }, filters);
    }
    assert.equal(imageFilterCacheSize(), 96);
    assert.notEqual(tintedOffscreenFor(firstImage, filters), first, "oldest entry must be evicted");
    assert.equal(imageFilterCacheSize(), 96);
  } finally {
    clearImageFilterCache();
    globalThis.document = previousDocument;
  }
});
