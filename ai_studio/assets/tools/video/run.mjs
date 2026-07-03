#!/usr/bin/env node
// Track B video-animation pipeline orchestrator: chains generate -> frames ->
// matte -> sheet on ONE run folder. Each stage is skippable/resumable — if its
// output already exists the stage is skipped unless --force (all) or
// --force-stage <name> (one). Prints the artifact paths + per-stage timings at
// the end.
//
// Contract:
//   node run.mjs --image <png> --text "<motion>" --profile draft|final
//               [--seed N] [--out <runDir>] [--name <slug>]
//               [--tool corridorkey|key_matte] [--screen-color green|blue]
//               [--key 0,255,0] [--columns N] [--trim] [--host h:p]
//               [--force] [--force-stage generate|frames|matte|sheet]
//
// v1 does NOT autostart ComfyUI (boot it yourself first — generate errors loud
// with the start command if it is down) and touches nothing under
// ai_studio/assets/canvas.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import { REPO_ROOT, parseArgs, runRepoPython } from "./_lib.mjs";
import { runGenerate } from "./generate/generate.mjs";
import { runFrames } from "./frames/frames.mjs";
import { runMatte } from "./matte/matte.mjs";

const PACK_SHEET = fileURLToPath(new URL("./sheet/pack_sheet.py", import.meta.url));

function hasMp4(dir) {
  return existsSync(dir) && readdirSync(dir).some((f) => f.endsWith(".mp4"));
}
function hasFrame0(dir) {
  return existsSync(join(dir, "frame_000.png"));
}
function hasSheet(dir) {
  return existsSync(dir) && readdirSync(dir).some((f) => f.endsWith("_sheet.png"));
}
function readJsonMaybe(path) {
  try {
    return JSON.parse(readFileSync(path, "utf8"));
  } catch {
    return null;
  }
}

async function main() {
  const a = parseArgs(process.argv.slice(2));
  const force = !!a.force;
  const forceStage = a["force-stage"];
  const forced = (stage) => force || forceStage === stage;
  const timings = {};

  let runDir = a.out ? resolve(a.out) : null;

  // ---- generate --------------------------------------------------------------
  let genParams = runDir ? readJsonMaybe(join(runDir, "generate", "params.json")) : null;
  const genOutExists = runDir && hasMp4(join(runDir, "generate")) && genParams;
  if (genOutExists && !forced("generate")) {
    console.log(`[run] skip generate (exists) -> ${runDir}`);
    timings.generate = "skipped";
  } else {
    const t0 = Date.now();
    const r = await runGenerate({
      image: a.image,
      text: a.text,
      profile: a.profile || "draft",
      seed: a.seed,
      outDir: runDir,
      name: a.name,
      host: a.host || "127.0.0.1:8188",
    });
    runDir = r.runDir;
    genParams = r.params;
    timings.generate = Number(((Date.now() - t0) / 1000).toFixed(1));
  }
  if (!runDir) throw new Error("no run directory established after generate");

  // ---- frames ----------------------------------------------------------------
  const framesDir = join(runDir, "frames");
  if (hasFrame0(framesDir) && !forced("frames")) {
    console.log(`[run] skip frames (exists) -> ${framesDir}`);
    timings.frames = "skipped";
  } else {
    const t0 = Date.now();
    await runFrames({ runDir });
    timings.frames = Number(((Date.now() - t0) / 1000).toFixed(1));
  }

  // ---- matte -----------------------------------------------------------------
  const matteDir = join(runDir, "matte");
  if (hasFrame0(matteDir) && !forced("matte")) {
    console.log(`[run] skip matte (exists) -> ${matteDir}`);
    timings.matte = "skipped";
  } else {
    const t0 = Date.now();
    await runMatte({
      runDir,
      tool: a.tool || "corridorkey",
      screenColor: a["screen-color"] || "green",
      key: a.key || "0,255,0",
    });
    timings.matte = Number(((Date.now() - t0) / 1000).toFixed(1));
  }

  // ---- sheet -----------------------------------------------------------------
  const sheetDir = join(runDir, "sheet");
  if (hasSheet(sheetDir) && !forced("sheet")) {
    console.log(`[run] skip sheet (exists) -> ${sheetDir}`);
    timings.sheet = "skipped";
  } else {
    const t0 = Date.now();
    const fps = genParams?.fps ?? 16;
    const args = ["--run-dir", runDir, "--fps", String(fps)];
    if (a.columns) args.push("--columns", String(a.columns));
    if (a.trim) args.push("--trim");
    await runRepoPython(PACK_SHEET, args, { root: REPO_ROOT, env: { PYTHONPATH: REPO_ROOT } });
    timings.sheet = Number(((Date.now() - t0) / 1000).toFixed(1));
  }

  // ---- report ----------------------------------------------------------------
  const sheetPng = existsSync(sheetDir) ? readdirSync(sheetDir).find((f) => f.endsWith("_sheet.png")) : null;
  const sheetJson = existsSync(sheetDir) ? readdirSync(sheetDir).find((f) => f.endsWith("_sheet.json")) : null;
  const artifacts = {
    runDir,
    video: genParams?.video_file ?? null,
    framesDir,
    matteDir,
    sheetPng: sheetPng ? join(sheetDir, sheetPng) : null,
    sheetJson: sheetJson ? join(sheetDir, sheetJson) : null,
  };
  console.log("\n=== Track B run complete ===");
  console.log(JSON.stringify({ artifacts, timings_seconds: timings }, null, 2));
  return artifacts;
}

main().catch((error) => {
  console.error(`\n[run] ERROR: ${error.message}`);
  process.exit(1);
});
