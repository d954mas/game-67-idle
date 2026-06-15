import assert from "node:assert/strict";
import test from "node:test";
import { spawnSync } from "node:child_process";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");

function run(args = []) {
  return spawnSync(process.execPath, ["tools/pipeline_validate.mjs", ...args], {
    cwd: root,
    encoding: "utf8",
    stdio: "pipe",
  });
}

test("pipeline validation defaults to quick dry-run without export checks", () => {
  const result = run(["--dry-run"]);
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /mode: quick \(dry-run\)/);
  assert.match(result.stdout, /== ai facade tests/);
  assert.match(result.stdout, /== product gate tests/);
  assert.doesNotMatch(result.stdout, /== portable export/);
  assert.doesNotMatch(result.stdout, /== exported ai profile tests/);
  assert.match(result.stdout, /reusable pipeline quick validation passed/);
});

test("pipeline validation full dry-run keeps portable export checks", () => {
  const result = run(["--full", "--dry-run"]);
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /mode: full \(dry-run\)/);
  assert.match(result.stdout, /== portable export/);
  assert.match(result.stdout, /== exported ai profile tests/);
  assert.match(result.stdout, /== exported taskboard validate/);
  assert.match(result.stdout, /reusable pipeline validation passed/);
});

test("pipeline validation rejects conflicting modes", () => {
  const result = run(["--quick", "--full", "--dry-run"]);
  assert.equal(result.status, 2);
  assert.match(result.stderr, /usage:/);
});

test("pipeline validation does not prune during dry-run", () => {
  const result = run(["--dry-run"]);
  assert.equal(result.status, 0, result.stderr);
  assert.doesNotMatch(result.stdout, /pruned .* old tmp/);
});

test("pipeline validation rejects a non-integer --keep-exports", () => {
  const result = run(["--keep-exports", "nope", "--dry-run"]);
  assert.equal(result.status, 2);
  assert.match(result.stderr, /usage:/);
});

test("pipeline validation accepts --keep-exports and --no-prune", () => {
  const result = run(["--keep-exports", "2", "--no-prune", "--dry-run"]);
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /mode: quick \(dry-run\)/);
});
