import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const tool = resolve(root, "tools/assets/plan_source_sheet_prompt.mjs");

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "source-prompt-test-"));
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

function writeJob(dir) {
  mkdirSync(join(dir, "gamedesign/projects/test/art_requests"), { recursive: true });
  const jobPath = "gamedesign/projects/test/art_requests/ui-job.json";
  writeFileSync(join(dir, jobPath), `${JSON.stringify({
    schema: "game.art_job",
    id: "ui-job",
    asset_family: "runtime-ui-kit",
    reusable_kinds: ["slice9", "icon", "sprite"],
    required_asset_groups: [
      {
        id: "panel_slice9",
        kind: "slice9",
        need: "blank resizable panel and button bases",
      },
      {
        id: "resource_icons",
        kind: "icon",
        need: "isolated resource icons",
      },
    ],
    must_not_bake: ["button labels", "game state values"],
    generation_contract: {
      source_families: ["blank UI kit sheet", "isolated icon sheet"],
      source_family_roles: {
        "blank UI kit sheet": "stretchable bases without unique stretch-zone ornaments",
        "isolated icon sheet": "semantic icons with gutters",
      },
      final_asset_policy: {
        procedural_art_allowed: "debug_only",
        runtime_final_must_use_generated_or_artist_source: true,
      },
      prompt_constraints: ["no readable text", "clear gutters between assets", "no unique decoration inside slice9 stretch zones"],
    },
    qa_rejects: ["watermarks", "weak silhouette at gameplay size"],
  }, null, 2)}\n`, "utf8");
  return jobPath;
}

test("writes prompt packet from art job and intake suggested key color", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  const audit = "intake.json";
  writeFileSync(join(dir, audit), `${JSON.stringify({ suggested_key_color: "#00ff00" })}\n`, "utf8");
  const markdown = "prompt.md";
  const json = "prompt.json";

  const result = run([
    "--job", job,
    "--source-family", "blank UI kit sheet",
    "--intake-audit", audit,
    "--output", markdown,
    "--json-output", json,
  ], dir);

  assert.equal(result.status, 0, result.stdout + result.stderr);
  const text = readFileSync(join(dir, markdown), "utf8");
  assert.match(text, /flat chroma background #00ff00/);
  assert.match(text, /not a gameplay screenshot/);
  assert.match(text, /no unique ornaments that will stretch/);
  const packet = JSON.parse(readFileSync(join(dir, json), "utf8"));
  assert.equal(packet.suggested_key_color, "#00ff00");
  assert.equal(packet.relevant_asset_groups.length, 1);
  assert.equal(packet.relevant_asset_groups[0].id, "panel_slice9");
});

test("uses explicit key color over intake audit", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  writeFileSync(join(dir, "intake.json"), `${JSON.stringify({ suggested_key_color: "#00ff00" })}\n`, "utf8");

  const result = run([
    "--job", job,
    "--source-family", "isolated icon sheet",
    "--intake-audit", "intake.json",
    "--key-color", "#00ffff",
    "--output", "prompt.md",
    "--json-output", "prompt.json",
  ], dir);

  assert.equal(result.status, 0, result.stdout + result.stderr);
  const packet = JSON.parse(readFileSync(join(dir, "prompt.json"), "utf8"));
  assert.equal(packet.suggested_key_color, "#00ffff");
  assert.equal(packet.relevant_asset_groups[0].id, "resource_icons");
});

test("rejects source family outside generation contract", (t) => {
  const dir = tempDir(t);
  const job = writeJob(dir);
  const result = run([
    "--job", job,
    "--source-family", "whole game screenshot",
    "--output", "prompt.md",
  ], dir);

  assert.equal(result.status, 1);
  assert.match(result.stderr, /not listed in generation_contract/);
});
