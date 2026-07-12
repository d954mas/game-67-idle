import assert from "node:assert/strict";
import test from "node:test";

import { createCleanupPreviewController } from "../site/workspace/cleanup_preview.js";

test("cleanup preview transitions compare state and repaints only for real changes", () => {
  let repaints = 0;
  const controller = createCleanupPreviewController(() => { repaints += 1; });
  const preview = { elementId: "hero", bitmap: {} };

  controller.setComparing(true);
  assert.equal(repaints, 0);
  controller.setPreview(preview);
  controller.setComparing(true);
  controller.setComparing(true);
  assert.equal(controller.getPreview(), preview);
  assert.equal(controller.isComparing(), true);
  assert.equal(repaints, 2);

  controller.clearPreview();
  controller.clearPreview();
  assert.equal(controller.getPreview(), null);
  assert.equal(controller.isComparing(), false);
  assert.equal(repaints, 3);
});

test("cleanup bitmap loader resolves and rejects through the Image boundary", async () => {
  const PreviousImage = globalThis.Image;
  const created = [];
  globalThis.Image = class FakeImage {
    constructor() { created.push(this); }
    set src(value) {
      this.value = value;
      queueMicrotask(() => value.endsWith("good") ? this.onload() : this.onerror());
    }
  };

  try {
    const controller = createCleanupPreviewController(() => {});
    const loaded = controller.loadBitmap("good");
    assert.equal((await loaded).value, "data:image/png;base64,good");
    await assert.rejects(controller.loadBitmap("bad"), /could not decode/);
    assert.equal(created.length, 2);
  } finally {
    globalThis.Image = PreviousImage;
  }
});
