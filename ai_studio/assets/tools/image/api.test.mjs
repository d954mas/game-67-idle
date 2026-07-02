import assert from "node:assert/strict";
import test from "node:test";
import { existsSync, mkdirSync, rmSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { Readable } from "node:stream";

import { createImageAssetToolsApi, resolveImageTmpPath } from "./api.mjs";

function tempRoot() {
  const root = join(tmpdir(), `image-api-${process.pid}-${Date.now()}-${Math.random().toString(16).slice(2)}`);
  mkdirSync(root, { recursive: true });
  return root;
}

// Drive the composed handler with a mock req/res so the public endpoint routing
// (the frozen /api/asset-tools/raster2d/* URLs) is covered at the JS layer.
function callHandler(handler, method, pathname, body) {
  const req = Readable.from([JSON.stringify(body ?? {})]);
  req.method = method;
  const url = new URL(`http://localhost${pathname}`);
  return new Promise((resolveCall, rejectCall) => {
    const res = {
      status: 0,
      payload: "",
      writeHead(status) {
        this.status = status;
      },
      end(data) {
        this.payload = data || "";
        resolveCall({ handled: true, status: this.status, json: this.payload ? JSON.parse(this.payload) : null });
      },
    };
    Promise.resolve(handler(req, res, url))
      .then((handled) => {
        if (!handled) resolveCall({ handled: false });
      })
      .catch(rejectCall);
  });
}

test("resolveImageTmpPath confines public tmp routes", () => {
  const root = tempRoot();
  try {
    assert.equal(
      resolveImageTmpPath(root, "/tmp/ai_studio/assets/raster2d/session/file.png"),
      join(root, "tmp", "ai_studio", "assets", "raster2d", "session", "file.png"),
    );
    assert.equal(resolveImageTmpPath(root, "/tmp/../AGENTS.md"), null);
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
});

test("createImageAssetToolsApi routes the frozen raster2d endpoint URLs", async () => {
  const root = process.cwd();
  const handler = createImageAssetToolsApi(root);
  let sessionId = "";
  try {
    const png1x1 = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+/p9sAAAAASUVORK5CYII=";
    const upload = await callHandler(handler, "POST", "/api/asset-tools/raster2d/upload", {
      fileName: "handler.png",
      dataUrl: `data:image/png;base64,${png1x1}`,
    });
    assert.equal(upload.handled, true);
    assert.equal(upload.status, 200);
    assert.match(upload.json.sourcePath, /^tmp\/ai_studio\/assets\/raster2d\//);

    const detect = await callHandler(handler, "POST", "/api/asset-tools/raster2d/detect", {
      sourcePath: upload.json.sourcePath,
      options: { backgroundMode: "whole_image" },
    });
    assert.equal(detect.handled, true);
    assert.equal(detect.status, 200);
    assert.equal(detect.json.regions.mode, "whole_image");
    sessionId = detect.json.sessionId;

    const unknown = await callHandler(handler, "GET", "/api/asset-tools/raster2d/upload", {});
    assert.equal(unknown.handled, false);
  } finally {
    if (sessionId) {
      rmSync(join(root, "tmp", "ai_studio", "assets", "raster2d", sessionId), { recursive: true, force: true });
    }
  }
});
