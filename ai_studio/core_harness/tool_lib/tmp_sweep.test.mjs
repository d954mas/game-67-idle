import assert from "node:assert/strict";
import test from "node:test";
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, existsSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", "..");

function run(args) {
  return spawnSync(process.execPath, ["ai_studio/core_harness/tool_lib/tmp_sweep.mjs", ...args], {
    cwd: repoRoot,
    encoding: "utf8",
    stdio: "pipe",
  });
}

function makeFakeTmp() {
  const dir = mkdtempSync(join(tmpdir(), "tmp-sweep-"));
  const tmp = join(dir, "tmp");
  for (const name of [
    "rune_marches",
    "NanoAlpha",
    "pipeline-validate-old",
  ]) {
    mkdirSync(join(tmp, name), { recursive: true });
    writeFileSync(join(tmp, name, "f.txt"), "x", "utf8");
  }
  return { dir, tmp };
}

test("tmp_sweep --list reports without deleting", () => {
  const { dir, tmp } = makeFakeTmp();
  try {
    const result = run(["--list", "--root", dir]);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /reclaimable:/);
    // Nothing deleted on a list.
    assert.equal(existsSync(join(tmp, "rune_marches")), true);
    assert.equal(existsSync(join(tmp, "NanoAlpha")), true);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("tmp_sweep --all-scratch removes scratch entries", () => {
  const { dir, tmp } = makeFakeTmp();
  try {
    const result = run(["--all-scratch", "--root", dir]);
    assert.equal(result.status, 0, result.stderr);
    assert.equal(existsSync(join(tmp, "rune_marches")), false);
    assert.equal(existsSync(join(tmp, "NanoAlpha")), false);
    assert.equal(existsSync(join(tmp, "pipeline-validate-old")), false);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("tmp_sweep --all-scratch --dry-run deletes nothing", () => {
  const { dir, tmp } = makeFakeTmp();
  try {
    const result = run(["--all-scratch", "--dry-run", "--root", dir]);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /would free/);
    assert.equal(existsSync(join(tmp, "rune_marches")), true);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("tmp_sweep rejects unknown args", () => {
  const result = run(["--nuke-everything"]);
  assert.equal(result.status, 2);
});
