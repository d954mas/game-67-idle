import test from "node:test";
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const tool = resolve(root, "tools/assets/run_ui_asset_tier.mjs");

function run(args) {
  return spawnSync(process.execPath, [tool, ...args], {
    cwd: root,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

test("draft tier plans <=2 cheap commands", () => {
  const result = run(["--tier", "draft", "--plan"]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /tier: draft/);
  const count = Number(result.stdout.match(/commands: (\d+)/)[1]);
  assert.ok(count <= 2, `draft must be <=2 commands, got ${count}`);
  assert.match(result.stdout, /audit_source_sheet_intake\.py/);
  // No final-only battery gate should leak into draft.
  assert.doesNotMatch(result.stdout, /--final-art/);
});

test("integrate tier plans ~3 wiring commands without the full battery", () => {
  const result = run(["--tier", "integrate", "--plan"]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const count = Number(result.stdout.match(/commands: (\d+)/)[1]);
  assert.equal(count, 3, `integrate should be 3 commands, got ${count}`);
  assert.match(result.stdout, /validate_art_job\.mjs --job .* --strict/);
  assert.match(result.stdout, /audit_generated_ui_assets\.py/);
  assert.match(result.stdout, /render_ui_composition_proof\.py/);
  assert.doesNotMatch(result.stdout, /--final-art/);
});

test("final tier plans the full battery incl. --final-art", () => {
  const result = run(["--tier", "final", "--plan"]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const count = Number(result.stdout.match(/commands: (\d+)/)[1]);
  assert.ok(count >= 10, `final battery should be the full set, got ${count}`);
  assert.match(result.stdout, /validate_art_job\.mjs --job .* --final-art/);
  assert.match(result.stdout, /audit_source_family_coverage\.mjs/);
  assert.match(result.stdout, /audit_generated_source_derivation\.py/);
});

test("supplied paths are substituted into the plan", () => {
  const result = run([
    "--tier", "integrate", "--plan",
    "--job", "gamedesign/projects/x/art_requests/ui.json",
    "--crop-manifest", "gamedesign/projects/x/data/crop.json",
    "--runtime-manifest", "gamedesign/projects/x/data/runtime.json",
  ]);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  assert.match(result.stdout, /gamedesign\/projects\/x\/art_requests\/ui\.json/);
  assert.match(result.stdout, /gamedesign\/projects\/x\/data\/crop\.json/);
  assert.match(result.stdout, /gamedesign\/projects\/x\/data\/runtime\.json/);
});

test("rejects unknown tier", () => {
  const result = run(["--tier", "bogus", "--plan"]);
  assert.equal(result.status, 1);
  assert.match(result.stderr, /unknown tier/);
});

test("requires --plan; --execute is not implemented", () => {
  const missing = run(["--tier", "draft"]);
  assert.equal(missing.status, 1);
  assert.match(missing.stderr, /--plan/);

  const exec = run(["--tier", "draft", "--execute"]);
  assert.equal(exec.status, 1);
  assert.match(exec.stderr, /not implemented/);
});
