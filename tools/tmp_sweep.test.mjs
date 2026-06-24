import assert from "node:assert/strict";
import test from "node:test";
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, existsSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { VALIDATE_EXPORT_PREFIX } from "./lib/tmp_exports.mjs";

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const P = VALIDATE_EXPORT_PREFIX;

function run(args) {
  return spawnSync(process.execPath, ["tools/tmp_sweep.mjs", ...args], {
    cwd: repoRoot,
    encoding: "utf8",
    stdio: "pipe",
  });
}

function makeFakeTmp() {
  const dir = mkdtempSync(join(tmpdir(), "tmp-sweep-"));
  const tmp = join(dir, "tmp");
  for (const name of [
    `${P}2026-06-15T01-00-00-000Z`,
    `${P}2026-06-15T02-00-00-000Z`,
    `${P}2026-06-15T03-00-00-000Z`,
    `${P}2026-06-15T04-00-00-000Z`,
    "rune_marches",
    "NanoAlpha",
  ]) {
    mkdirSync(join(tmp, name), { recursive: true });
    writeFileSync(join(tmp, name, "f.txt"), "x", "utf8");
  }
  return { dir, tmp };
}

test("tmp_sweep --list reports without deleting", () => {
  const { dir, tmp } = makeFakeTmp();
  try {
    const result = run(["--list", "--root", dir, "--keep-validate", "3"]);
    assert.equal(result.status, 0, result.stderr);
    assert.match(result.stdout, /reclaimable:/);
    // Nothing deleted on a list.
    assert.equal(existsSync(join(tmp, "rune_marches")), true);
    assert.equal(existsSync(join(tmp, `${P}2026-06-15T01-00-00-000Z`)), true);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test("tmp_sweep --all-scratch keeps newest N validate dirs and removes the rest", () => {
  const { dir, tmp } = makeFakeTmp();
  try {
    const result = run(["--all-scratch", "--root", dir, "--keep-validate", "3"]);
    assert.equal(result.status, 0, result.stderr);
    // Scratch removed.
    assert.equal(existsSync(join(tmp, "rune_marches")), false);
    assert.equal(existsSync(join(tmp, "NanoAlpha")), false);
    // Oldest validate dir removed, newest 3 kept.
    assert.equal(existsSync(join(tmp, `${P}2026-06-15T01-00-00-000Z`)), false);
    assert.equal(existsSync(join(tmp, `${P}2026-06-15T02-00-00-000Z`)), true);
    assert.equal(existsSync(join(tmp, `${P}2026-06-15T04-00-00-000Z`)), true);
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
