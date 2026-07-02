import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

import { detectRaster2dRegions, exportRaster2dRegion, resolveRaster2dTmpPath, uploadRaster2dSource } from "./api.mjs";

function tempRoot() {
  const root = join(tmpdir(), `raster2d-api-${process.pid}-${Date.now()}-${Math.random().toString(16).slice(2)}`);
  mkdirSync(root, { recursive: true });
  return root;
}

test("uploadRaster2dSource writes uploaded images under raster2d tmp", async () => {
  const root = tempRoot();
  try {
    const dataUrl = `data:image/png;base64,${Buffer.from("png").toString("base64")}`;
    const result = await uploadRaster2dSource(root, {
      fileName: "../bad name.png",
      dataUrl,
    });

    assert.match(result.sourcePath, /^tmp\/ai_studio\/assets\/raster2d\//);
    assert.match(result.sourceUrl, /^\/tmp\/ai_studio\/assets\/raster2d\//);
    assert.equal(existsSync(join(root, result.sourcePath)), true);
    assert.equal(result.fileName, "bad_name.png");
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("resolveRaster2dTmpPath confines public tmp routes", () => {
  const root = tempRoot();
  try {
    assert.equal(
      resolveRaster2dTmpPath(root, "/tmp/ai_studio/assets/raster2d/session/file.png"),
      join(root, "tmp", "ai_studio", "assets", "raster2d", "session", "file.png"),
    );
    assert.equal(resolveRaster2dTmpPath(root, "/tmp/../AGENTS.md"), null);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("detectRaster2dRegions can preserve a whole image without background normalization", async () => {
  const root = process.cwd();
  let sessionId = "";
  try {
    const png1x1 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=";
    const uploaded = await uploadRaster2dSource(root, {
      fileName: "whole.png",
      dataUrl: `data:image/png;base64,${png1x1}`,
    });
    const result = await detectRaster2dRegions(root, {
      sourcePath: uploaded.sourcePath,
      options: { backgroundMode: "whole_image" },
    });
    sessionId = result.sessionId;

    assert.equal(result.normalizeReport.mode, "passthrough_no_background");
    assert.equal(result.regions.mode, "whole_image");
    assert.deepEqual(result.regions.regions[0].rect, [0, 0, 1, 1]);
    assert.equal(existsSync(join(root, result.normalizedPath)), true);
  } finally {
    if (sessionId) {
      rmSync(join(root, "tmp", "ai_studio", "assets", "raster2d", sessionId), { recursive: true, force: true });
    }
  }
});

test("exportRaster2dRegion returns a direct PNG slice for one selected region", async () => {
  const root = process.cwd();
  let sessionId = "";
  try {
    const png1x1 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=";
    const uploaded = await uploadRaster2dSource(root, {
      fileName: "single-source.png",
      dataUrl: `data:image/png;base64,${png1x1}`,
    });
    const result = await exportRaster2dRegion(root, {
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

test("exportRaster2dRegion supports the default key matte alpha path", async () => {
  const root = process.cwd();
  let sessionId = "";
  try {
    const png1x1 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=";
    const uploaded = await uploadRaster2dSource(root, {
      fileName: "key-source.png",
      dataUrl: `data:image/png;base64,${png1x1}`,
    });
    const result = await exportRaster2dRegion(root, {
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
    assert.match(result.manifest.slices[0].alpha.status, /^applied/);
    assert.equal(existsSync(join(root, result.slicePath)), true);
  } finally {
    if (sessionId) {
      rmSync(join(root, "tmp", "ai_studio", "assets", "raster2d", sessionId), { recursive: true, force: true });
    }
  }
});
