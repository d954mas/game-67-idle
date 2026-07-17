import assert from "node:assert/strict";
import { mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";

import { encodePng, solidPng } from "../../../canvas/tests/png_fixture.mjs";
import { runAssetQualityGate } from "./api.mjs";

const ROOT = resolve(fileURLToPath(new URL("../../../../..", import.meta.url)));
const THRESHOLDS = {
  max_spill_edge_ratio: 0.05,
  max_halo_edge_ratio: 0.05,
  max_alpha_noise_ratio: 0.02,
  max_empty_margin_ratio: 0.5,
  aspect_ratio: { width: 1, height: 1, max_relative_error: 0.05 },
};
const VENV_MISSING = /venv|Pillow|interpreter|setup_python|No module|ModuleNotFound/i;

async function runOrSkip(t, args) {
  try {
    return await runAssetQualityGate(ROOT, args);
  } catch (error) {
    if (VENV_MISSING.test(error.message)) {
      t.skip(`asset quality gate unavailable: ${error.message}`);
      return null;
    }
    throw error;
  }
}

test("Node quality-gate adapter returns PASS and fail reports, preserving only failed thumbnails", async (t) => {
  const dir = mkdtempSync(join(tmpdir(), "quality-gate-api-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const cleanPath = join(dir, "clean.png");
  const brokenPath = join(dir, "broken.png");
  writeFileSync(cleanPath, encodePng(32, 32, (x, y) => (
    x >= 4 && x <= 27 && y >= 4 && y <= 27 ? [180, 110, 40, 255] : [0, 0, 0, 0]
  ), { alpha: true }));
  writeFileSync(brokenPath, solidPng(32, 32, [180, 110, 40]));

  const clean = await runOrSkip(t, { sourcePath: cleanPath, keyColor: "#FF00FF", thresholds: THRESHOLDS });
  if (!clean) return;
  assert.equal(clean.report.verdict, "pass");
  assert.equal(clean.thumbnailBytes, null);

  const broken = await runOrSkip(t, { sourcePath: brokenPath, keyColor: "#FF00FF", thresholds: THRESHOLDS });
  if (!broken) return;
  assert.equal(broken.report.verdict, "fail");
  assert.ok(broken.report.problems.some((problem) => problem.code === "no_transparency"));
  assert.ok(Buffer.isBuffer(broken.thumbnailBytes));
});
