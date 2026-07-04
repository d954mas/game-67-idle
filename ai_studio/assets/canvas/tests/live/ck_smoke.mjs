// LIVE GPU smoke for the canvas "corridorkey" alpha method (T0261). This is NOT part of the
// test suite: it has no `.test.mjs` suffix (so `node --test tests/*.test.mjs` never picks it
// up), it needs the CorridorKey venv + a CUDA GPU under videoGenRoot, and it spends the real
// ~13-16s cold model load. It drives the WHOLE op layer end to end (real green gate ->
// runCorridorKey -> EXR->RGBA -> content-addressed src swap + provenance) on a throwaway
// project under a temp CANVAS_PROJECTS_ROOT, and reports timing. Run manually:
//   node ai_studio/assets/canvas/tests/live/ck_smoke.mjs
import assert from "node:assert/strict";
import { mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { fileURLToPath, URL } from "node:url";

import { addImage, alphaCutout, createProject } from "../../ops.mjs";
import { resolveProjectFile } from "../../store.mjs";
import { decodePng, encodePng } from "../png_fixture.mjs";

const REPO_ROOT = resolve(fileURLToPath(new URL("../../../../..", import.meta.url)));

// A 256x256 GREEN screen (#00FF00) with a soft golden radial glow + an opaque gold core — the
// fractional-alpha soft-edge case CorridorKey is FOR (key_matte would block-quantize the glow).
function greenGlowPng() {
  const W = 256;
  const H = 256;
  const cx = 128;
  const cy = 128;
  const bg = [0, 255, 0];
  const gold = [255, 200, 60];
  return encodePng(W, H, (x, y) => {
    const r = Math.hypot(x - cx, y - cy);
    if (r < 22) return gold; // opaque core
    const glow = Math.pow(Math.max(0, 1 - r / 90), 1.6);
    return [0, 1, 2].map((i) => Math.round(bg[i] * (1 - glow) + gold[i] * glow));
  });
}

async function main() {
  const dir = mkdtempSync(join(tmpdir(), "ck-smoke-"));
  process.env.CANVAS_PROJECTS_ROOT = dir;
  try {
    const project = createProject(REPO_ROOT, { title: "CK live smoke" });
    const { element } = addImage(REPO_ROOT, project.id, { name: "green_glow.png", bytes: greenGlowPng() });

    const t0 = Date.now();
    const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "corridorkey" });
    const wallSeconds = (Date.now() - t0) / 1000;

    const meta = result.element.meta.alpha;
    assert.equal(meta.method, "corridorkey", "provenance method is corridorkey");
    assert.equal(meta.tool, "corridorkey");
    assert.deepEqual(meta.key_color, [0, 255, 0], "green key detected");
    assert.equal(meta.screen_color, "green");

    const png = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, result.element.src)));
    assert.equal(png.channels, 4, "output PNG carries an alpha channel");
    const cornerAlpha = png.at(2, 2)[3]; // green background
    const centerAlpha = png.at(128, 128)[3]; // gold core

    console.log(
      JSON.stringify(
        {
          op_wall_seconds: Number(wallSeconds.toFixed(1)),
          ck_wall_seconds: meta.timings && meta.timings.wall_seconds,
          ck_per_frame_seconds: meta.timings && meta.timings.per_frame_seconds,
          commit: meta.commit,
          corner_alpha: cornerAlpha,
          center_alpha: centerAlpha,
        },
        null,
        2,
      ),
    );
    assert.ok(cornerAlpha < 40, `green corner should be transparent (alpha ${cornerAlpha})`);
    assert.ok(centerAlpha > 200, `gold core should be opaque (alpha ${centerAlpha})`);
    console.log("CK LIVE SMOKE: PASS");
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

main().catch((error) => {
  console.error("CK LIVE SMOKE: FAIL\n", error);
  process.exit(1);
});
