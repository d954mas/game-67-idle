import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, mkdirSync, mkdtempSync, rmSync, symlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";

import { uploadImageSource } from "../sources/api.mjs";
import { ensureInsideTmp } from "../_bridge/bridge.mjs";
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
test("detectImageRegions rejects a raster2d namespace junction before writing outputs", async () => {
  const root = mkdtempSync(join(tmpdir(), "image-regions-physical-root-"));
  const outside = mkdtempSync(join(tmpdir(), "image-regions-physical-outside-"));
  try {
    const namespaceParent = join(root, "tmp", "ai_studio", "assets");
    const session = join(outside, "session", "sources");
    mkdirSync(namespaceParent, { recursive: true });
    mkdirSync(session, { recursive: true });
    writeFileSync(
      join(session, "escape.png"),
      Buffer.from("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=", "base64"),
    );
    symlinkSync(outside, join(namespaceParent, "raster2d"), process.platform === "win32" ? "junction" : "dir");

    await assert.rejects(
      () => detectImageRegions(root, {
        sourcePath: "tmp/ai_studio/assets/raster2d/session/sources/escape.png",
        options: { backgroundMode: "whole_image" },
      }),
      /physical|path|tmp/i,
    );
    assert.equal(existsSync(join(outside, "session", "background", "normalized.png")), false);
  } finally {
    rmSync(root, { recursive: true, force: true });
    rmSync(outside, { recursive: true, force: true });
  }
});
test("detectImageRegions rejects a session junction before writing outputs", async () => {
  const root = mkdtempSync(join(tmpdir(), "image-session-physical-root-"));
  const outside = mkdtempSync(join(tmpdir(), "image-session-physical-outside-"));
  try {
    const namespace = join(root, "tmp", "ai_studio", "assets", "raster2d");
    const sourceDir = join(outside, "sources");
    mkdirSync(namespace, { recursive: true });
    mkdirSync(sourceDir, { recursive: true });
    writeFileSync(
      join(sourceDir, "escape.png"),
      Buffer.from("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=", "base64"),
    );
    symlinkSync(outside, join(namespace, "session"), process.platform === "win32" ? "junction" : "dir");

    await assert.rejects(
      () => detectImageRegions(root, {
        sourcePath: "tmp/ai_studio/assets/raster2d/session/sources/escape.png",
        options: { backgroundMode: "whole_image" },
      }),
      /physical|path|tmp/i,
    );
    assert.equal(existsSync(join(outside, "background", "normalized.png")), false);
  } finally {
    rmSync(root, { recursive: true, force: true });
    rmSync(outside, { recursive: true, force: true });
  }
});

for (const outputDir of ["background", "regions"]) {
  test(`detectImageRegions rejects a ${outputDir} junction before writing outputs`, async () => {
    const root = mkdtempSync(join(tmpdir(), "image-output-physical-root-"));
    const outside = mkdtempSync(join(tmpdir(), "image-output-physical-outside-"));
    try {
      const session = join(root, "tmp", "ai_studio", "assets", "raster2d", "session");
      const sourceDir = join(session, "sources");
      mkdirSync(sourceDir, { recursive: true });
      writeFileSync(
        join(sourceDir, "source.png"),
        Buffer.from("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=", "base64"),
      );
      symlinkSync(outside, join(session, outputDir), process.platform === "win32" ? "junction" : "dir");

      await assert.rejects(
        () => detectImageRegions(root, {
          sourcePath: "tmp/ai_studio/assets/raster2d/session/sources/source.png",
          options: { backgroundMode: "whole_image" },
        }),
        /physical|path|tmp/i,
      );
      assert.equal(existsSync(join(outside, "normalized.png")), false);
      assert.equal(existsSync(join(outside, "regions.json")), false);
    } finally {
      rmSync(root, { recursive: true, force: true });
      rmSync(outside, { recursive: true, force: true });
    }
  });
}

for (const [folder, leaf] of [["background", "normalized.png"], ["background", "normalize_report.json"], ["regions", "regions.json"], ["regions", "overlay.png"]]) {
  test(`ensureInsideTmp rejects a dangling junction at ${folder}/${leaf}`, () => {
    const root = mkdtempSync(join(tmpdir(), "image-output-dangling-root-"));
    const outside = mkdtempSync(join(tmpdir(), "image-output-dangling-outside-"));
    try {
      const outputDir = join(root, "tmp", "ai_studio", "assets", "raster2d", "session", folder);
      mkdirSync(outputDir, { recursive: true });
      const outputPath = join(outputDir, leaf);
      symlinkSync(outside, outputPath, process.platform === "win32" ? "junction" : "dir");
      rmSync(outside, { recursive: true, force: true });

      assert.throws(
        () => ensureInsideTmp(root, outputPath),
        /cannot be verified|physical|path|tmp/i,
      );
    } finally {
      rmSync(root, { recursive: true, force: true });
      rmSync(outside, { recursive: true, force: true });
    }
  });
}
