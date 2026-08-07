import { closeSync, existsSync, openSync } from "node:fs";
import { mkdir, readFile, rename, stat, writeFile } from "node:fs/promises";
import { dirname, extname, join, resolve } from "node:path";
import { execFile, spawn } from "node:child_process";
import { fileURLToPath } from "node:url";
import { delimiter } from "node:path";

import { loadStudioConfig } from "../../../../config.mjs";
import { sha256File, sha256Hex } from "../../../../core_harness/tool_lib/hash.mjs";

export const HUNYUAN_PROVENANCE_SCHEMA = "ai_studio.asset.hunyuan3d_generation.v1";
export const HUNYUAN_SOURCE = "https://github.com/Tencent-Hunyuan/Hunyuan3D-2";
export const HUNYUAN_LICENSE = "https://github.com/Tencent-Hunyuan/Hunyuan3D-2/blob/main/LICENSE";

const IMAGE_MIME = new Map([
  [".png", "image/png"],
  [".jpg", "image/jpeg"],
  [".jpeg", "image/jpeg"],
]);
const RUNNER_PATH = fileURLToPath(new URL("./runner.py", import.meta.url));

function requiredText(value, label) {
  const text = typeof value === "string" ? value.trim() : "";
  if (!text) throw new Error(`${label} is required`);
  return text;
}

function integer(value, fallback, label, min, max) {
  const number = value === undefined || value === null || value === "" ? fallback : Number(value);
  if (!Number.isInteger(number) || number < min || number > max) {
    throw new Error(`${label} must be an integer from ${min} to ${max}`);
  }
  return number;
}

function finiteNumber(value, fallback, label, min, max) {
  const number = value === undefined || value === null || value === "" ? fallback : Number(value);
  if (!Number.isFinite(number) || number < min || number > max) {
    throw new Error(`${label} must be a number from ${min} to ${max}`);
  }
  return number;
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

function defaultHome(env = process.env) {
  const local = String(env.LOCALAPPDATA || "").trim();
  return local ? join(local, "AIStudio", "tools", "Hunyuan3DLocal", "Hunyuan3D-2") : "";
}

function loopbackUrl(value) {
  let url;
  try { url = new URL(value); } catch { throw new Error(`invalid hunyuan3dUrl: ${value}`); }
  if (!["http:", "https:"].includes(url.protocol)) throw new Error("hunyuan3dUrl must use http or https");
  if (!["127.0.0.1", "localhost", "::1", "[::1]"].includes(url.hostname)) {
    throw new Error("hunyuan3dUrl must stay on loopback; the local adapter will not upload source images to a remote host");
  }
  return url.href.replace(/\/$/, "");
}

export function loadHunyuanConfig(root = process.cwd(), env = process.env) {
  const config = loadStudioConfig(root);
  const configuredRoot = env.HUNYUAN3D_WORK_ROOT || config.hunyuan3dWorkRoot;
  if (!configuredRoot) throw new Error("missing hunyuan3dWorkRoot in Studio config (or HUNYUAN3D_WORK_ROOT)");
  const configuredHome = String(env.HUNYUAN3D_HOME || config.hunyuan3dHome || defaultHome(env)).trim();
  if (!configuredHome) throw new Error("missing HUNYUAN3D_HOME and LOCALAPPDATA is unavailable");
  const home = resolve(configuredHome);
  return {
    home,
    workRoot: resolve(root, configuredRoot),
    apiUrl: loopbackUrl(env.HUNYUAN3D_URL || config.hunyuan3dUrl || "http://127.0.0.1:8081"),
  };
}

export async function buildPlan(kind, options = {}) {
  if (kind === "texture") {
    const meshPath = resolve(requiredText(options.mesh, "--mesh"));
    if (extname(meshPath).toLowerCase() !== ".glb") throw new Error("--mesh must be a .glb file");
    if (!existsSync(meshPath)) throw new Error(`mesh not found: ${meshPath}`);
    const meshBytes = await readFile(meshPath);
    if (!isGlb(meshBytes)) throw new Error("--mesh must be a valid GLB 2.0 file");
    const imagePath = resolve(requiredText(options.image, "--image"));
    const mime = IMAGE_MIME.get(extname(imagePath).toLowerCase());
    if (!mime) throw new Error("--image must be a .png, .jpg, or .jpeg file");
    if (!existsSync(imagePath)) throw new Error(`image not found: ${imagePath}`);
    const imageBytes = await stat(imagePath);
    const input = {
      mesh: { path: meshPath, bytes: meshBytes.length, sha256: sha256Hex(meshBytes) },
      image: { path: imagePath, mime, bytes: imageBytes.size, sha256: await sha256File(imagePath) },
    };
    const request = { low_vram: true, vae_slicing: true, texture_size: 2048 };
    return {
      schema: "ai_studio.asset.hunyuan3d_plan.v1",
      provider: "hunyuan3d-local",
      model: "tencent/Hunyuan3D-2/hunyuan3d-paint-v2-0-turbo",
      kind,
      request,
      input,
      estimated_credits: 0,
      fingerprint: fingerprintOf({ kind, provider: "hunyuan3d-paint-v2-0-turbo", request, input }),
    };
  }
  if (kind === "multiview") {
    const directory = resolve(requiredText(options.views, "--views"));
    let directoryStat;
    try { directoryStat = await stat(directory); } catch { throw new Error(`views folder not found: ${directory}`); }
    if (!directoryStat.isDirectory()) throw new Error(`--views must be a folder: ${directory}`);
    const views = [];
    for (const name of ["front", "back", "left", "right"]) {
      const path = join(directory, `${name}.png`);
      if (!existsSync(path)) continue;
      const bytes = await stat(path);
      views.push({ name, path, mime: "image/png", bytes: bytes.size, sha256: await sha256File(path) });
    }
    if (views[0]?.name !== "front" || views.length < 2) {
      throw new Error("--views must contain front.png and at least one of back.png, left.png, or right.png");
    }
    const input = { directory, views };
    const request = {
      seed: integer(options.seed, 1234, "--seed", 0, 2_147_483_647),
      octree_resolution: integer(options.octreeResolution, 256, "--octree-resolution", 64, 512),
      num_inference_steps: integer(options.steps, 30, "--steps", 1, 100),
      num_chunks: integer(options.numChunks, 20_000, "--num-chunks", 1_000, 100_000),
    };
    return {
      schema: "ai_studio.asset.hunyuan3d_plan.v1",
      provider: "hunyuan3d-local",
      model: "tencent/Hunyuan3D-2mv/hunyuan3d-dit-v2-mv",
      kind,
      request,
      input,
      estimated_credits: 0,
      fingerprint: fingerprintOf({ kind, provider: "hunyuan3d-2mv", request, input }),
    };
  }
  if (kind !== "image") throw new Error(`kind must be image, multiview, or texture (got ${JSON.stringify(kind)})`);
  const imagePath = resolve(requiredText(options.image, "--image"));
  const mime = IMAGE_MIME.get(extname(imagePath).toLowerCase());
  if (!mime) throw new Error("--image must be a .png, .jpg, or .jpeg file");
  if (!existsSync(imagePath)) throw new Error(`image not found: ${imagePath}`);
  const bytes = await readFile(imagePath);
  const input = {
    path: imagePath,
    mime,
    bytes: bytes.length,
    sha256: await sha256File(imagePath),
  };
  const request = {
    seed: integer(options.seed, 1234, "--seed", 0, 2_147_483_647),
    octree_resolution: integer(options.octreeResolution, 128, "--octree-resolution", 64, 512),
    num_inference_steps: integer(options.steps, 5, "--steps", 1, 100),
    guidance_scale: finiteNumber(options.guidanceScale, 5, "--guidance-scale", 0, 50),
  };
  const fingerprint = fingerprintOf({ kind, provider: "hunyuan3d-2mini-turbo", request, input });
  return {
    schema: "ai_studio.asset.hunyuan3d_plan.v1",
    provider: "hunyuan3d-local",
    model: "tencent/Hunyuan3D-2mini/hunyuan3d-dit-v2-mini-turbo",
    kind,
    request,
    input,
    estimated_credits: 0,
    fingerprint,
  };
}

async function atomicJson(path, value) {
  await mkdir(dirname(path), { recursive: true });
  const temp = `${path}.${process.pid}.tmp`;
  await writeFile(temp, `${JSON.stringify(value, null, 2)}\n`, "utf8");
  await rename(temp, path);
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

function hasEmbeddedTexture(buffer) {
  if (!isGlb(buffer) || buffer.length < 20) return false;
  let document = null;
  let binaryLength = 0;
  let offset = 12;
  try {
    while (offset < buffer.length) {
      if (offset + 8 > buffer.length) return false;
      const chunkLength = buffer.readUInt32LE(offset);
      const chunkType = buffer.readUInt32LE(offset + 4);
      const chunkStart = offset + 8;
      const chunkEnd = chunkStart + chunkLength;
      if (chunkEnd > buffer.length) return false;
      if (chunkType === 0x4e4f534a && document === null) {
        document = JSON.parse(buffer.subarray(chunkStart, chunkEnd).toString("utf8").replace(/\0+$/, "").trim());
      } else if (chunkType === 0x004e4942 && binaryLength === 0) {
        binaryLength = chunkLength;
      }
      offset = chunkEnd;
    }
  } catch {
    return false;
  }
  if (offset !== buffer.length || !document) return false;
  const materials = Array.isArray(document.materials) ? document.materials : [];
  const textures = Array.isArray(document.textures) ? document.textures : [];
  const images = Array.isArray(document.images) ? document.images : [];
  const views = Array.isArray(document.bufferViews) ? document.bufferViews : [];
  const buffers = Array.isArray(document.buffers) ? document.buffers : [];
  const textureIndices = [];
  for (const material of materials) {
    if (!material || typeof material !== "object") continue;
    const pbr = material.pbrMetallicRoughness;
    for (const reference of [
      pbr?.baseColorTexture,
      pbr?.metallicRoughnessTexture,
      material.normalTexture,
      material.occlusionTexture,
      material.emissiveTexture,
    ]) {
      if (Number.isInteger(reference?.index)) textureIndices.push(reference.index);
    }
  }
  return textureIndices.some((textureIndex) => {
    const texture = textures[textureIndex];
    const image = Number.isInteger(texture?.source) ? images[texture.source] : null;
    if (!image || typeof image !== "object") return false;
    if (typeof image.uri === "string") {
      return /^data:image\/[a-z0-9.+-]+;base64,[a-z0-9+/]+=*$/i.test(image.uri);
    }
    if (!Number.isInteger(image.bufferView) || binaryLength <= 0) return false;
    const view = views[image.bufferView];
    if (!view || view.buffer !== 0 || !Number.isInteger(view.byteLength) || view.byteLength <= 0) return false;
    const byteOffset = view.byteOffset ?? 0;
    if (!Number.isInteger(byteOffset) || byteOffset < 0 || byteOffset + view.byteLength > binaryLength) return false;
    return Number.isInteger(buffers[0]?.byteLength)
      && buffers[0].byteLength >= byteOffset + view.byteLength
      && typeof image.mimeType === "string"
      && image.mimeType.startsWith("image/");
  });
}

async function fetchWithTimeout(fetchImpl, url, options, timeoutMs) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), timeoutMs);
  try { return await fetchImpl(url, { ...options, signal: controller.signal }); }
  finally { clearTimeout(timer); }
}

function execFileAsync(execFileImpl, file, args, options) {
  return new Promise((resolvePromise, rejectPromise) => {
    execFileImpl(file, args, options, (error, stdout, stderr) => {
      if (error) {
        error.stdout = stdout;
        error.stderr = stderr;
        rejectPromise(error);
        return;
      }
      resolvePromise({ stdout, stderr });
    });
  });
}

export async function doctor({ root = process.cwd(), env = process.env, fetchImpl = fetch, timeoutMs = 2_000 } = {}) {
  const config = loadHunyuanConfig(root, env);
  const python = join(config.home, ".venv", "Scripts", "python.exe");
  const server = join(config.home, "api_server.py");
  let api = { reachable: false, error: "not checked" };
  try {
    const response = await fetchWithTimeout(fetchImpl, `${config.apiUrl}/openapi.json`, { method: "GET" }, timeoutMs);
    api = { reachable: response.ok, status: response.status, ...(response.ok ? {} : { error: response.statusText || `HTTP ${response.status}` }) };
  } catch (error) {
    api = { reachable: false, error: error.name === "AbortError" ? "timeout" : error.message };
  }
  return {
    schema: "ai_studio.asset.hunyuan3d_doctor.v1",
    ok: existsSync(python) && existsSync(server) && api.reachable,
    home: config.home,
    install: { python, python_exists: existsSync(python), server, server_exists: existsSync(server) },
    api: { url: config.apiUrl, ...api },
    work_root: config.workRoot,
  };
}

export async function startServer(options = {}, dependencies = {}) {
  const root = dependencies.root || process.cwd();
  const env = dependencies.env || process.env;
  const fetchImpl = dependencies.fetchImpl || fetch;
  const spawnImpl = dependencies.spawnImpl || spawn;
  const current = await doctor({ root, env, fetchImpl, timeoutMs: dependencies.timeoutMs ?? 500 });
  if (current.api.reachable) return { already_running: true, pid: null, doctor: current };
  if (!current.install.python_exists || !current.install.server_exists) {
    throw new Error(`Hunyuan3D install is incomplete at ${current.home}`);
  }
  const config = loadHunyuanConfig(root, env);
  const parsed = new URL(config.apiUrl);
  const port = parsed.port || (parsed.protocol === "https:" ? "443" : "80");
  await mkdir(config.workRoot, { recursive: true });
  const stdoutPath = join(config.workRoot, "server.stdout.log");
  const stderrPath = join(config.workRoot, "server.stderr.log");
  const stdout = openSync(stdoutPath, "a");
  const stderr = openSync(stderrPath, "a");
  let child;
  try {
    child = spawnImpl(current.install.python, [
      "-u", current.install.server,
      "--host", parsed.hostname,
      "--port", port,
      "--limit-model-concurrency", "1",
    ], {
      cwd: config.home,
      detached: true,
      windowsHide: true,
      stdio: ["ignore", stdout, stderr],
      env: {
        ...env,
        HF_HOME: join(dirname(config.home), "hf_cache"),
        HUGGINGFACE_HUB_CACHE: join(dirname(config.home), "hf_cache", "hub"),
        HF_HUB_DISABLE_SYMLINKS_WARNING: "1",
        XFORMERS_FORCE_DISABLE_TRITON: "1",
        PYTHONUNBUFFERED: "1",
      },
    });
  } finally {
    closeSync(stdout);
    closeSync(stderr);
  }
  child.unref?.();
  return { already_running: false, pid: child.pid, api_url: config.apiUrl, stdout: stdoutPath, stderr: stderrPath };
}

export async function runGeneration(kind, options = {}, dependencies = {}) {
  if (options.execute !== true) throw new Error("local generation blocked: pass --execute after reviewing the plan");
  const root = dependencies.root || process.cwd();
  const env = dependencies.env || process.env;
  const fetchImpl = dependencies.fetchImpl || fetch;
  const now = dependencies.now || (() => new Date());
  const config = loadHunyuanConfig(root, env);
  const plan = await buildPlan(kind, options);
  const runDir = join(config.workRoot, plan.fingerprint);
  const outputPath = join(runDir, "model.glb");
  const provenancePath = join(runDir, "provenance.json");
  if (existsSync(outputPath) && existsSync(provenancePath)) {
    return { cached: true, run_dir: runDir, model: outputPath, provenance: provenancePath, plan };
  }
  if (kind === "multiview" || kind === "texture") {
    const isTexture = kind === "texture";
    const python = join(config.home, ".venv", "Scripts", "python.exe");
    if (!existsSync(python)) throw new Error(`Hunyuan3D Python not found: ${python}`);
    await mkdir(runDir, { recursive: true });
    const temporaryOutput = join(runDir, `model.${process.pid}.tmp.glb`);
    const timeoutMs = integer(options.timeoutSeconds, isTexture ? 3_600 : 1_800, "--timeout-seconds", 10, 7_200) * 1_000;
    const args = isTexture
      ? [
          RUNNER_PATH, "texture",
          "--mesh", plan.input.mesh.path,
          "--image", plan.input.image.path,
          "--output", temporaryOutput,
        ]
      : [
          RUNNER_PATH, "multiview",
          "--views", plan.input.directory,
          "--output", temporaryOutput,
          "--seed", String(plan.request.seed),
          "--steps", String(plan.request.num_inference_steps),
          "--octree-resolution", String(plan.request.octree_resolution),
          "--num-chunks", String(plan.request.num_chunks),
        ];
    try {
      await execFileAsync(dependencies.execFileImpl || execFile, python, args, {
        cwd: config.home,
        timeout: timeoutMs,
        maxBuffer: 10 * 1024 * 1024,
        windowsHide: true,
        env: {
          ...env,
          PYTHONPATH: [config.home, env.PYTHONPATH].filter(Boolean).join(delimiter),
          HF_HOME: join(dirname(config.home), "hf_cache"),
          HUGGINGFACE_HUB_CACHE: join(dirname(config.home), "hf_cache", "hub"),
          HF_HUB_OFFLINE: "1",
          TRANSFORMERS_OFFLINE: "1",
          HF_HUB_DISABLE_SYMLINKS_WARNING: "1",
          PYTORCH_CUDA_ALLOC_CONF: "expandable_segments:True",
        },
      });
    } catch (error) {
      const detail = String(error.stderr || error.stdout || error.message).trim().slice(-2_000);
      throw new Error(`Hunyuan3D ${kind} runner failed: ${detail}`);
    }
    const glb = await readFile(temporaryOutput);
    if (!isGlb(glb)) throw new Error(`Hunyuan3D ${kind} runner did not produce a valid GLB 2.0 file`);
    if (isTexture && !hasEmbeddedTexture(glb)) {
      throw new Error("Hunyuan3D texture runner output does not contain an embedded material, texture, and image");
    }
    await rename(temporaryOutput, outputPath);
    const provenance = {
      schema: HUNYUAN_PROVENANCE_SCHEMA,
      provider: "Tencent Hunyuan3D 2.0 (local)",
      origin: "ai",
      source_page: HUNYUAN_SOURCE,
      model: plan.model,
      license_url: HUNYUAN_LICENSE,
      license_review: "required-before-promotion",
      created_at: now().toISOString(),
      request_fingerprint: plan.fingerprint,
      request: plan.request,
      input: plan.input,
      output: { file: outputPath, format: "glb", bytes: glb.length, sha256: sha256Hex(glb), textured: isTexture },
    };
    await atomicJson(provenancePath, provenance);
    return { cached: false, run_dir: runDir, model: outputPath, provenance: provenancePath, plan };
  }
  const image = await readFile(plan.input.path);
  const payload = { image: image.toString("base64"), ...plan.request };
  const timeoutMs = integer(options.timeoutSeconds, 900, "--timeout-seconds", 10, 7_200) * 1_000;
  let response;
  try {
    response = await fetchWithTimeout(fetchImpl, `${config.apiUrl}/generate`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify(payload),
    }, timeoutMs);
  } catch (error) {
    throw new Error(`Hunyuan3D request failed: ${error.name === "AbortError" ? `timed out after ${timeoutMs / 1000}s` : error.message}`);
  }
  if (!response.ok) {
    const text = await response.text();
    throw new Error(`Hunyuan3D HTTP ${response.status}: ${text.slice(0, 500) || response.statusText || "request failed"}`);
  }
  const glb = Buffer.from(await response.arrayBuffer());
  if (!isGlb(glb)) throw new Error("Hunyuan3D returned a response that is not a valid GLB 2.0 file");
  await mkdir(runDir, { recursive: true });
  const temp = `${outputPath}.${process.pid}.tmp`;
  await writeFile(temp, glb);
  await rename(temp, outputPath);
  const provenance = {
    schema: HUNYUAN_PROVENANCE_SCHEMA,
    provider: "Tencent Hunyuan3D 2.0 (local)",
    origin: "ai",
    source_page: HUNYUAN_SOURCE,
    model: plan.model,
    license_url: HUNYUAN_LICENSE,
    license_review: "required-before-promotion",
    created_at: now().toISOString(),
    request_fingerprint: plan.fingerprint,
    request: plan.request,
    input: plan.input,
    output: { file: outputPath, format: "glb", bytes: glb.length, sha256: sha256Hex(glb), textured: false },
  };
  await atomicJson(provenancePath, provenance);
  return { cached: false, run_dir: runDir, model: outputPath, provenance: provenancePath, plan };
}
