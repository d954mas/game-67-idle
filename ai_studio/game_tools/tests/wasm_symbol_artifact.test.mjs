import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, readdirSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import test from "node:test";

import { publishArtifactSet, readWasmSymbols, retainWasmSymbols } from "../lib/wasm_symbol_artifact.mjs";
import { loadWebPackageLayout } from "../lib/release_package.mjs";

function fixture(t) {
  const root = mkdtempSync(join(tmpdir(), "wasm-symbols-"));
  t.after(() => rmSync(root, { recursive: true, force: true }));
  const wasmPath = join(root, "game.wasm");
  const emittedPath = join(root, "emitted.symbols");
  const symbolPath = join(root, "game.js.symbols");
  const digestPath = join(root, "game.js.symbols.wasm.sha256");
  const wasm = Buffer.from("linked wasm");
  writeFileSync(wasmPath, wasm);
  writeFileSync(emittedPath, "0:main\n");
  return { root, wasmPath, emittedPath, symbolPath, digestPath, wasm };
}

test("retained symbols are bound to the exact linked WASM", (t) => {
  const item = fixture(t);
  assert.equal(retainWasmSymbols({ ...item, retainedPath: item.symbolPath }), true);
  assert.equal(readWasmSymbols({ ...item, linkedWasmPath: item.wasmPath, packagedWasm: item.wasm }).toString(), "0:main\n");
  assert.throws(() => readWasmSymbols({ ...item, linkedWasmPath: item.wasmPath, packagedWasm: Buffer.from("different") }), /exact linked WASM/i);
  writeFileSync(item.wasmPath, "different");
  assert.throws(() => readWasmSymbols({ ...item, linkedWasmPath: item.wasmPath, packagedWasm: Buffer.from("different") }), /digest/i);
});

test("symbol retention recovers a digest interrupted after the map move", (t) => {
  const item = fixture(t);
  retainWasmSymbols({ ...item, retainedPath: item.symbolPath });
  renameSync(item.digestPath, `${item.digestPath}.pending`);
  assert.equal(retainWasmSymbols({ ...item, retainedPath: item.symbolPath }), true);
  assert.equal(readWasmSymbols({ ...item, linkedWasmPath: item.wasmPath, packagedWasm: item.wasm }).toString(), "0:main\n");
  writeFileSync(item.symbolPath, "0:other\n");
  assert.throws(() => readWasmSymbols({ ...item, linkedWasmPath: item.wasmPath, packagedWasm: item.wasm }), /digest/i);
});

test("failed publication removes only new incomplete artifacts", (t) => {
  const item = fixture(t);
  const files = ["manifest", "symbols", "zip"].map((name) => {
    const bytes = Buffer.from(name);
    const tempPath = join(item.root, `${name}.tmp`);
    writeFileSync(tempPath, bytes);
    return { tempPath, finalPath: join(item.root, name), bytes };
  });
  let count = 0;
  assert.throws(() => publishArtifactSet(files, (tempPath, finalPath) => {
    if (++count === 3) throw new Error("publish failed");
    writeFileSync(finalPath, readFileSync(tempPath));
  }), /publish failed/);
  assert.deepEqual(readdirSync(item.root).filter((name) => ["manifest", "symbols", "zip"].includes(name)), []);
});

test("publication recovers a lock left by a dead process", (t) => {
  const item = fixture(t);
  const tempPath = join(item.root, "zip.tmp");
  const finalPath = join(item.root, "zip");
  const bytes = Buffer.from("zip");
  writeFileSync(tempPath, bytes);
  const lockPath = `${finalPath}.publish.lock`;
  mkdirSync(lockPath);
  writeFileSync(join(lockPath, "owner.json"), JSON.stringify({ pid: 2147483647, createdAt: Date.now() }));
  publishArtifactSet([{ tempPath, finalPath, bytes }]);
  assert.deepEqual(readFileSync(finalPath), bytes);
  assert.equal(readdirSync(item.root).includes("zip.publish.lock"), false);
});

test("game package layout config selects files without allowing path escapes", (t) => {
  const item = fixture(t);
  const standard = loadWebPackageLayout(item.root);
  assert.equal(standard.clientWasm, "game.wasm");
  const releaseDir = join(item.root, "release");
  mkdirSync(releaseDir);
  const path = join(releaseDir, "web-package.json");
  writeFileSync(path, JSON.stringify({ ...standard, clientWasm: "plw_client.wasm" }));
  assert.equal(loadWebPackageLayout(item.root).clientWasm, "plw_client.wasm");
  writeFileSync(path, JSON.stringify({ ...standard, symbolMap: "../other.symbols" }));
  assert.throws(() => loadWebPackageLayout(item.root), /invalid web package symbolMap/i);
});
