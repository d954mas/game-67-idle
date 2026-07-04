#!/usr/bin/env node
// Stage 3 of the Track B pipeline: RGB frames -> RGBA (alpha) frames.
//
// Two tools, chosen by the asset:
//   --tool corridorkey  (DEFAULT) neural unmixer for glow/translucent/soft-edge
//       identity-critical assets. Wraps the EXACT invocation verified in T0257
//       phase-3 R2 (its venv under videoGenRoot/tools/CorridorKey): build a
//       coarse chroma AlphaHint, run corridorkey_cli run-inference, convert the
//       FG+Matte EXR pair to straight RGBA. The ONLY tool that keeps the soft
//       gold glow AND despills. CC-BY-NC-SA-4.0 (asset-processing carve-out;
//       no reselling / paid inference API).
//   --tool key_matte    fast in-repo cutout for OPAQUE / non-glow sprites
//       (ai_studio.assets.tools.image.alpha_matte.key_matte). No licence
//       constraint. Cannot recover soft fractional alpha (its docstring).
//
// Contract:
//   node matte.mjs --run-dir <dir> [--tool corridorkey|key_matte]
//                  [--screen-color green|blue] [--key 0,255,0]
//                  [--frames <dir>] [--out <dir>]
//
// Writes <runDir>/matte/frame_%03d.png (RGBA) + report.json (tool, settings,
// per-frame + wall timing). LOUD if the chosen tool is not installed.
//
// Cross-module reuse (T0261): the canvas alpha op imports runCorridorKey() from here as the
// ONE source of truth for the CorridorKey invocation (prep -> corridorkey_cli -> EXR->RGBA)
// — the canvas "corridorkey" alpha method keys a single element by staging its pixels as a
// 1-frame ClipsForInference shot. The video Track-B pipeline behavior is unchanged; only the
// export visibility of runCorridorKey was widened. See ai_studio/assets/canvas/ops.mjs.
import { existsSync, readdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import {
  REPO_ROOT,
  corridorKeyDir,
  corridorKeyPython,
  ensureDir,
  parseArgs,
  resetDir,
  runProcess,
  runRepoPython,
  sanitizeSlug,
  writeJson,
} from "../_lib.mjs";

const PREP_SCRIPT = fileURLToPath(new URL("./corridorkey_prep.py", import.meta.url));
const EXR_SCRIPT = fileURLToPath(new URL("./ck_exr_to_rgba.py", import.meta.url));
const KEY_MATTE_SCRIPT = fileURLToPath(new URL("./run_key_matte.py", import.meta.url));

// Commit pin + licence, echoed into every report for provenance (T0257 tools table).
const CORRIDORKEY_COMMIT = "97e55a453060745bead1befd293f6e523c4b845c";
const CORRIDORKEY_LICENSE = "CC-BY-NC-SA-4.0 (asset-processing carve-out; no reselling / paid inference API)";

function countPngs(dir) {
  return existsSync(dir) ? readdirSync(dir).filter((f) => f.toLowerCase().endsWith(".png")).length : 0;
}

// Exported (T0261) so the canvas alpha op can reuse the EXACT CorridorKey invocation for its
// single-element "corridorkey" method — one source of truth for prep -> inference -> EXR->RGBA.
export async function runCorridorKey({ root, framesDir, outDir, runDir, screenColor }) {
  const ckDir = corridorKeyDir(root);
  const ckPython = corridorKeyPython(root); // LOUD if the venv is missing

  const shot = `t0263_${sanitizeSlug(runDir ? resolve(runDir).split(/[\\/]/).pop() : "shot")}`;
  const shotDir = resetDir(join(ckDir, "ClipsForInference", shot));

  // 1. Coarse chroma AlphaHint + Input copies (repo venv).
  await runRepoPython(PREP_SCRIPT, ["--frames", framesDir, "--shot", shotDir], { root });

  // 2. CorridorKey inference — the SAME flags R2 used. --skip-existing means any
  //    other ClipsForInference shot that already has Output is skipped; only our
  //    fresh shot is processed. Invoke the venv python DIRECTLY (not `uv run`).
  //
  //    Env (both matter on this box):
  //    - CORRIDORKEY_SKIP_COMPILE=1 forces pure EAGER inference (the tool's own
  //      escape hatch). On Windows CUDA the default max-autotune path builds
  //      triton/cudagraph kernels that error at replay time (NOT caught by the
  //      engine's compile-time eager fallback) — but eager is exactly what R2
  //      effectively ran in (~2.5-4 s/frame), so we force it deterministically.
  //    - PYTHONUTF8/PYTHONIOENCODING: the captured stdout pipe defaults to the
  //      console's cp1251 codec here, which crashes rich's logging on the '->'
  //      (U+2192) glyphs CorridorKey logs. UTF-8 makes the pipe encode them.
  const ckEnv = {
    OPENCV_IO_ENABLE_OPENEXR: "1",
    CORRIDORKEY_SKIP_COMPILE: "1",
    PYTHONUTF8: "1",
    PYTHONIOENCODING: "utf-8",
  };
  const cliArgs = [
    "corridorkey_cli.py",
    "--device",
    "cuda",
    "run-inference",
    "--backend",
    "torch",
    "--srgb",
    "--despill",
    "0",
    "--no-despeckle",
    "--refiner",
    "1.0",
    "--comp",
    "--cpu-post",
    "--screen-color",
    screenColor,
    "--image-size",
    "2048",
    "--skip-existing",
  ];
  const wallStart = Date.now();
  const inf = await runProcess(ckPython, cliArgs, { cwd: ckDir, env: ckEnv });
  if (inf.code !== 0) {
    throw new Error(`CorridorKey inference failed (exit ${inf.code}):\n${(inf.stderr || inf.stdout).trim()}`);
  }
  const wallSeconds = (Date.now() - wallStart) / 1000;

  // 3. FG + Matte EXR -> straight RGBA (CorridorKey venv, has cv2/OpenEXR).
  const fgDir = join(shotDir, "Output", "FG");
  const matteExrDir = join(shotDir, "Output", "Matte");
  if (!existsSync(matteExrDir)) {
    throw new Error(`CorridorKey produced no Matte EXR at ${matteExrDir} — inference may have skipped the shot`);
  }
  const conv = await runProcess(
    ckPython,
    [EXR_SCRIPT, "--fg", fgDir, "--matte", matteExrDir, "--out", outDir],
    { cwd: ckDir, env: ckEnv, quiet: true },
  );
  if (conv.code !== 0) {
    throw new Error(`EXR->RGBA conversion failed (exit ${conv.code}):\n${(conv.stderr || conv.stdout).trim()}`);
  }

  const frameCount = countPngs(outDir);
  return {
    report: {
      tool: "corridorkey",
      commit: CORRIDORKEY_COMMIT,
      license: CORRIDORKEY_LICENSE,
      screen_color: screenColor,
      settings: {
        backend: "torch",
        input_colorspace: "srgb",
        despill: 0,
        despeckle: false,
        refiner: 1.0,
        comp: true,
        gpu_post: false,
        image_size: 2048,
        compile: "skipped (CORRIDORKEY_SKIP_COMPILE=1 -> pure eager)",
      },
      shot_dir: shotDir,
      cli: `CORRIDORKEY_SKIP_COMPILE=1 python ${cliArgs.join(" ")}`,
      frame_count: frameCount,
      wall_seconds: wallSeconds,
      per_frame_seconds: frameCount ? Number((wallSeconds / frameCount).toFixed(2)) : null,
      note: "wall_seconds is gross (includes ~2048 backbone model load). Inference runs in EAGER mode on this Windows CUDA box.",
    },
    frameCount,
    wallSeconds,
  };
}

async function runKeyMatte({ root, framesDir, outDir, key }) {
  const wallStart = Date.now();
  const stdout = await runRepoPython(
    KEY_MATTE_SCRIPT,
    ["--frames", framesDir, "--out", outDir, "--key", key],
    { root, env: { PYTHONPATH: REPO_ROOT } },
  );
  const wallSeconds = (Date.now() - wallStart) / 1000;
  let summary = {};
  try {
    summary = JSON.parse(stdout.trim().split(/\r?\n/).pop());
  } catch {
    summary = {};
  }
  return {
    report: {
      tool: "key_matte",
      module: "ai_studio.assets.tools.image.alpha_matte.key_matte",
      key: summary.key ?? key,
      frame_count: summary.frame_count ?? countPngs(outDir),
      mean_ms: summary.mean_ms ?? null,
      per_frame_ms: summary.per_frame_ms ?? null,
      wall_seconds: wallSeconds,
    },
    frameCount: summary.frame_count ?? countPngs(outDir),
    wallSeconds,
  };
}

export async function runMatte({
  root = REPO_ROOT,
  runDir,
  tool = "corridorkey",
  screenColor = "green",
  key = "0,255,0",
  framesDir,
  outDir,
} = {}) {
  const resolvedFrames = framesDir ? resolve(framesDir) : runDir ? join(resolve(runDir), "frames") : null;
  if (!resolvedFrames || !existsSync(resolvedFrames)) {
    throw new Error(`matte needs frames; expected ${resolvedFrames || "--frames/--run-dir"} (run the frames stage first)`);
  }
  if (!countPngs(resolvedFrames)) throw new Error(`no .png frames in ${resolvedFrames}`);
  const matteDir = ensureDir(outDir ? resolve(outDir) : join(resolve(runDir), "matte"));

  let result;
  if (tool === "corridorkey") {
    result = await runCorridorKey({ root, framesDir: resolvedFrames, outDir: matteDir, runDir, screenColor });
  } else if (tool === "key_matte") {
    result = await runKeyMatte({ root, framesDir: resolvedFrames, outDir: matteDir, key });
  } else {
    throw new Error(`--tool must be corridorkey|key_matte (got '${tool}')`);
  }

  const report = {
    schema: "ai_studio.video.matte.v1",
    task: "T0263",
    created_utc: new Date().toISOString(),
    frames_dir: resolvedFrames,
    matte_dir: matteDir,
    ...result.report,
  };
  writeJson(join(matteDir, "report.json"), report);
  console.log(
    `[matte] tool=${tool} ${result.frameCount} frames in ${result.wallSeconds.toFixed(1)}s -> ${matteDir}`,
  );
  return { matteDir, frameCount: result.frameCount, wallSeconds: result.wallSeconds, report };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1])) {
  const a = parseArgs(process.argv.slice(2));
  runMatte({
    runDir: a["run-dir"],
    tool: a.tool || "corridorkey",
    screenColor: a["screen-color"] || "green",
    key: a.key || "0,255,0",
    framesDir: a.frames,
    outDir: a.out,
  })
    .then((r) => console.log(JSON.stringify({ matteDir: r.matteDir, frameCount: r.frameCount }, null, 2)))
    .catch((error) => {
      console.error(`\n[matte] ERROR: ${error.message}`);
      process.exit(1);
    });
}
