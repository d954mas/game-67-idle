import test from "node:test";
import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../../..", import.meta.url)));
const script = join(root, "tools/assets/job/audit_asset_semantic_style.mjs");

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "asset-style-audit-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function review(overrides = {}) {
  return {
    schema: "game.asset_semantic_style_review",
    source_family: "isolated icon sheet",
    accepted_visual_target: "gamedesign/projects/test/visual/art_bible.md",
    pipeline_stage: "pre_slice",
    style_contract: {
      style_group: "chunky-icy-ui-icons",
      required_traits: ["chunky silhouette", "icy material", "same camera angle"],
      forbidden_mixes: ["coin badge reused as block icon", "castle silhouette used as armor"],
    },
    assets: [
      {
        id: "frost_block_icon",
        intended_role: "resource icon for frost blocks",
        observed_subject: "icy block shard",
        evidence: "contact-sheet row 1 col 2",
        style_group: "chunky-icy-ui-icons",
        semantic_match: "pass",
        style_match: "pass",
        composability: "pass",
        verdict: "accept",
        problems: [],
      },
      {
        id: "coin_badge_block_candidate",
        intended_role: "resource icon for frost blocks",
        observed_subject: "green coin badge",
        evidence: "rejected candidate note",
        style_group: "old-coin-badges",
        semantic_match: "fail",
        style_match: "fail",
        composability: "fail",
        verdict: "reject",
        problems: ["reads as coin currency instead of block resource"],
      },
    ],
    ...overrides,
  };
}

function run(t, data) {
  const dir = tempDir(t);
  const file = join(dir, "review.json");
  writeFileSync(file, `${JSON.stringify(data, null, 2)}\n`, "utf8");
  return spawnSync(process.execPath, [script, "--review", file], {
    cwd: root,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

test("passes accepted matching asset with rejected example", (t) => {
  const result = run(t, review());
  assert.equal(result.status, 0, result.stderr + result.stdout);
  const report = JSON.parse(result.stdout);
  assert.equal(report.verdict, "pass");
  assert.deepEqual(report.accepted_asset_ids, ["frost_block_icon"]);
  assert.deepEqual(report.rejected_asset_ids, ["coin_badge_block_candidate"]);
});

test("fails accepted semantic mismatch", (t) => {
  const data = review({
    assets: [
      {
        id: "armor_icon",
        intended_role: "armor upgrade",
        observed_subject: "castle tower",
        evidence: "contact-sheet row 2 col 1",
        style_group: "chunky-icy-ui-icons",
        semantic_match: "fail",
        style_match: "pass",
        composability: "pass",
        verdict: "accept",
        problems: ["reads as castle instead of armor"],
      },
    ],
  });
  const result = run(t, data);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /cannot be accepted with semantic_match=fail/);
});

test("fails accepted mixed style groups", (t) => {
  const data = review({
    assets: [
      review().assets[0],
      {
        id: "gold_icon",
        intended_role: "gold currency icon",
        observed_subject: "flat vector coin",
        evidence: "contact-sheet row 1 col 3",
        style_group: "flat-vector-icons",
        semantic_match: "pass",
        style_match: "pass",
        composability: "pass",
        verdict: "accept",
        problems: [],
      },
    ],
  });
  const result = run(t, data);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /accepted style_group flat-vector-icons does not match contract/);
  assert.match(result.stdout, /accepted assets mix style groups/);
});

test("fails rejected example without rejection reason", (t) => {
  const badReject = { ...review().assets[1], problems: [] };
  delete badReject.rejection_reason;
  const result = run(t, review({ assets: [review().assets[0], badReject] }));
  assert.equal(result.status, 1);
  assert.match(result.stdout, /rejected asset needs problems or rejection_reason/);
});
