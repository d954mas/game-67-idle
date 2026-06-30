import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../../../../", import.meta.url)));

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "new-art-job-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function run(args, cwd) {
  return spawnSync(process.execPath, [join(root, "ai_studio/assets/workflow/art_jobs/new_art_job.mjs"), ...args], {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

test("default concept writes art job paths under gamedesign/projects", (t) => {
  const dir = tempDir(t);
  const result = run([
    "--id", "ui-kit-v1",
    "--family", "starter UI kit",
    "--concept", "test-game",
    "--dry-run",
  ], dir);

  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /would write gamedesign\/projects\/test-game\/art_requests\/ui-kit-v1\.json/);
  assert.match(result.stdout, /would write gamedesign\/projects\/test-game\/data\/ui-kit-v1-crop_plan\.json/);
  assert.doesNotMatch(result.stdout, /gamedesign\/test-game\//);
});

test("explicit project-dir still overrides the concept default", (t) => {
  const dir = tempDir(t);
  const result = run([
    "--id", "ui-kit-v1",
    "--family", "starter UI kit",
    "--concept", "test-game",
    "--project-dir", "custom/art-space",
    "--dry-run",
  ], dir);

  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /would write custom\/art-space\/art_requests\/ui-kit-v1\.json/);
  assert.doesNotMatch(result.stdout, /gamedesign\/projects\/test-game\/art_requests/);
});
