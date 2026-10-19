import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, mkdirSync, mkdtempSync, rmSync, symlinkSync } from "node:fs";
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

test("uploadImageSource rejects a raster2d namespace junction before writing a source", async () => {
  const root = mkdtempSync(join(tmpdir(), "image-sources-physical-root-"));
  const outside = mkdtempSync(join(tmpdir(), "image-sources-physical-outside-"));
  try {
    const namespaceParent = join(root, "tmp", "ai_studio", "assets");
    mkdirSync(namespaceParent, { recursive: true });
    symlinkSync(outside, join(namespaceParent, "raster2d"), process.platform === "win32" ? "junction" : "dir");

    await assert.rejects(
      () => uploadImageSource(root, {
        fileName: "escape.png",
        dataUrl: "data:image/png;base64,cG5n",
      }),
      /physical|tmp/i,
    );
    assert.equal(existsSync(join(outside, "escape.png")), false);
  } finally {
    rmSync(root, { recursive: true, force: true });
    rmSync(outside, { recursive: true, force: true });
  }
});

test("uploadImageSource accepts a repository-root alias when its namespace is physical", async () => {
  const physicalRoot = mkdtempSync(join(tmpdir(), "image-sources-physical-root-"));
  const alias = join(tmpdir(), `image-sources-root-alias-${process.pid}-${Date.now()}`);
  try {
    symlinkSync(physicalRoot, alias, process.platform === "win32" ? "junction" : "dir");
    const result = await uploadImageSource(alias, {
      fileName: "source.png",
      dataUrl: "data:image/png;base64,cG5n",
    });

    assert.equal(existsSync(join(alias, result.sourcePath)), true);
    assert.equal(existsSync(join(physicalRoot, result.sourcePath)), true);
  } finally {
    rmSync(alias, { recursive: true, force: true });
    rmSync(physicalRoot, { recursive: true, force: true });
  }
});
