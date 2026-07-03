#!/usr/bin/env node
// Stage 2 of the Track B pipeline: generated video -> PNG frames.
//
// Contract:
//   node frames.mjs --run-dir <dir>            (reads <dir>/generate/*.mp4)
//   node frames.mjs --video <mp4> --out <dir>  (standalone)
//
// Uses the ComfyUI portable EMBEDDED Python's PyAV (see video/README.md for why
// the repo venv is not used here). Writes <runDir>/frames/frame_%03d.png plus
// frames.json provenance. LOUD if the source video or embedded interpreter is
// missing.
import { existsSync, readdirSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

import {
  REPO_ROOT,
  embeddedPython,
  ensureDir,
  parseArgs,
  runProcess,
  writeJson,
} from "../_lib.mjs";

const EXTRACT_SCRIPT = fileURLToPath(new URL("./extract_frames.py", import.meta.url));

function findRunVideo(runDir) {
  const genDir = join(runDir, "generate");
  if (!existsSync(genDir)) return null;
  const mp4s = readdirSync(genDir).filter((f) => f.endsWith(".mp4"));
  if (!mp4s.length) return null;
  mp4s.sort();
  return join(genDir, mp4s[mp4s.length - 1]);
}

export async function runFrames({ root = REPO_ROOT, runDir, video, outDir } = {}) {
  let sourceVideo = video ? resolve(video) : runDir ? findRunVideo(resolve(runDir)) : null;
  if (!sourceVideo) {
    throw new Error(
      runDir
        ? `no generated video found under ${join(resolve(runDir), "generate")} (run the generate stage first)`
        : "frames requires --run-dir <dir> or --video <mp4>",
    );
  }
  if (!existsSync(sourceVideo)) throw new Error(`source video not found: ${sourceVideo}`);

  const framesDir = ensureDir(outDir ? resolve(outDir) : join(resolve(runDir), "frames"));
  const python = embeddedPython(root); // LOUD if the experiment is not installed

  const wallStart = Date.now();
  const { code, stdout, stderr } = await runProcess(
    python,
    ["-s", EXTRACT_SCRIPT, "--video", sourceVideo, "--out", framesDir],
    { quiet: true },
  );
  if (code !== 0) {
    throw new Error(`frame extraction failed (exit ${code}):\n${(stderr || stdout).trim()}`);
  }
  let summary = {};
  try {
    summary = JSON.parse(stdout.trim().split(/\r?\n/).pop());
  } catch {
    summary = {};
  }
  const wallSeconds = (Date.now() - wallStart) / 1000;

  const provenance = {
    schema: "ai_studio.video.frames.v1",
    task: "T0263",
    created_utc: new Date().toISOString(),
    source_video: sourceVideo,
    frames_dir: framesDir,
    frame_count: summary.frame_count ?? null,
    avg_fps: summary.avg_fps ?? null,
    extractor: "pyav",
    av_version: summary.av_version ?? null,
    embedded_python: python,
    wall_seconds: wallSeconds,
  };
  writeJson(join(framesDir, "frames.json"), provenance);
  console.log(`[frames] ${summary.frame_count ?? "?"} frames in ${wallSeconds.toFixed(1)}s -> ${framesDir}`);
  return { framesDir, frameCount: summary.frame_count ?? null, wallSeconds };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1])) {
  const a = parseArgs(process.argv.slice(2));
  runFrames({ runDir: a["run-dir"], video: a.video, outDir: a.out })
    .then((r) => console.log(JSON.stringify({ framesDir: r.framesDir, frameCount: r.frameCount }, null, 2)))
    .catch((error) => {
      console.error(`\n[frames] ERROR: ${error.message}`);
      process.exit(1);
    });
}
