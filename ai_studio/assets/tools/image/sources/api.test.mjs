import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

import { uploadImageSource } from "./api.mjs";

function tempRoot() {
  const root = join(tmpdir(), `image-sources-${process.pid}-${Date.now()}-${Math.random().toString(16).slice(2)}`);
  mkdirSync(root, { recursive: true });
  return root;
}

test("uploadImageSource writes uploaded images under the raster2d tmp prefix", async () => {
  const root = tempRoot();
  try {
    const dataUrl = `data:image/png;base64,${Buffer.from("png").toString("base64")}`;
    const result = await uploadImageSource(root, {
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
