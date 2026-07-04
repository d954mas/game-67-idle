// LIVE GPU smoke for the canvas "corridorkey" alpha method (T0261 native green; T0262 magenta
// hue180 shim). This is NOT part of the test suite: it has no `.test.mjs` suffix (so
// `node --test tests/*.test.mjs` never picks it up), it needs the CorridorKey venv + a CUDA GPU
// under videoGenRoot, and it spends the real ~13-16s cold model load PER scenario. It drives the
// WHOLE op layer end to end (real key gate -> [hue180 shim for magenta] -> runCorridorKey ->
// EXR->RGBA -> [hue180 un-shim for magenta] -> content-addressed src swap + provenance) on
// throwaway projects under a temp CANVAS_PROJECTS_ROOT, and reports timing for BOTH the native
// green path and the magenta-shim path. Run manually:
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

// A 256x256 screen with a soft golden radial glow + an opaque gold core — the fractional-alpha
// soft-edge case CorridorKey is FOR (key_matte would block-quantize the glow). `bg` is the flat
// chroma-key background color; green and magenta share IDENTICAL geometry so the two scenarios
// are directly comparable.
function glowPng(bg) {
  const W = 256;
  const H = 256;
  const cx = 128;
  const cy = 128;
  const gold = [255, 200, 60];
  return encodePng(W, H, (x, y) => {
    const r = Math.hypot(x - cx, y - cy);
    if (r < 22) return gold; // opaque core
    const glow = Math.pow(Math.max(0, 1 - r / 90), 1.6);
    return [0, 1, 2].map((i) => Math.round(bg[i] * (1 - glow) + gold[i] * glow));
  });
}

// One scenario end-to-end: mint a project, run the REAL op (no injected seam), assert the
// contract, print the numbers the lead asked for, and assert the fractional-alpha soft glow
// AND (T0262) that the reconstructed FG is NOT hue-shifted (a known subject pixel — the opaque
// gold core — must read back close to gold, never its hue+180 complement).
async function runScenario({ name, bg, expectShim }) {
  const dir = mkdtempSync(join(tmpdir(), `ck-smoke-${name}-`));
  process.env.CANVAS_PROJECTS_ROOT = dir;
  try {
    const project = createProject(REPO_ROOT, { title: `CK live smoke (${name})` });
    const { element } = addImage(REPO_ROOT, project.id, { name: `${name}_glow.png`, bytes: glowPng(bg) });

    const t0 = Date.now();
    const result = await alphaCutout(REPO_ROOT, { projectId: project.id, elementId: element.id, method: "corridorkey" });
    const wallSeconds = (Date.now() - t0) / 1000;

    const meta = result.element.meta.alpha;
    assert.equal(meta.method, "corridorkey", `${name}: provenance method is corridorkey`);
    assert.equal(meta.tool, "corridorkey", `${name}: provenance tool is corridorkey`);
    assert.deepEqual(meta.key_color, bg, `${name}: border key detected as the fixture background`);
    assert.equal(meta.screen_color, "green", `${name}: CK always runs the shipped green checkpoint`);
    if (expectShim) assert.equal(meta.shim, "hue180", `${name}: hue180 shim provenance recorded`);
    else assert.equal(meta.shim, undefined, `${name}: no shim for the native-green path`);

    const png = decodePng(readFileSync(resolveProjectFile(REPO_ROOT, project.id, result.element.src)));
    assert.equal(png.channels, 4, `${name}: output PNG carries an alpha channel`);
    const cornerAlpha = png.at(2, 2)[3]; // chroma background
    const centerPixel = png.at(128, 128); // gold core
    const centerAlpha = centerPixel[3];
    const centerRgb = centerPixel.slice(0, 3);

    const report = {
      scenario: name,
      op_wall_seconds: Number(wallSeconds.toFixed(1)),
      ck_wall_seconds: meta.timings && meta.timings.wall_seconds,
      ck_per_frame_seconds: meta.timings && meta.timings.per_frame_seconds,
      commit: meta.commit,
      shim: meta.shim || null,
      corner_alpha: cornerAlpha,
      center_alpha: centerAlpha,
      center_rgb: centerRgb,
    };
    console.log(JSON.stringify(report, null, 2));

    assert.ok(cornerAlpha < 40, `${name}: background corner should be transparent (alpha ${cornerAlpha})`);
    assert.ok(centerAlpha > 200, `${name}: gold core should be opaque (alpha ${centerAlpha})`);
    // FG must read as GOLD (255,200,60-ish), never hue-shifted to its ~hue+180 complement (a
    // desaturated blue/teal) — the shim must be fully undone on the way out, alpha untouched.
    const [r, g, b] = centerRgb;
    assert.ok(r > 150 && g > 100 && b < 150, `${name}: FG core should read as gold, not hue-shifted (rgb ${r},${g},${b})`);

    console.log(`CK LIVE SMOKE (${name}): PASS`);
    return report;
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
}

async function main() {
  const green = await runScenario({ name: "green", bg: [0, 255, 0], expectShim: false });
  const magenta = await runScenario({ name: "magenta", bg: [255, 0, 255], expectShim: true });
  console.log("CK LIVE SMOKE: ALL PASS");
  return { green, magenta };
}

main().catch((error) => {
  console.error("CK LIVE SMOKE: FAIL\n", error);
  process.exit(1);
});
