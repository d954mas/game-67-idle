import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../../..", import.meta.url)));
const tool = resolve(root, "tools/assets/job/plan_missing_source_family_prompts.mjs");

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "missing-family-prompts-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function run(args, cwd) {
  return spawnSync(process.execPath, [tool, ...args], {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function writeJson(dir, path, data) {
  mkdirSync(join(dir, dirname(path)), { recursive: true });
  writeFileSync(join(dir, path), `${JSON.stringify(data, null, 2)}\n`, "utf8");
}

function writeJob(dir) {
  const job = "gamedesign/projects/test/art_requests/ui-job.json";
  writeJson(dir, job, {
    schema: "game.art_job",
    id: "ui-job",
    asset_family: "runtime-ui-kit",
    must_not_bake: ["button labels", "game state values"],
    qa_rejects: ["watermarks"],
    required_asset_groups: [
      { id: "panel_slice9", kind: "slice9", need: "blank panel and button bases" },
      { id: "resource_icons", kind: "icon", need: "isolated resource icons" },
      { id: "decor_overlays", kind: "sprite", need: "separate corner caps, gems, badges, and medallion overlays" },
    ],
    generation_contract: {
      source_families: ["blank UI kit sheet", "isolated icon sheet", "ui decor overlay sheet"],
      source_family_roles: {
        "blank UI kit sheet": "stretchable bases",
        "isolated icon sheet": "semantic icons",
        "ui decor overlay sheet": "non-stretch ornaments",
      },
      prompt_constraints: ["no readable text", "clear gutters between assets", "no unique decoration inside slice9 stretch zones"],
    },
    expected_outputs: {
      required_source_families: ["blank UI kit sheet", "isolated icon sheet", "ui decor overlay sheet"],
    },
  });
  return job;
}

test("writes prompt queue for source families missing in coverage audit", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  const audit = "gamedesign/projects/test/reviews/source-family-coverage.json";
  writeJson(dir, audit, {
    schema: "game.source_family_coverage_audit",
    verdict: "fail",
    records: [
      { id: "blank", source_family: "blank UI kit sheet", status: "pass" },
      { id: "mixed", source_family: "", status: "fail" },
    ],
  });

  const result = run([
    "--job", job,
    "--coverage-audit", audit,
    "--output-dir", "gamedesign/projects/test/art/prompts",
    "--key-color", "#00ff00",
  ], dir);

  assert.equal(result.status, 0, result.stdout + result.stderr);
  const queue = JSON.parse(readFileSync(join(dir, "gamedesign/projects/test/art/prompts/ui-job-source-family-prompt-queue.json"), "utf8"));
  assert.deepEqual(queue.missing_source_families, ["isolated icon sheet", "ui decor overlay sheet"]);
  assert.equal(queue.packets.length, 2);
  const decor = JSON.parse(readFileSync(join(dir, "gamedesign/projects/test/art/prompts/ui-job-ui-decor-overlay-sheet-prompt.json"), "utf8"));
  assert.match(decor.prompt, /non-stretch decorative overlays/);
  assert.equal(decor.relevant_asset_groups[0].id, "decor_overlays");
});

test("prints no-op when coverage has all required families passing", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  const audit = "coverage.json";
  writeJson(dir, audit, {
    schema: "game.source_family_coverage_audit",
    verdict: "pass",
    records: [
      { source_family: "blank UI kit sheet", status: "pass" },
      { source_family: "isolated icon sheet", status: "pass" },
      { source_family: "ui decor overlay sheet", status: "pass" },
    ],
  });

  const result = run(["--job", job, "--coverage-audit", audit, "--output-dir", "prompts"], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /no missing source families/);
});
