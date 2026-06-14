import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "source-family-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function run(args, cwd) {
  return spawnSync(process.execPath, [join(root, "tools/assets/audit_source_family_coverage.mjs"), ...args], {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function writeJson(dir, path, data) {
  mkdirSync(join(dir, dirname(path)), { recursive: true });
  writeFileSync(join(dir, path), `${JSON.stringify(data, null, 2)}\n`, "utf8");
}

function writeJob(dir, records, required = ["blank UI kit sheet", "isolated icon sheet", "ui decor overlay sheet"]) {
  const job = "gamedesign/projects/test/art_requests/ui-job.json";
  writeJson(dir, job, {
    schema: "game.art_job",
    id: "ui-job",
    generation_contract: {
      source_families: ["blank UI kit sheet", "isolated icon sheet", "ui decor overlay sheet"],
    },
    expected_outputs: {
      required_source_families: required,
      generation_records: records,
    },
  });
  return job;
}

function writeRecord(dir, id, sourceFamily, overrides = {}) {
  const source = `gamedesign/projects/test/art/source_sheets/${id}.png`;
  const record = `gamedesign/projects/test/art/generation_records/${id}.json`;
  writeJson(dir, source, { fake: "png placeholder" });
  writeJson(dir, record, {
    schema: "game.art_generation_record",
    id,
    provider: "test",
    model_or_workflow: "test",
    source_family: sourceFamily,
    source_family_role: sourceFamily,
    accepted_source_image: source,
    final_art_source: "generated",
    ...overrides,
  });
  return record;
}

test("passes when every required source family has a separate accepted record", (t) => {
  const dir = tempDir(t);
  const records = [
    writeRecord(dir, "blank-bases", "blank UI kit sheet"),
    writeRecord(dir, "icons", "isolated icon sheet"),
    writeRecord(dir, "decor", "ui decor overlay sheet"),
  ];
  const job = writeJob(dir, records);
  const reportPath = "gamedesign/projects/test/reviews/source-family-coverage.json";
  const result = run(["--job", job, "--json-output", reportPath], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const report = JSON.parse(readFileSync(join(dir, reportPath), "utf8"));
  assert.equal(report.schema, "game.source_family_coverage_audit");
  assert.equal(report.verdict, "pass");
  assert.equal(report.records.length, 3);
});

test("fails when a mixed source sheet tries to satisfy final families", (t) => {
  const dir = tempDir(t);
  const record = writeRecord(dir, "mixed-sheet", "blank UI kit sheet", {
    source_family_role: "mixed first-pass UI kit source; release replacement should split blank bases, decor overlays, and icon sheets",
  });
  const job = writeJob(dir, [record]);
  const result = run(["--job", job], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /missing accepted source family: isolated icon sheet/);
  assert.match(result.stdout, /record appears mixed/);
});

test("fails when required source families are not declared", (t) => {
  const dir = tempDir(t);
  const record = writeRecord(dir, "blank-bases", "blank UI kit sheet");
  const job = writeJob(dir, [record], []);
  const result = run(["--job", job], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /required_source_families/);
});
