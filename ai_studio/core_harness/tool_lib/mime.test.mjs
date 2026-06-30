import assert from "node:assert/strict";
import test from "node:test";
import { MIME_TYPES, mimeType } from "./mime.mjs";

test("mimeType maps known extensions, case-insensitively", () => {
  assert.equal(mimeType("index.html"), "text/html; charset=utf-8");
  assert.equal(mimeType("app.js"), "text/javascript");
  assert.equal(mimeType("scene.GLB"), "model/gltf-binary");
  assert.equal(mimeType("/a/b/photo.JPEG"), "image/jpeg");
});

test("mimeType falls back to binary for unknown extensions", () => {
  assert.equal(mimeType("archive.zip"), "application/octet-stream");
  assert.equal(mimeType("noext"), "application/octet-stream");
  assert.equal(mimeType("weird.xyz", "text/plain"), "text/plain");
});

test("overlapping extensions both servers used map identically (no conflict)", () => {
  // These extensions appeared in BOTH serve_tunnel and serve_gallery before the
  // merge; the shared map must keep their (identical) types so neither server
  // changes behaviour for them.
  for (const [ext, type] of Object.entries({
    ".html": "text/html; charset=utf-8",
    ".js": "text/javascript",
    ".mjs": "text/javascript",
    ".css": "text/css",
    ".json": "application/json",
    ".png": "image/png",
    ".jpg": "image/jpeg",
    ".jpeg": "image/jpeg",
    ".gif": "image/gif",
    ".webp": "image/webp",
    ".svg": "image/svg+xml",
    ".ico": "image/x-icon",
    ".ttf": "font/ttf",
    ".glb": "model/gltf-binary",
    ".gltf": "model/gltf+json",
    ".wasm": "application/wasm",
  })) {
    assert.equal(MIME_TYPES[ext], type, `mismatch for ${ext}`);
  }
});
