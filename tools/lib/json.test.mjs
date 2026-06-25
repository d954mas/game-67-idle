import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, writeFileSync, readFileSync, existsSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { readJson, writeJsonFile } from "./json.mjs";

function tmp(t) {
  const dir = mkdtempSync(join(tmpdir(), "json-lib-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

test("readJson parses a JSON file", (t) => {
  const dir = tmp(t);
  const f = join(dir, "a.json");
  writeFileSync(f, '{"x":1}', "utf8");
  assert.deepEqual(readJson(f), { x: 1 });
});

test("readJson calls onError with a message on parse failure", (t) => {
  const dir = tmp(t);
  const f = join(dir, "bad.json");
  writeFileSync(f, "{nope", "utf8");
  let msg = null;
  const r = readJson(f, (m) => {
    msg = m;
    return "SENTINEL";
  });
  assert.equal(r, "SENTINEL");
  assert.match(msg, /cannot read JSON .*bad\.json/);
});

test("readJson rethrows when no onError is given", (t) => {
  const dir = tmp(t);
  assert.throws(() => readJson(join(dir, "missing.json")));
});

test("writeJsonFile writes pretty JSON + trailing newline under root", (t) => {
  const dir = tmp(t);
  writeJsonFile("sub/a.json", { x: 1 }, { root: dir });
  assert.equal(readFileSync(join(dir, "sub/a.json"), "utf8"), `${JSON.stringify({ x: 1 }, null, 2)}\n`);
});

test("writeJsonFile refuses to overwrite via onError and leaves the file intact", (t) => {
  const dir = tmp(t);
  writeFileSync(join(dir, "a.json"), "{}", "utf8");
  let msg = null;
  writeJsonFile("a.json", { x: 1 }, { root: dir, onError: (m) => { msg = m; } });
  assert.match(msg, /refusing to overwrite/);
  assert.equal(readFileSync(join(dir, "a.json"), "utf8"), "{}");
});

test("writeJsonFile dryRun does not touch disk", (t) => {
  const dir = tmp(t);
  writeJsonFile("a.json", { x: 1 }, { root: dir, dryRun: true });
  assert.equal(existsSync(join(dir, "a.json")), false);
});
