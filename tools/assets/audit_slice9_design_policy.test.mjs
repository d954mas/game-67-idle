import test from "node:test";
import assert from "node:assert/strict";
import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { tmpdir } from "node:os";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));

function tempDir(t) {
  const dir = mkdtempSync(join(tmpdir(), "slice9-policy-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function run(args, cwd) {
  return spawnSync(process.execPath, [join(root, "tools/assets/audit_slice9_design_policy.mjs"), ...args], {
    cwd,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
}

function writeJson(dir, path, data) {
  mkdirSync(join(dir, dirname(path)), { recursive: true });
  writeFileSync(join(dir, path), `${JSON.stringify(data, null, 2)}\n`, "utf8");
}

function policy() {
  return {
    center: "plain_texture",
    horizontal_edges: "straight_frame",
    vertical_edges: "straight_frame",
    corners: "decorative_fixed",
    non_stretch_ornaments: "corner_only",
  };
}

function usage() {
  return {
    size_class: "flexible",
    min_size: [160, 96],
    disallowed_uses: [],
  };
}

function writeValidManifests(dir, overrides = {}) {
  const cropPath = "gamedesign/projects/test/data/ui-crop.json";
  const runtimePath = "gamedesign/projects/test/data/ui-assets.json";
  const cropEntry = {
    id: "panel",
    kind: "slice9",
    rect: [0, 0, 96, 64],
    output: "assets/runtime/ui/panel.png",
    slice9: { left: 12, top: 12, right: 12, bottom: 12 },
    content: { x: 18, y: 18, w: 60, h: 32 },
    target_preview_sizes: [[160, 96], [240, 160]],
    stretch_policy: policy(),
    usage_policy: usage(),
    ...overrides.crop,
  };
  const runtimeEntry = {
    id: "panel",
    kind: "slice9",
    path: "assets/runtime/ui/panel.png",
    original_size: [96, 64],
    slice9: { left: 12, top: 12, right: 12, bottom: 12 },
    content: { x: 18, y: 18, w: 60, h: 32 },
    target_preview_sizes: [[160, 96], [240, 160]],
    stretch_policy: policy(),
    usage_policy: usage(),
    ...overrides.runtime,
  };
  const extraCropEntries = Array.isArray(overrides.extraCrops) ? overrides.extraCrops : [];
  const extraRuntimeEntries = Array.isArray(overrides.extraRuntimeAssets) ? overrides.extraRuntimeAssets : [];
  writeJson(dir, cropPath, {
    schema: "game.art_crop_manifest",
    version: 1,
    output_dir: "assets/runtime/ui",
    sources: [{ id: "source", path: "source.png", crops: [cropEntry, ...extraCropEntries] }],
  });
  writeJson(dir, runtimePath, {
    schema: "game.asset_manifest",
    version: 1,
    crop_manifest: cropPath,
    runtime_dir: "assets/runtime/ui",
    assets: [runtimeEntry, ...extraRuntimeEntries],
  });
  return { cropPath, runtimePath };
}

test("passes when crop and runtime slice9 design policies match", (t) => {
  const dir = tempDir(t);
  const { cropPath, runtimePath } = writeValidManifests(dir);
  const reportPath = "gamedesign/projects/test/reviews/slice9-policy.json";
  const markdownPath = "gamedesign/projects/test/reviews/slice9-policy.md";
  const result = run(["--crop-manifest", cropPath, "--runtime-manifest", runtimePath, "--json-output", reportPath, "--report", markdownPath, "--profile"], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
  const report = JSON.parse(readFileSync(join(dir, reportPath), "utf8"));
  assert.equal(report.schema, "game.slice9_design_policy_audit");
  assert.equal(report.verdict, "pass");
  assert.equal(report.assets[0].id, "panel");
  assert.ok(report.timing_ms.total >= 0);
  assert.ok(report.assets[0].timing_ms.total >= 0);
  assert.match(result.stdout, /profile: slowest slice9 policy asset `panel`/);
  assert.match(readFileSync(join(dir, markdownPath), "utf8"), /## Timing/);
});

test("fails when slice9 crop omits stretch and usage policy", (t) => {
  const dir = tempDir(t);
  const { cropPath, runtimePath } = writeValidManifests(dir, {
    crop: { stretch_policy: undefined, usage_policy: undefined },
  });
  const result = run(["--crop-manifest", cropPath, "--runtime-manifest", runtimePath], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /needs stretch_policy/);
  assert.match(result.stdout, /needs usage_policy/);
});

test("fails when runtime policy diverges from crop policy", (t) => {
  const dir = tempDir(t);
  const { cropPath, runtimePath } = writeValidManifests(dir, {
    runtime: {
      stretch_policy: {
        ...policy(),
        non_stretch_ornaments: "separate_overlay_assets",
        overlay_family: "ui decor overlay sheet",
      },
    },
  });
  const result = run(["--crop-manifest", cropPath, "--runtime-manifest", runtimePath], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /stretch_policy must match crop manifest/);
});

test("validates separate overlay ornament asset ids", (t) => {
  const dir = tempDir(t);
  const overlayPolicy = {
    ...policy(),
    non_stretch_ornaments: "separate_overlay_assets",
    overlay_asset_ids: ["panel_top_plaque"],
  };
  const { cropPath, runtimePath } = writeValidManifests(dir, {
    crop: { stretch_policy: overlayPolicy },
    runtime: { stretch_policy: overlayPolicy },
    extraCrops: [
      {
        id: "panel_top_plaque",
        kind: "sprite",
        rect: [100, 0, 32, 16],
        output: "assets/runtime/ui/panel-top-plaque.png",
      },
    ],
    extraRuntimeAssets: [
      {
        id: "panel_top_plaque",
        kind: "sprite",
        path: "assets/runtime/ui/panel-top-plaque.png",
      },
    ],
  });
  const result = run(["--crop-manifest", cropPath, "--runtime-manifest", runtimePath], dir);
  assert.equal(result.status, 0, result.stdout + result.stderr);
});

test("fails when separate overlay ornament ids are missing or slice9 assets", (t) => {
  const dir = tempDir(t);
  const badOverlayPolicy = {
    ...policy(),
    non_stretch_ornaments: "separate_overlay_assets",
    overlay_asset_ids: ["missing_plaque", "other_panel"],
  };
  const { cropPath, runtimePath } = writeValidManifests(dir, {
    crop: { stretch_policy: badOverlayPolicy },
    runtime: { stretch_policy: badOverlayPolicy },
    extraCrops: [
      {
        id: "other_panel",
        kind: "slice9",
        rect: [100, 0, 48, 32],
        output: "assets/runtime/ui/other-panel.png",
      },
    ],
    extraRuntimeAssets: [
      {
        id: "other_panel",
        kind: "slice9",
        path: "assets/runtime/ui/other-panel.png",
      },
    ],
  });
  const result = run(["--crop-manifest", cropPath, "--runtime-manifest", runtimePath], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /overlay_asset_id missing_plaque is not present in manifest/);
  assert.match(result.stdout, /overlay_asset_id other_panel must reference a non-slice9 overlay asset/);
});

test("fails when slice9 preview coverage omits min or stress size", (t) => {
  const dir = tempDir(t);
  const { cropPath, runtimePath } = writeValidManifests(dir, {
    crop: { target_preview_sizes: [[180, 96]] },
    runtime: { target_preview_sizes: [[180, 96]] },
  });
  const result = run(["--crop-manifest", cropPath, "--runtime-manifest", runtimePath], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /must include usage_policy\.min_size 160x96/);
  assert.match(result.stdout, /at least two distinct sizes/);
  assert.match(result.stdout, /needs a stress preview/);
});

test("fails when slice9 margins or content safe area are not valid", (t) => {
  const dir = tempDir(t);
  const { cropPath, runtimePath } = writeValidManifests(dir, {
    crop: {
      slice9: { left: 48, top: 12, right: 48, bottom: 12 },
      content: { x: 80, y: 18, w: 40, h: 32 },
    },
    runtime: {
      slice9: { left: 48, top: 12, right: 48, bottom: 12 },
      content: undefined,
    },
  });
  const result = run(["--crop-manifest", cropPath, "--runtime-manifest", runtimePath], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /horizontal margins leave no stretchable center/);
  assert.match(result.stdout, /content safe area exceeds source bounds/);
  assert.match(result.stdout, /needs content safe area/);
});

test("fails when content safe area overlaps fixed slice9 margins", (t) => {
  const dir = tempDir(t);
  const { cropPath, runtimePath } = writeValidManifests(dir, {
    crop: {
      content: { x: 4, y: 8, w: 82, h: 44 },
    },
    runtime: {
      content: { x: 4, y: 8, w: 82, h: 44 },
    },
  });
  const result = run(["--crop-manifest", cropPath, "--runtime-manifest", runtimePath], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /content safe area overlaps fixed left slice9 margin/);
  assert.match(result.stdout, /content safe area overlaps fixed top slice9 margin/);
  assert.match(result.stdout, /content safe area overlaps fixed right slice9 margin/);
});

test("fails when preview leaves no runtime content area", (t) => {
  const dir = tempDir(t);
  const { cropPath, runtimePath } = writeValidManifests(dir, {
    crop: {
      rect: [0, 0, 320, 120],
      content: { x: 140, y: 18, w: 40, h: 50 },
      target_preview_sizes: [[160, 96], [400, 160]],
    },
    runtime: {
      original_size: [320, 120],
      content: { x: 140, y: 18, w: 40, h: 50 },
      target_preview_sizes: [[160, 96], [400, 160]],
    },
  });
  const result = run(["--crop-manifest", cropPath, "--runtime-manifest", runtimePath], dir);
  assert.equal(result.status, 1);
  assert.match(result.stdout, /preview 160x96 leaves no runtime content width/);
});
