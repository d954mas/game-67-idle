import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, writeFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { loadArtContract } from "./art_contract.mjs";

function tmp(t) {
  const dir = mkdtempSync(join(tmpdir(), "art-contract-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

test("loadArtContract returns the parsed contract", (t) => {
  const dir = tmp(t);
  const f = join(dir, "art_contract.json");
  writeFileSync(f, '{"pass_threshold":4}', "utf8");
  assert.deepEqual(loadArtContract(f), { pass_threshold: 4 });
});

test("loadArtContract returns null when absent and not required (no onError)", (t) => {
  const dir = tmp(t);
  let called = false;
  const r = loadArtContract(join(dir, "missing.json"), { onError: () => { called = true; } });
  assert.equal(r, null);
  assert.equal(called, false);
});

test("loadArtContract calls onError when absent and required", (t) => {
  const dir = tmp(t);
  let msg = null;
  loadArtContract(join(dir, "missing.json"), { required: true, onError: (m) => { msg = m; } });
  assert.match(msg, /art contract does not exist/);
});

test("loadArtContract calls onError on invalid JSON", (t) => {
  const dir = tmp(t);
  const f = join(dir, "bad.json");
  writeFileSync(f, "{nope", "utf8");
  let msg = null;
  loadArtContract(f, { onError: (m) => { msg = m; } });
  assert.match(msg, /art contract is not valid JSON/);
});
