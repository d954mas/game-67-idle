import { existsSync } from "node:fs";
import { mkdir, readFile, rename, writeFile } from "node:fs/promises";
import { basename, dirname, extname, join, resolve } from "node:path";

import { loadStudioConfig } from "../../../../config.mjs";
import { sha256File, sha256Hex } from "../../../../core_harness/tool_lib/hash.mjs";

export const MESHY_BASE_URL = "https://api.meshy.ai";
export const MESHY_JOB_SCHEMA = "ai_studio.asset.meshy_job.v1";
export const MESHY_PROVENANCE_SCHEMA = "ai_studio.asset.meshy_generation.v1";
export const PRICING_SOURCE = "https://docs.meshy.ai/en/api/pricing";
export const PRICING_VERIFIED_AT = "2026-08-05";

const TERMINAL = new Set(["SUCCEEDED", "FAILED", "CANCELED"]);
const IMAGE_MIME = new Map([
  [".jpg", "image/jpeg"],
  [".jpeg", "image/jpeg"],
  [".png", "image/png"],
]);

const PROFILES = Object.freeze({
  "text-preview": Object.freeze({
    draft: { credits: 5, aiModel: "meshy-5", targetPolycount: 15_000 },
    game: { credits: 20, aiModel: "latest", targetPolycount: 15_000 },
  }),
  "text-refine": Object.freeze({
    draft: { credits: 10, aiModel: "meshy-5", textureResolution: "2k" },
    game: { credits: 10, aiModel: "latest", textureResolution: "2k" },
    ultra: { credits: 15, aiModel: "latest", textureResolution: "8k" },
  }),
  image: Object.freeze({
    draft: { credits: 5, modelType: "smart-topology", aiModel: "meshy-t2", shouldTexture: false, targetPolycount: 4_000 },
    game: { credits: 15, modelType: "smart-topology", aiModel: "meshy-t2", shouldTexture: true, targetPolycount: 8_000, textureResolution: "2k" },
    quality: { credits: 30, modelType: "standard", aiModel: "latest", shouldTexture: true, targetPolycount: 30_000, textureResolution: "2k" },
    ultra: { credits: 35, modelType: "standard", aiModel: "latest", shouldTexture: true, targetPolycount: 30_000, textureResolution: "8k" },
  }),
});

function requiredText(value, label, max = Infinity) {
  const text = typeof value === "string" ? value.trim() : "";
  if (!text) throw new Error(`${label} is required`);
  if (text.length > max) throw new Error(`${label} must be at most ${max} characters`);
  return text;
}

function taskId(value, label = "task id") {
  const id = requiredText(value, label, 128);
  if (!/^[a-zA-Z0-9-]+$/.test(id)) throw new Error(`${label} contains unsupported characters`);
  return id;
}

function positiveInteger(value, fallback, label) {
  if (value === undefined || value === null || value === "") return fallback;
  const number = Number(value);
  if (!Number.isInteger(number) || number < 100 || number > 300_000) {
    throw new Error(`${label} must be an integer from 100 to 300000`);
  }
  return number;
}

function profileFor(kind, name) {
  const profiles = PROFILES[kind];
  if (!profiles) throw new Error(`kind must be text-preview, text-refine, or image (got ${JSON.stringify(kind)})`);
  const fallback = "draft";
  const profileName = name || fallback;
  const profile = profiles[profileName];
  if (!profile) throw new Error(`${kind} profile must be one of ${Object.keys(profiles).join("|")} (got ${JSON.stringify(profileName)})`);
  return { profileName, profile };
}

function publicPayload(payload) {
  if (!payload.image_url?.startsWith("data:")) return payload;
  return { ...payload, image_url: `${payload.image_url.slice(0, payload.image_url.indexOf(",") + 1)}<redacted>` };
}

function stable(value) {
  if (Array.isArray(value)) return value.map(stable);
  if (value && typeof value === "object") {
    return Object.fromEntries(Object.keys(value).sort().map((key) => [key, stable(value[key])]));
  }
  return value;
}

function fingerprintOf(value) {
  return sha256Hex(Buffer.from(JSON.stringify(stable(value)), "utf8"));
}

export function loadMeshyConfig(root = process.cwd(), env = process.env) {
  const config = loadStudioConfig(root);
  const configuredRoot = env.MESHY_WORK_ROOT || config.meshyWorkRoot;
  if (!configuredRoot) throw new Error("missing meshyWorkRoot in Studio config (or MESHY_WORK_ROOT)");
  const reserveRaw = env.MESHY_CREDIT_RESERVE ?? config.meshyCreditReserve ?? 20;
  const reserveCredits = Number(reserveRaw);
  if (!Number.isInteger(reserveCredits) || reserveCredits < 0) {
    throw new Error("meshyCreditReserve must be a non-negative integer");
  }
  return {
    apiKey: String(env.MESHY_API_KEY || "").trim(),
    workRoot: resolve(root, configuredRoot),
    reserveCredits,
  };
}

export async function buildPlan(kind, options = {}) {
  const { profileName, profile } = profileFor(kind, options.profile);
  let endpoint;
  let taskEndpoint;
  let payload;
  let input = null;

  if (kind === "text-preview") {
    const prompt = requiredText(options.prompt, "--prompt", 600);
    const targetPolycount = positiveInteger(options.targetPolycount, profile.targetPolycount, "--target-polycount");
    endpoint = "/openapi/v2/text-to-3d";
    taskEndpoint = endpoint;
    payload = {
      mode: "preview",
      prompt,
      model_type: "standard",
      ai_model: profile.aiModel,
      should_remesh: true,
      topology: "triangle",
      target_polycount: targetPolycount,
      target_formats: ["glb"],
      moderation: true,
    };
  } else if (kind === "text-refine") {
    endpoint = "/openapi/v2/text-to-3d";
    taskEndpoint = endpoint;
    payload = {
      mode: "refine",
      preview_task_id: taskId(options.previewTaskId, "--preview-task-id"),
      ai_model: profile.aiModel,
      enable_pbr: true,
      texture_resolution: profile.textureResolution,
      remove_lighting: true,
      target_formats: ["glb"],
      moderation: true,
    };
    if (options.texturePrompt) payload.texture_prompt = requiredText(options.texturePrompt, "--texture-prompt", 600);
  } else if (kind === "image") {
    const imagePath = resolve(requiredText(options.image, "--image"));
    const mime = IMAGE_MIME.get(extname(imagePath).toLowerCase());
    if (!mime) throw new Error("--image must be a .png, .jpg, or .jpeg file");
    if (!existsSync(imagePath)) throw new Error(`image not found: ${imagePath}`);
    const imageSha256 = await sha256File(imagePath);
    const bytes = await readFile(imagePath);
    const targetPolycount = positiveInteger(options.targetPolycount, profile.targetPolycount, "--target-polycount");
    if (profile.modelType === "smart-topology" && targetPolycount > 15_000) {
      throw new Error("smart-topology --target-polycount must not exceed 15000");
    }
    if (!profile.shouldTexture && options.texturePrompt) {
      throw new Error("--texture-prompt cannot be used with the untextured image draft profile");
    }
    endpoint = "/openapi/v1/image-to-3d";
    taskEndpoint = endpoint;
    payload = {
      image_url: `data:${mime};base64,${bytes.toString("base64")}`,
      model_type: profile.modelType,
      ai_model: profile.aiModel,
      should_texture: profile.shouldTexture,
      target_polycount: targetPolycount,
      target_formats: ["glb"],
      moderation: true,
    };
    if (profile.shouldTexture) {
      payload.enable_pbr = true;
      payload.texture_resolution = profile.textureResolution;
      payload.remove_lighting = true;
      if (options.texturePrompt) payload.texture_prompt = requiredText(options.texturePrompt, "--texture-prompt", 600);
    }
    if (profile.modelType === "standard") {
      payload.should_remesh = true;
      payload.topology = "triangle";
    }
    input = { path: imagePath, filename: basename(imagePath), mime, sha256: imageSha256, bytes: bytes.length };
  }

  const fingerprintPayload = { kind, profile: profileName, payload: publicPayload(payload), input };
  const fingerprint = fingerprintOf(fingerprintPayload);
  return {
    schema: "ai_studio.asset.meshy_plan.v1",
    provider: "meshy",
    kind,
    profile: profileName,
    endpoint,
    task_endpoint: taskEndpoint,
    payload,
    request: publicPayload(payload),
    input,
    estimated_credits: profile.credits,
    pricing_source: PRICING_SOURCE,
    pricing_verified_at: PRICING_VERIFIED_AT,
    fingerprint,
  };
}

function paidApproval(plan, options) {
  if (options.execute !== true) {
    throw new Error(`paid generation blocked: pass --execute and --confirm-credits ${plan.estimated_credits} after reviewing the plan`);
  }
  const confirmed = Number(options.confirmCredits);
  if (!Number.isInteger(confirmed) || confirmed !== plan.estimated_credits) {
    throw new Error(`paid generation blocked: --confirm-credits must equal the estimated ${plan.estimated_credits}`);
  }
  const maximum = Number(options.maxCredits);
  if (!Number.isInteger(maximum) || maximum !== plan.estimated_credits) {
    throw new Error(`paid generation blocked: --max-credits must exactly equal the estimated ${plan.estimated_credits}`);
  }
}

async function atomicJson(path, value) {
  await mkdir(dirname(path), { recursive: true });
  const temp = `${path}.${process.pid}.tmp`;
  await writeFile(temp, `${JSON.stringify(value, null, 2)}\n`, "utf8");
  await rename(temp, path);
}

async function readJson(path) {
  if (!existsSync(path)) return null;
  return JSON.parse((await readFile(path, "utf8")).replace(/^\uFEFF/, ""));
}

function isGlb(buffer) {
  if (buffer.length < 20
    || buffer.subarray(0, 4).toString("ascii") !== "glTF"
    || buffer.readUInt32LE(4) !== 2
    || buffer.readUInt32LE(8) !== buffer.length) return false;
  let offset = 12;
  let firstChunk = true;
  try {
    while (offset < buffer.length) {
      if (offset + 8 > buffer.length) return false;
      const chunkLength = buffer.readUInt32LE(offset);
      const chunkType = buffer.readUInt32LE(offset + 4);
      const chunkStart = offset + 8;
      const chunkEnd = chunkStart + chunkLength;
      if (chunkLength % 4 !== 0 || chunkEnd > buffer.length) return false;
      if (firstChunk) {
        if (chunkType !== 0x4e4f534a) return false;
        const document = JSON.parse(buffer.subarray(chunkStart, chunkEnd).toString("utf8").replace(/[\0 ]+$/, ""));
        if (document?.asset?.version !== "2.0") return false;
      }
      firstChunk = false;
      offset = chunkEnd;
    }
  } catch {
    return false;
  }
  return !firstChunk && offset === buffer.length;
}

function authHeaders(apiKey, json = false) {
  if (!apiKey) throw new Error("MESHY_API_KEY is not set");
  return {
    authorization: `Bearer ${apiKey}`,
    ...(json ? { "content-type": "application/json" } : {}),
  };
}

async function apiJson(fetchImpl, url, { apiKey, method = "GET", body } = {}) {
  const response = await fetchImpl(url, {
    method,
    headers: authHeaders(apiKey, body !== undefined),
    ...(body !== undefined ? { body: JSON.stringify(body) } : {}),
  });
  const text = await response.text();
  let data;
  try {
    data = text ? JSON.parse(text) : {};
  } catch {
    throw new Error(`Meshy returned non-JSON (HTTP ${response.status})`);
  }
  if (!response.ok) {
    const message = data?.message || data?.detail || data?.error || response.statusText || "request failed";
    const error = new Error(`Meshy HTTP ${response.status}: ${typeof message === "string" ? message : JSON.stringify(message)}`);
    error.definiteHttpRejection = true;
    throw error;
  }
  return data;
}

export async function getBalance({ root = process.cwd(), env = process.env, fetchImpl = fetch } = {}) {
  const config = loadMeshyConfig(root, env);
  const data = await apiJson(fetchImpl, `${MESHY_BASE_URL}/openapi/v1/balance`, { apiKey: config.apiKey });
  if (!Number.isFinite(Number(data.balance))) throw new Error("Meshy balance response has no numeric balance");
  return { balance: Number(data.balance), reserve_credits: config.reserveCredits };
}

async function downloadGlb(fetchImpl, url) {
  let parsed;
  try {
    parsed = new URL(url);
  } catch {
    throw new Error("Meshy task returned an invalid GLB URL");
  }
  if (parsed.protocol !== "https:") throw new Error("Meshy GLB download URL must use HTTPS");
  const response = await fetchImpl(parsed.href, { method: "GET" });
  if (!response.ok) throw new Error(`Meshy GLB download failed: HTTP ${response.status}`);
  const buffer = Buffer.from(await response.arrayBuffer());
  if (!isGlb(buffer)) throw new Error("Meshy GLB download is not a valid GLB 2.0 file");
  return buffer;
}

function sleep(ms) {
  return new Promise((done) => setTimeout(done, ms));
}

export async function runGeneration(kind, options = {}, dependencies = {}) {
  const root = dependencies.root || process.cwd();
  const env = dependencies.env || process.env;
  const fetchImpl = dependencies.fetchImpl || fetch;
  const now = dependencies.now || (() => new Date());
  const pollIntervalMs = dependencies.pollIntervalMs ?? 5_000;
  const timeoutMs = dependencies.timeoutMs ?? 20 * 60_000;
  const plan = await buildPlan(kind, options);
  const config = loadMeshyConfig(root, env);
  const reserveCredits = options.reserveCredits === undefined ? config.reserveCredits : Number(options.reserveCredits);
  if (!Number.isInteger(reserveCredits) || reserveCredits < 0) throw new Error("--reserve-credits must be a non-negative integer");
  if (reserveCredits < config.reserveCredits) {
    const confirmedReserve = Number(options.confirmReserveCredits);
    if (!Number.isInteger(confirmedReserve) || confirmedReserve !== reserveCredits) {
      throw new Error(`paid generation blocked: --confirm-reserve-credits must equal ${reserveCredits}`);
    }
  }

  const runDir = join(config.workRoot, plan.fingerprint);
  const jobPath = join(runDir, "job.json");
  const outputPath = join(runDir, "model.glb");
  const provenancePath = join(runDir, "provenance.json");
  let job = await readJson(jobPath);
  if (job && (job.schema !== MESHY_JOB_SCHEMA || job.fingerprint !== plan.fingerprint)) {
    throw new Error(`Meshy job record does not match the planned request: ${jobPath}`);
  }
  if (job?.status === "SUCCEEDED" && existsSync(outputPath) && existsSync(provenancePath)) {
    return { cached: true, resumed: true, task_id: job.task_id, run_dir: runDir, model: outputPath, provenance: provenancePath, plan: printablePlan(plan) };
  }
  if (job?.status === "SUBMISSION_UNKNOWN" && !job.task_id) {
    throw new Error(`previous Meshy submission outcome is unknown; reconcile account tasks before retrying: ${jobPath}`);
  }

  const resumedExistingTask = Boolean(job?.task_id);
  let balanceBefore = job?.balance_before ?? null;
  if (!job?.task_id) {
    paidApproval(plan, options);
    const balance = await getBalance({ root, env, fetchImpl });
    balanceBefore = balance.balance;
    if (balanceBefore < plan.estimated_credits + reserveCredits) {
      throw new Error(`Meshy generation blocked: balance ${balanceBefore}, estimate ${plan.estimated_credits}, reserve ${reserveCredits}`);
    }
    job = {
      schema: MESHY_JOB_SCHEMA,
      fingerprint: plan.fingerprint,
      kind,
      profile: plan.profile,
      status: "SUBMITTING",
      estimated_credits: plan.estimated_credits,
      balance_before: balanceBefore,
      created_at: now().toISOString(),
      request: plan.request,
      input: plan.input,
    };
    await atomicJson(jobPath, job);
    let created;
    try {
      created = await apiJson(fetchImpl, `${MESHY_BASE_URL}${plan.endpoint}`, {
        apiKey: config.apiKey,
        method: "POST",
        body: plan.payload,
      });
    } catch (error) {
      const status = error.definiteHttpRejection ? "REJECTED" : "SUBMISSION_UNKNOWN";
      job = { ...job, status, last_error: error.message, updated_at: now().toISOString() };
      await atomicJson(jobPath, job);
      if (status === "SUBMISSION_UNKNOWN") {
        throw new Error(`${error.message}; submission was not retried to avoid duplicate credit spend`);
      }
      throw error;
    }
    const createdTaskId = taskId(created.result, "Meshy result task id");
    job = { ...job, status: "PENDING", task_id: createdTaskId, updated_at: now().toISOString() };
    await atomicJson(jobPath, job);
  }

  const deadline = Date.now() + timeoutMs;
  let task;
  for (;;) {
    task = await apiJson(fetchImpl, `${MESHY_BASE_URL}${plan.task_endpoint}/${job.task_id}`, { apiKey: config.apiKey });
    const status = String(task.status || "").toUpperCase();
    job = { ...job, status: status || "UNKNOWN", progress: task.progress ?? null, updated_at: now().toISOString() };
    await atomicJson(jobPath, job);
    if (TERMINAL.has(status)) break;
    if (Date.now() >= deadline) throw new Error(`Meshy task ${job.task_id} did not finish within ${Math.round(timeoutMs / 1000)} seconds; rerun the same command to resume without a new charge`);
    await sleep(pollIntervalMs);
  }
  if (job.status !== "SUCCEEDED") {
    const message = task?.task_error?.message || `task ended with ${job.status}`;
    throw new Error(`Meshy task ${job.task_id} failed: ${message}`);
  }
  const glbUrl = task?.model_urls?.glb;
  if (!glbUrl) throw new Error(`Meshy task ${job.task_id} succeeded without a GLB URL`);
  const glb = await downloadGlb(fetchImpl, glbUrl);
  await mkdir(runDir, { recursive: true });
  const outputTemp = `${outputPath}.${process.pid}.tmp`;
  await writeFile(outputTemp, glb);
  await rename(outputTemp, outputPath);

  let balanceAfter = null;
  try {
    balanceAfter = (await getBalance({ root, env, fetchImpl })).balance;
  } catch {
    // The paid result is already safe locally. A transient balance failure must not discard it.
  }
  const provenance = {
    schema: MESHY_PROVENANCE_SCHEMA,
    provider: "Meshy",
    origin: "ai",
    source_page: "https://www.meshy.ai/",
    license_review: "required-before-promotion",
    created_at: now().toISOString(),
    kind,
    profile: plan.profile,
    task_id: job.task_id,
    request_fingerprint: plan.fingerprint,
    request: plan.request,
    input: plan.input,
    cost: {
      estimated_credits: plan.estimated_credits,
      balance_before: balanceBefore,
      balance_after: balanceAfter,
      pricing_source: PRICING_SOURCE,
      pricing_verified_at: PRICING_VERIFIED_AT,
    },
    output: { file: outputPath, format: "glb", bytes: glb.length, sha256: sha256Hex(glb) },
  };
  await atomicJson(provenancePath, provenance);
  job = { ...job, status: "SUCCEEDED", output: outputPath, provenance: provenancePath, balance_after: balanceAfter, updated_at: now().toISOString() };
  await atomicJson(jobPath, job);
  return { cached: false, resumed: resumedExistingTask, task_id: job.task_id, run_dir: runDir, model: outputPath, provenance: provenancePath, plan: printablePlan(plan) };
}

export function printablePlan(plan) {
  const { payload: _payload, ...safe } = plan;
  return safe;
}
