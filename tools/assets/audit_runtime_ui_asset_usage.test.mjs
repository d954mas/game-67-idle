import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "runtime-ui-usage-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function run(args, cwd) {
  return spawnSync(process.execPath, [join(root, "tools/assets/audit_runtime_ui_asset_usage.mjs"), ...args], {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function writeJson(dir, path, data) {
  mkdirSync(join(dir, dirname(path)), { recursive: true });
  writeFileSync(join(dir, path), `${JSON.stringify(data, null, 2)}\n`, "utf8");
}

function writeFixtures(dir, usageEntry) {
  const manifestPath = "gamedesign/projects/test/data/ui-assets.json";
  const usagePath = "gamedesign/projects/test/data/native-ui-usage.json";
  writeJson(dir, manifestPath, {
    schema: "game.asset_manifest",
    version: 1,
    assets: [
      {
        id: "primary_button",
        kind: "slice9",
        usage_policy: {
          size_class: "large_only",
          min_size: [280, 104],
          disallowed_uses: ["compact_secondary_button", "mobile_dense_button_row"],
        },
      },
    ],
  });
  writeJson(dir, usagePath, {
    schema: "game.runtime_ui_asset_usage",
    version: 1,
    usages: [usageEntry],
  });
  return { manifestPath, usagePath };
}

test("passes when runtime usage obeys min size and layout class", (t) => {
  const dir = tempDir(t);
  const { manifestPath, usagePath } = writeFixtures(dir, {
    id: "primary_button",
    context: "desktop_primary_action",
    layout_mode: "desktop",
    size: [320, 104],
    usage_tags: ["primary_action"],
  });
  const reportPath = "gamedesign/projects/test/reviews/runtime-ui-usage.json";
  const result = run(["--asset-manifest", manifestPath, "--usage", usagePath, "--json-output", reportPath], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const report = JSON.parse(readFileSync(join(dir, reportPath), "utf8"));
  assert.equal(report.schema, "game.runtime_ui_asset_usage_audit");
  assert.equal(report.verdict, "pass");
});

test("fails when a generated UI asset is drawn below usage min size", (t) => {
  const dir = tempDir(t);
  const { manifestPath, usagePath } = writeFixtures(dir, {
    id: "primary_button",
    context: "desktop_primary_action",
    layout_mode: "desktop",
    size: [260, 64],
    usage_tags: ["primary_action"],
  });
  const result = run(["--asset-manifest", manifestPath, "--usage", usagePath], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /width 260 is smaller/);
  assert.match(result.stdout, /height 64 is smaller/);
});

test("fails when large-only art is used in compact dense UI", (t) => {
  const dir = tempDir(t);
  const { manifestPath, usagePath } = writeFixtures(dir, {
    id: "primary_button",
    context: "compact_secondary_action",
    layout_mode: "compact",
    size: [300, 120],
    usage_tags: ["compact_secondary_button"],
  });
  const result = run(["--asset-manifest", manifestPath, "--usage", usagePath], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /disallowed tag compact_secondary_button/);
  assert.match(result.stdout, /not allowed for size_class large_only/);
});
