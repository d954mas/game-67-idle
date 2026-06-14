import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));
const script = join(root, "tools/assets/audit_atlas_metadata.mjs");

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "atlas-audit-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function run(args, cwd) {
  return spawnSync(process.execPath, [script, ...args], {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function asset(overrides = {}) {
  return {
    id: "panel",
    kind: "slice9",
    path: "assets/panel.png",
    pack_group: "ui_common",
    source_crop: "panel",
    original_size: [96, 64],
    trim_rect: [0, 0, 96, 64],
    atlas_policy: {
      trim_mode: "alpha",
      alpha_bleed: true,
      premultiply_alpha: true,
      extrude: 2,
      shape_padding: 2,
      border_padding: 1,
      scale_variant: "1x",
      allow_rotation: false,
      trim_preserves_slice9: true,
    },
    ...overrides,
  };
}

function writeManifest(dir, assets) {
  const path = join(dir, "assets.json");
  writeFileSync(path, `${JSON.stringify({ schema: "game.asset_manifest", version: 1, assets }, null, 2)}\n`);
  return path;
}

test("passes complete atlas metadata", (t) => {
  const dir = tempDir(t);
  const manifest = writeManifest(dir, [asset()]);
  const result = run(["--asset-manifest", manifest, "--json-output", "report.json", "--report", "report.md"], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const report = JSON.parse(readFileSync(join(dir, "report.json"), "utf8"));
  assert.equal(report.verdict, "pass");
});

test("rejects missing atlas metadata and unsafe slice9 rotation", (t) => {
  const dir = tempDir(t);
  const bad = asset({
    pack_group: "",
    atlas_policy: {
      trim_mode: "tight",
      alpha_bleed: false,
      premultiply_alpha: false,
      extrude: 0,
      shape_padding: 1,
      border_padding: -1,
      scale_variant: "",
      allow_rotation: true,
      trim_preserves_slice9: false,
    },
  });
  const manifest = writeManifest(dir, [bad]);
  const result = run(["--asset-manifest", manifest], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /pack_group/);
  assert.match(result.stdout, /trim_mode/);
  assert.match(result.stdout, /allow_rotation must be false/);
});

test("rejects bad alias links", (t) => {
  const dir = tempDir(t);
  const manifest = writeManifest(dir, [
    asset({ id: "base" }),
    asset({ id: "alias", kind: "sprite", alias_of: "missing", atlas_policy: { ...asset().atlas_policy, allow_rotation: true } }),
  ]);
  const result = run(["--asset-manifest", manifest], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /alias_of references missing asset missing/);
});
