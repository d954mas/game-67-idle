import { createWriteStream, existsSync } from "node:fs";
import { mkdir, readFile, rename, stat, writeFile } from "node:fs/promises";
import { basename, delimiter, dirname, extname, join, resolve } from "node:path";
import { execFile, spawn } from "node:child_process";
import { createInterface } from "node:readline";
import { fileURLToPath } from "node:url";

import { loadStudioConfig } from "../../../../config.mjs";
import { sha256File, sha256Hex } from "../../../../core_harness/tool_lib/hash.mjs";

export const KIMODO_GENERATION_SCHEMA = "ai_studio.asset.kimodo_generation.v1";
export const KIMODO_RETARGET_SCHEMA = "ai_studio.asset.kimodo_retarget.v1";
export const RIG_MAP_SCHEMA = "ai_studio.assets.kimodo_rig_map.v1";
export const KIMODO_SOURCE = "https://github.com/localai-org/kimodo.cpp";

// Only SOMA checkpoints: NVIDIA Open Model License allows commercial use and SOMA is
// the humanoid skeleton. SMPL-X is research-only and G1 is a robot rig.
export const MODELS = Object.freeze({
  "soma-rp-v1.1": { file: "models/kimodo-soma-rp-v1.1-f32.gguf", upstream: "nvidia/Kimodo-SOMA-RP-v1.1", gguf: "LocalAI-io/Kimodo-SOMA-RP-v1.1-GGML" },
  "soma-seed-v1.1": { file: "models/kimodo-soma-seed-v1.1-f32.gguf", upstream: "nvidia/Kimodo-SOMA-SEED-v1.1", gguf: "LocalAI-io/Kimodo-SOMA-SEED-v1.1-GGML" },
});
export const TEXT_MODEL = Object.freeze({ file: "Llama-3-Kimodo-Q8_0.gguf", tokenizer: "tokenizer.gguf", gguf: "LocalAI-io/Llama-3-Kimodo-GGML" });
export const LICENSES = Object.freeze({
  motion_model: "NVIDIA Open Model License https://www.nvidia.com/en-us/agreements/enterprise-software/nvidia-open-model-license/",
  text_encoder: "Meta Llama 3 Community License https://www.llama.com/llama3/license/",
  code: "Apache-2.0 https://github.com/localai-org/kimodo.cpp/blob/main/LICENSE",
});

const TOOL_DIR = dirname(fileURLToPath(import.meta.url));
const BLENDER_SCRIPT = join(TOOL_DIR, "blender_kimodo.py");
const MAPS_DIR = join(TOOL_DIR, "maps");
const DEFAULTS = Object.freeze({ frames: 150, steps: 100, seed: 1, transition: 5, model: "soma-rp-v1.1", backend: "auto" });
const BACKENDS = ["auto", "cpu", "vulkan"];

function integer(value, fallback, label, min, max) {
  const number = value === undefined || value === null || value === "" ? fallback : Number(value);
  if (!Number.isInteger(number) || number < min || number > max) throw new Error(`${label} must be an integer from ${min} to ${max}`);
  return number;
}

function stable(value) {
  if (Array.isArray(value)) return value.map(stable);
  if (value && typeof value === "object") return Object.fromEntries(Object.keys(value).sort().map((key) => [key, stable(value[key])]));
  return value;
}

export function fingerprintOf(value) {
  return sha256Hex(Buffer.from(JSON.stringify(stable(value)), "utf8"));
}

function slug(text, max = 40) {
  const s = String(text).toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-+|-+$/g, "");
  return (s || "clip").slice(0, max).replace(/-+$/, "");
}

async function atomicJson(path, value) {
  const temp = `${path}.tmp`;
  await writeFile(temp, `${JSON.stringify(value, null, 2)}\n`, "utf8");
  await rename(temp, path);
}

function defaultHome(env) {
  const local = String(env.LOCALAPPDATA || "").trim();
  return local ? join(local, "AIStudio", "tools", "kimodo") : "";
}

export function loadKimodoConfig(root = process.cwd(), env = process.env) {
  const config = loadStudioConfig(root);
  const workRoot = env.KIMODO_WORK_ROOT || config.kimodoWorkRoot;
  if (!workRoot) throw new Error("missing kimodoWorkRoot in Studio config (or KIMODO_WORK_ROOT)");
  const home = String(env.KIMODO_HOME || config.kimodoHome || defaultHome(env)).trim();
  if (!home) throw new Error("missing KIMODO_HOME and LOCALAPPDATA is unavailable");
  const blender = String(env.KIMODO_BLENDER || config.blenderExecutable || "").trim();
  // kimodo.cpp always takes Vulkan device 0, which on laptops is usually the integrated GPU.
  const vulkanDevice = String(env.KIMODO_VULKAN_DEVICE ?? config.kimodoVulkanDevice ?? "").trim();
  if (vulkanDevice && !/^\d+$/.test(vulkanDevice)) throw new Error("kimodoVulkanDevice must be a Vulkan device index");
  return { home: resolve(home), workRoot: resolve(root, workRoot), blender: blender ? resolve(blender) : "", vulkanDevice };
}

/** The Vulkan build wins when present; `cpu` forces the CPU backend in either build. */
export function resolveInstall(config, { model = DEFAULTS.model, backend = DEFAULTS.backend } = {}) {
  if (!MODELS[model]) throw new Error(`--model must be one of ${Object.keys(MODELS).join(", ")}`);
  if (!BACKENDS.includes(backend)) throw new Error(`--backend must be one of ${BACKENDS.join(", ")}`);
  const src = join(config.home, "src");
  const builds = ["vulkan", "release"].map((name) => ({ name, generator: join(src, "build", name, "kmd-generate.exe") }));
  const build = backend === "vulkan" ? builds[0] : builds.find((b) => existsSync(b.generator)) || builds[1];
  const weights = join(config.home, "weights");
  const resolvedBackend = backend === "cpu" || build.name === "release" ? "cpu" : "vulkan";
  return {
    src,
    scripts: join(src, "scripts"),
    build: build.name,
    generator: build.generator,
    dllDir: join(dirname(build.generator), "bin"),
    backend: resolvedBackend,
    vulkanDevice: resolvedBackend === "vulkan" ? config.vulkanDevice || "0" : "",
    motionModel: join(weights, MODELS[model].file),
    textModel: join(weights, TEXT_MODEL.file),
    tokenizer: join(weights, TEXT_MODEL.tokenizer),
  };
}

export async function doctor({ root = process.cwd(), env = process.env } = {}) {
  const config = loadKimodoConfig(root, env);
  const install = resolveInstall(config);
  const checks = [
    ["generator", install.generator],
    ["ggml dlls", join(install.dllDir, "ggml.dll")],
    ["bvh exporter", join(install.scripts, "export_bvh.py")],
    ["motion model", install.motionModel],
    ["text encoder", install.textModel],
    ["tokenizer", install.tokenizer],
    ["blender", config.blender],
  ].map(([name, path]) => ({ name, path, ok: Boolean(path) && existsSync(path) }));
  const ok = checks.every((c) => c.ok);
  return { ok, home: config.home, work_root: config.workRoot, build: install.build, backend: install.backend, vulkan_device: install.vulkanDevice, checks, ...(ok ? {} : { hint: "see ai_studio/assets/tools/model/kimodo/README.md#local-setup" }) };
}

function execFileText(file, args) {
  return new Promise((resolvePromise, reject) => {
    execFile(file, args, { windowsHide: true }, (error, stdout) => (error ? reject(error) : resolvePromise(String(stdout).trim())));
  });
}

/** What makes two installs produce different motion: source commit, built binary, weights. */
async function buildIdentity(install) {
  let commit;
  try { commit = await execFileText("git", ["-C", install.src, "rev-parse", "HEAD"]); } catch (error) {
    throw new Error(`cannot read the kimodo.cpp commit in ${install.src}: ${error.message}`);
  }
  const bytes = async (path) => (await stat(path)).size;
  return {
    kimodo_commit: commit,
    generator_sha256: await sha256File(install.generator),
    motion_model_bytes: await bytes(install.motionModel),
    text_model_bytes: await bytes(install.textModel),
  };
}

/**
 * Runs requests through one `kmd-generate --server` process so the 8B text encoder
 * loads once. Never throws after start: a request the server could not answer gets
 * an `ERR` reply, so clips finished before a crash are still kept.
 */
export async function runGeneratorSession(install, requests, logPath, spawnImpl = spawn) {
  const env = { ...process.env, PATH: `${install.dllDir}${delimiter}${process.env.PATH || ""}` };
  if (install.backend === "cpu") env.KIMODO_BACKEND = "cpu";
  else env.GGML_VK_VISIBLE_DEVICES = install.vulkanDevice;
  const log = createWriteStream(logPath, { flags: "a" });
  const child = spawnImpl(install.generator, ["--server", install.motionModel, install.textModel], { env, windowsHide: true, stdio: ["pipe", "pipe", "pipe"] });
  child.stderr.pipe(log, { end: false });
  child.stdin.on("error", () => {}); // a dead server is reported through `died`
  const closed = new Promise((r) => { child.once("close", r); child.once("error", r); });
  const died = new Promise((_, reject) => {
    child.once("error", (error) => reject(new Error(`cannot start kmd-generate: ${error.message}`)));
    child.once("close", (code, signal) => reject(new Error(`kmd-generate exited (${code ?? signal}); see ${logPath}`)));
  });
  died.catch(() => {});
  const lines = createInterface({ input: child.stdout })[Symbol.asyncIterator]();
  const nextReply = async () => {
    for (;;) {
      const next = await Promise.race([lines.next(), died]);
      if (next.done) throw new Error(`kmd-generate closed its output; see ${logPath}`);
      if (/^(OK|ERR)\t/.test(next.value)) return next.value;
      log.write(`[stdout] ${next.value}\n`);
    }
  };
  const replies = [];
  try {
    for (const request of requests) {
      child.stdin.write(`${request.join("\t")}\n`);
      replies.push(await nextReply());
    }
  } catch (error) {
    while (replies.length < requests.length) replies.push(`ERR\t${error.message}`);
  } finally {
    child.stdin.end();
    await closed;
    log.end();
  }
  return replies;
}

function runBlender(blender, args, logPath) {
  return new Promise((resolvePromise, reject) => {
    const log = createWriteStream(logPath);
    // Without --python-exit-code Blender exits 0 when the script raises.
    const child = spawn(blender, ["-b", "--factory-startup", "--python-exit-code", "1", "--python", BLENDER_SCRIPT, "--", ...args], { windowsHide: true, stdio: ["ignore", "pipe", "pipe"] });
    child.stdout.pipe(log, { end: false });
    child.stderr.pipe(log, { end: false });
    child.once("error", (error) => { log.end(); reject(error); });
    child.once("close", (code) => {
      log.end();
      if (code === 0) resolvePromise();
      else reject(new Error(`blender exited with ${code}; see ${logPath}`));
    });
  });
}

function requireOutputs(paths, logPath) {
  const missing = paths.filter((path) => !existsSync(path));
  if (missing.length) throw new Error(`blender finished without ${missing.map((p) => basename(p)).join(", ")}; see ${logPath}`);
}

const DEPENDENCIES = { runGeneratorSession, runBlender, buildIdentity };

export function generationRequest(options = {}) {
  const raw = (Array.isArray(options.prompts) ? options.prompts : [options.prompts]).map((p) => String(p ?? "").trim()).filter(Boolean);
  const prompts = [...new Set(raw)];
  if (prompts.length === 0) throw new Error("--prompt is required");
  return {
    prompts,
    frames: integer(options.frames, DEFAULTS.frames, "--frames", 2, 300),
    steps: integer(options.steps, DEFAULTS.steps, "--steps", 1, 1000),
    seed: integer(options.seed, DEFAULTS.seed, "--seed", 0, 2 ** 31 - 1),
    model: options.model || DEFAULTS.model,
    backend: options.backend || DEFAULTS.backend,
  };
}

export async function generate(options = {}, dependencies = {}) {
  const deps = { ...DEPENDENCIES, ...dependencies };
  const root = options.root || process.cwd();
  const config = loadKimodoConfig(root, options.env || process.env);
  const request = generationRequest(options);
  const install = resolveInstall(config, request);
  if (!config.blender) throw new Error("missing blenderExecutable in Studio local config (or KIMODO_BLENDER)");
  const identity = await deps.buildIdentity(install);

  const clips = request.prompts.map((prompt) => {
    const params = { prompt, frames: request.frames, steps: request.steps, seed: request.seed, model: request.model, text_model: TEXT_MODEL.file, backend: install.backend, vulkan_device: install.vulkanDevice, ...identity };
    const fingerprint = fingerprintOf(params);
    const runDir = join(config.workRoot, "runs", `${slug(prompt)}-${fingerprint.slice(0, 12)}`);
    return { params, fingerprint, runDir, provenance: join(runDir, "provenance.json") };
  });
  const pending = clips.filter((clip) => !existsSync(clip.provenance));

  if (pending.length > 0) {
    await mkdir(join(config.workRoot, "logs"), { recursive: true });
    const requests = [];
    for (const clip of pending) {
      await mkdir(clip.runDir, { recursive: true });
      const promptPath = join(clip.runDir, "prompt.txt");
      await writeFile(promptPath, clip.params.prompt, "utf8");
      requests.push([DEFAULTS.transition, request.steps, request.seed, clip.runDir, request.frames, promptPath]);
    }
    const started = Date.now();
    const replies = await deps.runGeneratorSession(install, requests, join(config.workRoot, "logs", "generator.log"));
    const seconds = (Date.now() - started) / 1000;
    const failures = [];
    for (const [index, clip] of pending.entries()) {
      const reply = String(replies[index] || "");
      if (!reply.startsWith("OK\t")) {
        failures.push(`"${clip.params.prompt}": ${reply || "no reply"}`);
        continue;
      }
      const [, frames, joints] = reply.split("\t");
      const blenderLog = join(clip.runDir, "blender.log");
      try {
        await deps.runBlender(config.blender, ["preview", "--motion-dir", clip.runDir, "--kimodo-scripts", install.scripts], blenderLog);
        requireOutputs([join(clip.runDir, "motion.bvh"), join(clip.runDir, "preview.png")], blenderLog);
      } catch (error) {
        failures.push(`"${clip.params.prompt}": ${error.message}`);
        continue;
      }
      await atomicJson(clip.provenance, {
        schema: KIMODO_GENERATION_SCHEMA,
        origin: "ai",
        provider: "kimodo.cpp-local",
        source: KIMODO_SOURCE,
        model: { id: clip.params.model, upstream: MODELS[clip.params.model].upstream, gguf: MODELS[clip.params.model].gguf },
        text_encoder: { file: TEXT_MODEL.file, gguf: TEXT_MODEL.gguf },
        licenses: LICENSES,
        request: clip.params,
        fingerprint: clip.fingerprint,
        output: { frames: Number(frames), joints: Number(joints), fps: 30, skeleton: "soma30", bvh: "motion.bvh", preview: "preview.png" },
        session_seconds: seconds,
        created_at: new Date().toISOString(),
      });
    }
    // Finished clips already have provenance, so a rerun regenerates only the failed ones.
    if (failures.length) throw new Error(`generation failed for ${failures.length} of ${pending.length} clip(s):\n${failures.join("\n")}`);
  }
  return clips.map((clip) => ({
    cached: !pending.includes(clip),
    prompt: clip.params.prompt,
    run_dir: clip.runDir,
    bvh: join(clip.runDir, "motion.bvh"),
    preview: join(clip.runDir, "preview.png"),
    provenance: clip.provenance,
  }));
}

export function resolveMap(map) {
  const value = String(map || "").trim();
  if (!value) throw new Error("--map is required");
  const path = extname(value).toLowerCase() === ".json" ? resolve(value) : join(MAPS_DIR, `${value}.json`);
  if (!existsSync(path)) throw new Error(`rig map not found: ${path}`);
  return path;
}

export function parseOffsets(entries = []) {
  const offsets = {};
  for (const entry of entries) {
    const match = /^([^=]+)=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)$/.exec(String(entry).trim());
    if (!match) throw new Error(`--offset must look like Bone=x,y,z (degrees): ${entry}`);
    offsets[match[1]] = match.slice(2).map(Number);
  }
  return offsets;
}

export async function retarget(options = {}, dependencies = {}) {
  const deps = { ...DEPENDENCIES, ...dependencies };
  const root = options.root || process.cwd();
  const config = loadKimodoConfig(root, options.env || process.env);
  if (!config.blender) throw new Error("missing blenderExecutable in Studio local config (or KIMODO_BLENDER)");

  const motion = resolve(String(options.motion || "").trim() || ".");
  const bvh = extname(motion).toLowerCase() === ".bvh" ? motion : join(motion, "motion.bvh");
  if (!existsSync(bvh)) throw new Error(`motion not found: ${bvh} (pass a generate run dir or a .bvh)`);
  const character = resolve(String(options.character || "").trim() || ".");
  if (extname(character).toLowerCase() !== ".glb" || !existsSync(character)) throw new Error(`--character must be an existing .glb: ${character}`);
  const mapPath = resolveMap(options.map);
  const rigMap = JSON.parse(await readFile(mapPath, "utf8"));
  if (rigMap.schema !== RIG_MAP_SCHEMA) throw new Error(`rig map ${mapPath} must declare schema ${RIG_MAP_SCHEMA}`);
  const clipName = String(options.clipName || "").trim() || basename(dirname(bvh)).replace(/-[0-9a-f]{12}$/, "") || "kimodo";
  const offsets = options.offsets || {};
  const footLock = options.footLock !== false;

  const input = {
    bvh_sha256: await sha256File(bvh),
    character: { name: basename(character), sha256: await sha256File(character) },
    map: { id: rigMap.id, sha256: await sha256File(mapPath) },
    // The retarget code is part of the input: an improved pass must not reuse old output.
    tool_sha256: sha256Hex(Buffer.concat([await readFile(BLENDER_SCRIPT), await readFile(join(TOOL_DIR, "motion_math.py"))])),
    clip_name: clipName,
    offsets,
    foot_lock: footLock,
  };
  const fingerprint = fingerprintOf(input);
  const outDir = join(config.workRoot, "retarget", `${slug(clipName)}-${slug(basename(character, ".glb"), 24)}-${fingerprint.slice(0, 12)}`);
  const provenancePath = join(outDir, "provenance.json");
  const result = { run_dir: outDir, clip: join(outDir, "clip.glb"), sheet: join(outDir, "sheet.png"), metrics: join(outDir, "metrics.json"), provenance: provenancePath };
  if (existsSync(provenancePath)) return { cached: true, ...result };

  await mkdir(outDir, { recursive: true });
  const args = ["retarget", "--bvh", bvh, "--character", character, "--map", mapPath, "--out-dir", outDir, "--clip-name", clipName, "--offsets", JSON.stringify(offsets)];
  if (!footLock) args.push("--no-foot-lock");
  const blenderLog = join(outDir, "blender.log");
  await deps.runBlender(config.blender, args, blenderLog);
  requireOutputs([result.clip, result.sheet, result.metrics], blenderLog);
  const metrics = JSON.parse(await readFile(result.metrics, "utf8"));
  const motionProvenance = join(dirname(bvh), "provenance.json");
  await atomicJson(provenancePath, {
    schema: KIMODO_RETARGET_SCHEMA,
    origin: "ai",
    source_motion: existsSync(motionProvenance) ? JSON.parse(await readFile(motionProvenance, "utf8")) : { bvh },
    licenses: { ...LICENSES, character: "governed by the character asset's own license" },
    input,
    fingerprint,
    metrics,
    created_at: new Date().toISOString(),
  });
  return { cached: false, ...result, foot_slide: Object.fromEntries(Object.entries(metrics.legs).map(([side, leg]) => [side, { before_m: leg.slide_before_m, after_m: leg.slide_after_m }])) };
}
