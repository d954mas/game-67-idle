import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, rmSync } from "node:fs";
import { join } from "node:path";

import { uploadImageSource } from "../sources/api.mjs";
import { exportImageRegion } from "./api.mjs";

const png1x1 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=";

test("exportImageRegion returns a direct PNG slice for one selected region", async () => {
  const root = process.cwd();
  let sessionId = "";
  try {
    const uploaded = await uploadImageSource(root, {
      fileName: "single-source.png",
      dataUrl: `data:image/png;base64,${png1x1}`,
    });
    const result = await exportImageRegion(root, {
      imagePath: uploaded.sourcePath,
      prefix: "single",
      region: {
        id: "chosen",
        name: "Icon",
        rect: [0, 0, 1, 1],
        content_bbox: [0, 0, 1, 1],
        alpha: { mode: "generation" },
      },
    });
    sessionId = result.sessionId;

    assert.equal(result.manifest.slice_count, 1);
    assert.equal(result.fileName, "single_Icon.png");
    assert.match(result.sliceUrl, /^\/tmp\/ai_studio\/assets\/raster2d\/.+\/single\/slices\/single_Icon\.png$/);
    assert.equal(existsSync(join(root, result.slicePath)), true);
  } finally {
    if (sessionId) {
      rmSync(join(root, "tmp", "ai_studio", "assets", "raster2d", sessionId), { recursive: true, force: true });
    }
  }
});

test("exportImageRegion applies the default key matte alpha path", async () => {
  const root = process.cwd();
  let sessionId = "";
  try {
    const uploaded = await uploadImageSource(root, {
      fileName: "key-source.png",
      dataUrl: `data:image/png;base64,${png1x1}`,
    });
    const result = await exportImageRegion(root, {
      imagePath: uploaded.sourcePath,
      prefix: "key",
      region: {
        id: "default_alpha",
        rect: [0, 0, 1, 1],
        content_bbox: [0, 0, 1, 1],
      },
    });
    sessionId = result.sessionId;

    assert.equal(result.manifest.slice_count, 1);
    assert.equal(result.manifest.slices[0].alpha.mode, "key_matte");
    assert.equal(result.manifest.slices[0].alpha.status, "applied");
    assert.equal(existsSync(join(root, result.slicePath)), true);
  } finally {
    if (sessionId) {
      rmSync(join(root, "tmp", "ai_studio", "assets", "raster2d", sessionId), { recursive: true, force: true });
    }
  }
});
