#!/usr/bin/env node
// Stage 1 of the Track B pipeline: (art image + motion text) -> a generated
// video, via the isolated ComfyUI WAN 2.2 I2V stack.
//
// Contract:
//   node generate.mjs --image <png> --text "<motion>" --profile draft|final
//                     [--seed N] [--out <runDir>] [--name <slug>] [--host h:p]
//
// What it does:
//   1. Resolves videoGenRoot + the profile workflow JSON (draft/final).
//   2. Checks ComfyUI on 127.0.0.1:8188 is UP. v1 does NOT autostart — a down
//      server is a LOUD error printing the exact start command.
//   3. Copies the input PNG into ComfyUI/input/ and injects it + the hardened
//      positive prompt + the seed into the workflow graph.
//   4. Submits via /prompt, polls /history to completion (LOUD on node/exec
//      errors), collects the mp4 into <runDir>/generate/, and writes
//      params.json provenance (prompt, prefix, seed, profile, workflow, models,
//      timings, comfy prompt_id).
//
// Outputs land under a run folder OUTSIDE the repo (default under
// videoGenRoot/video_runs) — generated video is large and machine-local.
import { copyFileSync, existsSync, readFileSync } from "node:fs";
import { basename, join, resolve } from "node:path";
import { randomUUID } from "node:crypto";
import { fileURLToPath } from "node:url";

import {
  HARDENED_PREFIX,
  REPO_ROOT,
  comfyInputDir,
  comfyOutputDir,
  ensureDir,
  parseArgs,
  sanitizeSlug,
  utcStamp,
  videoGenRoot,
  writeJson,
} from "../_lib.mjs";

const PROFILE_WORKFLOWS = {
  draft: "draft_workflow_api.json",
  final: "final_workflow_api.json",
};

const START_COMMAND =
  "cd C:\\projects\\video_gen_experiment\\ComfyUI_windows_portable\n" +
  "python_embeded\\python.exe -s ComfyUI\\main.py --listen 127.0.0.1 --port 8188";

// ---- ComfyUI HTTP client -----------------------------------------------------

async function comfyUp(base) {
  try {
    const res = await fetch(`${base}/system_stats`, { method: "GET" });
    return res.ok;
  } catch {
    return false;
  }
}

async function submitPrompt(base, prompt, clientId) {
  const res = await fetch(`${base}/prompt`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ prompt, client_id: clientId }),
  });
  const text = await res.text();
  let body;
  try {
    body = JSON.parse(text);
  } catch {
    throw new Error(`ComfyUI /prompt returned non-JSON (HTTP ${res.status}): ${text.slice(0, 500)}`);
  }
  if (!res.ok) {
    throw new Error(`ComfyUI /prompt rejected the graph (HTTP ${res.status}): ${JSON.stringify(body)}`);
  }
  if (body.node_errors && Object.keys(body.node_errors).length) {
    throw new Error(`ComfyUI reported node_errors: ${JSON.stringify(body.node_errors)}`);
  }
  if (!body.prompt_id) throw new Error(`ComfyUI /prompt gave no prompt_id: ${JSON.stringify(body)}`);
  return body.prompt_id;
}

// Poll /history/<id> until the entry is present and completed. Returns the raw
// history entry. LOUD on execution error or timeout.
async function pollHistory(base, promptId, { timeoutMs = 900_000, intervalMs = 1500 } = {}) {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    let entry;
    try {
      const res = await fetch(`${base}/history/${promptId}`, { method: "GET" });
      if (res.ok) {
        const data = await res.json();
        entry = data[promptId];
      }
    } catch {
      // transient — the server may be busy mid-generation; keep polling.
    }
    if (entry && entry.status) {
      const s = entry.status;
      if (s.status_str === "error" || (s.completed === false && /error/i.test(String(s.status_str)))) {
        throw new Error(`ComfyUI execution error for ${promptId}: ${JSON.stringify(s.messages || s)}`);
      }
      if (s.completed === true) return entry;
    }
    if (Date.now() > deadline) {
      throw new Error(`ComfyUI did not finish ${promptId} within ${Math.round(timeoutMs / 1000)}s`);
    }
    await new Promise((r) => setTimeout(r, intervalMs));
  }
}

// Extract execution_start -> execution_success seconds from the history
// messages, if present (ComfyUI timestamps are epoch ms). Returns null when the
// markers are absent so the caller falls back to wall-clock.
function execSeconds(entry) {
  const msgs = entry?.status?.messages || [];
  let start = null;
  let end = null;
  for (const m of msgs) {
    if (!Array.isArray(m) || m.length < 2) continue;
    const [name, data] = m;
    if (name === "execution_start" && data?.timestamp) start = data.timestamp;
    if (name === "execution_success" && data?.timestamp) end = data.timestamp;
  }
  if (start != null && end != null) return { execStartTs: start, execSuccessTs: end, execSeconds: (end - start) / 1000 };
  return null;
}

// ---- stage -------------------------------------------------------------------

export async function runGenerate({
  root = REPO_ROOT,
  image,
  text,
  profile = "draft",
  seed,
  outDir,
  name,
  host = "127.0.0.1:8188",
} = {}) {
  if (!image) throw new Error("generate requires --image <png>");
  if (!text) throw new Error("generate requires --text \"<motion>\"");
  const workflowName = PROFILE_WORKFLOWS[profile];
  if (!workflowName) throw new Error(`--profile must be draft|final (got '${profile}')`);

  const vgRoot = videoGenRoot(root);
  const imageAbs = resolve(image);
  if (!existsSync(imageAbs)) throw new Error(`input image not found: ${imageAbs}`);

  const workflowPath = join(vgRoot, workflowName);
  if (!existsSync(workflowPath)) throw new Error(`profile workflow not found: ${workflowPath}`);

  const base = `http://${host}`;
  if (!(await comfyUp(base))) {
    throw new Error(
      `ComfyUI is not reachable at ${base}. Start it (v1 does NOT autostart), then re-run:\n\n${START_COMMAND}\n`,
    );
  }

  const chosenSeed = Number.isFinite(Number(seed)) ? Number(seed) : Math.floor(Math.random() * 1_000_000);
  const runSlug = sanitizeSlug(name || basename(imageAbs).replace(/\.[^.]+$/, ""));
  const runDir = outDir
    ? resolve(outDir)
    : join(vgRoot, "video_runs", `${runSlug}_${profile}_seed${chosenSeed}_${utcStamp()}`);
  const genDir = ensureDir(join(runDir, "generate"));

  // Load + inject the workflow graph.
  const workflow = JSON.parse(readFileSync(workflowPath, "utf8"));
  const g = workflow.prompt;
  const positive = `${HARDENED_PREFIX}, ${text}`;
  const filenamePrefix = `t0263_${runSlug}_${chosenSeed}`;
  const inputName = `${filenamePrefix}.png`;

  // Copy the source image into ComfyUI's input dir (LoadImage resolves by name).
  copyFileSync(imageAbs, join(ensureDir(comfyInputDir(root)), inputName));

  g["8"].inputs.text = positive; // positive prompt (hardened prefix + motion)
  g["11"].inputs.image = inputName; // LoadImage start frame
  g["13"].inputs.noise_seed = chosenSeed; // high-noise expert
  g["14"].inputs.noise_seed = chosenSeed; // low-noise expert (must match)
  g["17"].inputs.filename_prefix = filenamePrefix;

  const width = g["12"].inputs.width;
  const height = g["12"].inputs.height;
  const length = g["12"].inputs.length;
  const fps = g["16"].inputs.fps;
  const negative = g["9"].inputs.text;
  const models = {
    unet_high: g["1"].inputs.unet_name,
    unet_low: g["2"].inputs.unet_name,
    lora_high: g["3"].inputs.lora_name,
    lora_low: g["4"].inputs.lora_name,
    clip: g["7"].inputs.clip_name,
    vae: g["10"].inputs.vae_name,
  };

  // Submit + wait.
  const clientId = randomUUID();
  const wallStart = Date.now();
  console.log(`[generate] profile=${profile} seed=${chosenSeed} ${width}x${height}/${length}f -> ${runDir}`);
  const promptId = await submitPrompt(base, g, clientId);
  console.log(`[generate] submitted prompt_id=${promptId}; polling /history ...`);
  const entry = await pollHistory(base, promptId);
  const wallSeconds = (Date.now() - wallStart) / 1000;
  const timing = execSeconds(entry);

  // Collect the output mp4. The filename_prefix is unique per run, so the newest
  // matching file in ComfyUI/output is unambiguously this run's video.
  const outputDir = comfyOutputDir(root);
  const produced = existsSync(outputDir)
    ? (await import("node:fs")).readdirSync(outputDir).filter((f) => f.startsWith(filenamePrefix) && f.endsWith(".mp4"))
    : [];
  if (!produced.length) {
    throw new Error(
      `generation completed but no ${filenamePrefix}_*.mp4 found in ${outputDir}. ` +
        `History outputs: ${JSON.stringify(entry.outputs || {})}`,
    );
  }
  produced.sort();
  const srcVideo = join(outputDir, produced[produced.length - 1]);
  const videoDst = join(genDir, `${runSlug}.mp4`);
  copyFileSync(srcVideo, videoDst);

  const params = {
    schema: "ai_studio.video.generate.v1",
    task: "T0263",
    created_utc: new Date().toISOString(),
    profile,
    workflow_file: workflowPath,
    motion_text: text,
    hardened_prefix: HARDENED_PREFIX,
    positive_prompt: positive,
    negative_prompt: negative,
    seed: chosenSeed,
    width,
    height,
    length,
    fps,
    frame_count: length,
    models,
    input_image: { source: imageAbs, comfy_input_name: inputName },
    comfy: {
      host,
      prompt_id: promptId,
      client_id: clientId,
      output_source: srcVideo,
      exec_start_ts: timing?.execStartTs ?? null,
      exec_success_ts: timing?.execSuccessTs ?? null,
      exec_seconds: timing?.execSeconds ?? null,
    },
    wall_seconds: wallSeconds,
    video_file: videoDst,
  };
  writeJson(join(genDir, "params.json"), params);
  console.log(
    `[generate] done in ${wallSeconds.toFixed(1)}s (comfy exec ${timing ? timing.execSeconds.toFixed(1) + "s" : "n/a"}) -> ${videoDst}`,
  );
  return { runDir, video: videoDst, params, wallSeconds };
}

// ---- CLI ---------------------------------------------------------------------

if (process.argv[1] && fileURLToPath(import.meta.url) === resolve(process.argv[1])) {
  const a = parseArgs(process.argv.slice(2));
  runGenerate({
    image: a.image,
    text: a.text,
    profile: a.profile || "draft",
    seed: a.seed,
    outDir: a.out,
    name: a.name,
    host: a.host || "127.0.0.1:8188",
  })
    .then((r) => console.log(JSON.stringify({ runDir: r.runDir, video: r.video }, null, 2)))
    .catch((error) => {
      console.error(`\n[generate] ERROR: ${error.message}`);
      process.exit(1);
    });
}
