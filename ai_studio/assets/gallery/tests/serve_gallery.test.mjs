import test from "node:test";
import assert from "node:assert/strict";
import { normalize, resolve } from "node:path";
import { parseArgs, resolveTarget } from "../serve_gallery.mjs";

const roots = {
  galleryRoot: resolve("tmp/test-gallery"),
  libRoot: resolve("tmp/test-library"),
};

test("parseArgs requires a library root and parses gallery/port", () => {
  assert.throws(() => parseArgs([]), /missing --lib/);
  assert.throws(() => parseArgs(["--lib"]), /missing value/);
  assert.throws(() => parseArgs(["--bad", "x", "--lib", "library"]), /unknown option/);

  assert.deepEqual(parseArgs(["--gallery", "tmp/gallery", "--lib", "library", "--port", "8911"]), {
    gallery: "tmp/gallery",
    lib: "library",
    port: 8911,
  });
});

test("resolveTarget maps gallery and library requests", () => {
  assert.deepEqual(resolveTarget("/", roots), {
    root: roots.galleryRoot,
    rel: "index.html",
    file: resolve(roots.galleryRoot, "index.html"),
  });

  assert.deepEqual(resolveTarget("/viewer.js?cache=1", roots), {
    root: roots.galleryRoot,
    rel: "viewer.js",
    file: resolve(roots.galleryRoot, "viewer.js"),
  });

  assert.deepEqual(resolveTarget("/lib/packs/kit/files/model.glb", roots), {
    root: roots.libRoot,
    rel: normalize("packs/kit/files/model.glb"),
    file: resolve(roots.libRoot, "packs/kit/files/model.glb"),
  });
});

test("resolveTarget rejects traversal and absolute path requests", () => {
  assert.equal(resolveTarget("/../AGENTS.md", roots), null);
  assert.equal(resolveTarget("/lib/../secret.txt", roots), null);
  assert.equal(resolveTarget("/lib/%2e%2e/secret.txt", roots), null);
  assert.equal(resolveTarget("/lib/%E0%A4%A", roots), null);
  assert.equal(resolveTarget("/C:/Windows/System32/drivers/etc/hosts", roots), null);
  assert.equal(resolveTarget("/lib/C:/Windows/System32/drivers/etc/hosts", roots), null);
});
