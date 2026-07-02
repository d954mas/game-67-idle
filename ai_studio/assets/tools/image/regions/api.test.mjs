import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, rmSync } from "node:fs";
import { join } from "node:path";

import { uploadImageSource } from "../sources/api.mjs";
import { detectImageRegions } from "./api.mjs";

test("detectImageRegions can preserve a whole image without background normalization", async () => {
  const root = process.cwd();
  let sessionId = "";
  try {
    const png1x1 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=";
    const uploaded = await uploadImageSource(root, {
      fileName: "whole.png",
      dataUrl: `data:image/png;base64,${png1x1}`,
    });
    const result = await detectImageRegions(root, {
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
