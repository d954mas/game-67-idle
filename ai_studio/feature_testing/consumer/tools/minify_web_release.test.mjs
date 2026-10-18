import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { minifyWebRelease } from "./minify_web_release.mjs";

test("release minifier compacts HTML while preserving executable bootstraps", async (t) => {
  const artifactDir = mkdtempSync(join(tmpdir(), "web-minify-"));
  t.after(() => rmSync(artifactDir, { recursive: true, force: true }));
  const source = [
    "<!doctype html>",
    "<!-- release comment -->",
    "<style> body { color: red; } </style>",
    "<script> window.__boot = true; </script>",
    "<script type=\"module\"> import \"./platform-sdk.js\"; </script>",
    "<script src=\"game.js\"></script>",
  ].join("\n");
  writeFileSync(join(artifactDir, "index.html"), source);
  let options;

  const result = await minifyWebRelease({
    artifactDir,
    minify: async (input, received) => {
      options = received;
      assert.equal(input, source);
      return '<!doctype html><style>body{color:red}</style><script>window.__boot=!0</script><script type="module">import"./platform-sdk.js";</script><script src="game.js"></script>';
    },
  });

  const output = readFileSync(join(artifactDir, "index.html"), "utf8");
  assert.ok(Buffer.byteLength(output) < Buffer.byteLength(source));
  assert.equal(output.includes("platform-sdk.js"), true);
  assert.equal(output.includes('src="game.js"'), true);
  assert.equal(options.collapseWhitespace, true);
  assert.equal(options.minifyCSS, true);
  assert.equal(options.minifyJS.compress.booleans, false);
  assert.equal(options.minifyJS.compress.sequences, false);
  assert.equal(options.minifyJS.mangle, true);
  assert.deepEqual(result, { beforeBytes: Buffer.byteLength(source), afterBytes: Buffer.byteLength(output) });
});
