import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../../..", import.meta.url)));
const script = join(root, "tools/assets/job/audit_project_asset_boundaries.mjs");

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "asset-boundary-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

test("passes when project files do not contain forbidden asset ids", (t) => {
  const dir = tempDir(t);
  const file = join(dir, "rune_assets.gen.h");
  const report = join(dir, "report.json");
  writeFileSync(file, "RUNE_ASSET_BUTTON\n", "utf8");
  const result = spawnSync(process.execPath, [
    script,
    "--name",
    "rune-clean",
    "--file",
    file,
    "--forbidden-pattern",
    "FISHING|fishing_",
    "--json-output",
    report,
  ], { encoding: "utf8" });

  assert.equal(result.status, 0, result.stdout + result.stderr);
  const json = JSON.parse(readFileSync(report, "utf8"));
  assert.equal(json.verdict, "pass");
});

test("fails when project files contain another project asset id", (t) => {
  const dir = tempDir(t);
  const file = join(dir, "rune_assets.gen.h");
  mkdirSync(dir, { recursive: true });
  writeFileSync(file, "RUNE_ASSET_FISHING_BUTTON\n", "utf8");
  const result = spawnSync(process.execPath, [
    script,
    "--name",
    "rune-contaminated",
    "--file",
    file,
    "--forbidden-pattern",
    "FISHING|fishing_",
  ], { encoding: "utf8" });

  assert.equal(result.status, 1);
  assert.match(result.stdout, /problem:/);
  assert.match(result.stdout, /FISHING/);
});
