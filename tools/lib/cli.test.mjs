import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, writeFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";

const here = dirname(fileURLToPath(import.meta.url));
const cliUrl = pathToFileURL(join(here, "cli.mjs")).href;

function tmp(t) {
  const dir = mkdtempSync(join(tmpdir(), "cli-lib-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function runScript(dir, name, body) {
  const file = join(dir, name);
  writeFileSync(file, body, "utf8");
  return spawnSync(process.execPath, [file], { encoding: "utf8" });
}

test("fail prints 'error: <message>' to stderr and exits 1", (t) => {
  const dir = tmp(t);
  const r = runScript(
    dir,
    "boom.mjs",
    `import { fail } from ${JSON.stringify(cliUrl)};\nfail("boom");\nconsole.log("unreachable");\n`
  );
  assert.equal(r.status, 1);
  assert.equal(r.stderr, "error: boom\n");
  assert.doesNotMatch(r.stdout, /unreachable/);
});

test("isMain is true for the entry script", (t) => {
  const dir = tmp(t);
  const r = runScript(
    dir,
    "entry.mjs",
    `import { isMain } from ${JSON.stringify(cliUrl)};\nconsole.log(isMain(import.meta.url) ? "MAIN" : "NOT");\n`
  );
  assert.equal(r.status, 0, r.stderr);
  assert.equal(r.stdout.trim(), "MAIN");
});

test("isMain is false when the module is imported, not the entry", (t) => {
  const dir = tmp(t);
  const entry = join(dir, "entry.mjs");
  writeFileSync(
    entry,
    `import { isMain } from ${JSON.stringify(cliUrl)};\nconsole.log(isMain(import.meta.url) ? "MAIN" : "NOT");\n`,
    "utf8"
  );
  const r = runScript(dir, "importer.mjs", `import ${JSON.stringify(pathToFileURL(entry).href)};\n`);
  assert.equal(r.status, 0, r.stderr);
  assert.equal(r.stdout.trim(), "NOT");
});
