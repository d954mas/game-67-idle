import test from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const tool = resolve(root, "tools/assets/new_generation_record.mjs");

function run(args) {
  return spawnSync(process.execPath, [tool, ...args], {
    cwd: root,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

const baseArgs = [
  "--id", "ui-source-v1",
  "--project-dir", "gamedesign/projects/test",
  "--source-family", "blank UI kit sheet",
  "--accepted-source", "gamedesign/projects/test/art/source_sheets/ui-source-v1.png",
  "--provider", "comfyui",
  "--model", "fantasy-ui-workflow",
  "--workflow-json", "{\"nodes\":[]}",
  "--prompt-packet", "gamedesign/projects/test/art/prompts/ui-source-v1-prompt.json",
  "--seed", "42",
  "--prompt", "blank fantasy UI kit sheet",
  "--negative-prompt", "text, watermark",
  "--dry-run",
];

test("scaffolds generation record dry-run", () => {
  const result = run(baseArgs);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /game\.art_generation_record/);
  assert.match(result.stdout, /"source_family": "blank UI kit sheet"/);
  assert.match(result.stdout, /blank fantasy UI kit sheet/);
  assert.match(result.stdout, /prompt_packet/);
});

test("requires exception for procedural final art", () => {
  const result = run([...baseArgs, "--final-art-source", "procedural"]);
  assert.equal(result.status, 1);
  assert.match(result.stderr, /--procedural-exception is required/);
});

test("rejects empty inline workflow for generated records", () => {
  const args = [...baseArgs];
  args[args.indexOf("--workflow-json") + 1] = "{}";
  const result = run(args);
  assert.equal(result.status, 1);
  assert.match(result.stderr, /--workflow-json must not be empty/);
});

test("requires seed or no-seed reason", () => {
  const args = [...baseArgs];
  args.splice(args.indexOf("--seed"), 2);
  const result = run(args);
  assert.equal(result.status, 1);
  assert.match(result.stderr, /--seed or --no-seed-reason is required/);
});

test("allows generated records with explicit no-seed reason", () => {
  const args = [...baseArgs];
  args.splice(args.indexOf("--seed"), 2, "--no-seed-reason", "built-in provider does not expose a stable seed");
  const result = run(args);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /no_seed_reason/);
});

test("allows procedural final art only with explicit exception", () => {
  const result = run([...baseArgs, "--final-art-source", "procedural", "--procedural-exception", "debug scaffold only"]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /debug scaffold only/);
});

test("allows empty inline workflow only for procedural debug records", () => {
  const args = [...baseArgs];
  args[args.indexOf("--workflow-json") + 1] = "{}";
  const result = run([...args, "--final-art-source", "procedural", "--procedural-exception", "debug scaffold only"]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /procedural/);
});
