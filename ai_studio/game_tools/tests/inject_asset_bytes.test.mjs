import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import test from "node:test";

import { main } from "../inject_asset_bytes.mjs";

function write(path, bytes) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, bytes);
}

test("the shell learns the staged download sizes, in the default or a named layout", (t) => {
  const root = mkdtempSync(join(tmpdir(), "inject-asset-bytes-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const shell = "<script>window.__gameAssetBytes = { wasm: 0, pack: 0 };</script>";

  const web = join(root, "bin");
  write(join(web, "index.html"), shell);
  write(join(web, "game.wasm"), Buffer.alloc(3));
  write(join(web, "assets", "game.ntpack"), Buffer.alloc(5));
  assert.match(main(["--artifact", web]), /game\.wasm 3 B, assets\/game\.ntpack 5 B/);
  assert.match(readFileSync(join(web, "index.html"), "utf8"), /\{ wasm: 3, pack: 5 \}/);

  const site = join(root, "site");
  write(join(site, "index.html"), shell);
  write(join(site, "client.wasm"), Buffer.alloc(7));
  write(join(site, "packs", "boot.ntpack"), Buffer.alloc(11));
  main(["--artifact", site, "--wasm", "client.wasm", "--pack", "packs/boot.ntpack"]);
  assert.match(readFileSync(join(site, "index.html"), "utf8"), /\{ wasm: 7, pack: 11 \}/);

  assert.throws(() => main([]), /usage/);
});
