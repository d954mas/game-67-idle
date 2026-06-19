import assert from "node:assert/strict";
import test from "node:test";
import { spawnSync } from "node:child_process";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");

function run(args = [], env = {}) {
  return spawnSync(process.execPath, ["tools/pipeline_validate.mjs", ...args], {
    cwd: root,
    env: { ...process.env, ...env },
    encoding: "utf8",
    stdio: "pipe",
  });
}

test("pipeline validation defaults to quick dry-run without export checks", () => {
  const result = run(["--dry-run"]);
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /mode: quick \(dry-run\)/);
  assert.match(result.stdout, /== ai facade tests/);
  assert.match(result.stdout, /== context budget report/);
  assert.match(result.stdout, /== context budget tests/);
  assert.match(result.stdout, /== repeated product gate failure guard/);
  assert.match(result.stdout, /== product gate tests/);
  assert.doesNotMatch(result.stdout, /== portable export/);
  assert.doesNotMatch(result.stdout, /== exported ai profile tests/);
  assert.match(result.stdout, /reusable pipeline quick validation passed/);
});

test("pipeline validation full dry-run runs the minimal export check by default", () => {
  const result = run(["--full", "--dry-run"]);
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /mode: full \(dry-run\)/);
  assert.match(result.stdout, /== portable export/);
  assert.match(result.stdout, /== exported skill eval/);
  assert.match(result.stdout, /== exported taskboard validate/);
  // Default --full skips the redundant in-export test battery.
  assert.doesNotMatch(result.stdout, /== exported ai profile tests/);
  assert.match(result.stdout, /skipped the in-export test battery/);
  assert.match(result.stdout, /reusable pipeline validation passed/);
});

test("pipeline validation full --reexport-tests dry-run runs the full in-export battery", () => {
  const result = run(["--full", "--reexport-tests", "--dry-run"]);
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /mode: full \(dry-run\)/);
  assert.match(result.stdout, /== exported ai profile tests/);
  assert.doesNotMatch(result.stdout, /skipped the in-export test battery/);
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

test("pipeline validation dry-run shows configured Python command with args", () => {
  const result = run(["--full", "--dry-run"], {
    AI_PIPELINE_PYTHON: "uv run python",
  });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /python runner: <dry-run> uv run python/);
  assert.match(result.stdout, /\$ uv run python -m unittest/);
});
