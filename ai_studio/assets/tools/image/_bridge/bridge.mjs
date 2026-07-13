// Shared plumbing for the image asset tools (ai_studio/assets/tools/image/*).
//
// This is the ONE place that owns the security-sensitive and environment-coupled
// glue every per-tool bridge needs: the studio Python interpreter, tmp-path
// confinement, session-directory math, and the small JSON/image helpers. Per-tool
// api.mjs bridges import from here so path-confinement and interpreter resolution
// are never duplicated (and never diverge) across tools.
//
// LAW (lead, 2026-07-02): image tools have NO silent fallbacks. The Python
// interpreter is resolved from studio config ONLY -- no candidate probing, no
// PATH search. A missing venv or a missing dependency is a LOUD error whose
// message names the interpreter and the exact one-shot setup command.
//
// The public HTTP contract for the frozen viewer keeps the historical
// `tmp/ai_studio/assets/raster2d/` prefix (see `namespace` default below); new
// tools may pass a different namespace, but the raster2d endpoints must not move.
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { extname, isAbsolute, join, normalize, relative, resolve, sep } from "node:path";

import { studioPythonPath } from "../../../../core_harness/tool_lib/studio_config.mjs";
import { runPythonScript, shutdownImageWorkers } from "./worker.mjs";

export { shutdownImageWorkers };

const maxBodyBytes = 64 * 1024 * 1024;

// One-shot command that (re)creates the studio venv and installs pinned deps.
// Quoted verbatim in every "missing interpreter/dependency" error so an operator
// can copy-paste the fix.
export const IMAGE_PYTHON_SETUP_COMMAND = "node ai_studio/assets/tools/image/_bridge/setup_python.mjs";

// ---------------------------------------------------------------------------
// Slug + path confinement
// ---------------------------------------------------------------------------

export function safeSlug(value, fallback = "asset") {
  return String(value || fallback)
    .trim()
    .replace(/\.[^.]+$/, "")
    .replace(/[^a-zA-Z0-9_.-]+/g, "_")
    .replace(/^_+|_+$/g, "")
    .slice(0, 90) || fallback;
}

export function safeResolve(base, relativePath) {
  const resolvedBase = resolve(base);
  const full = resolve(resolvedBase, normalize(relativePath));
  if (full !== resolvedBase && !full.startsWith(resolvedBase + sep)) return null;
  return full;
}

export function tmpRoot(root, namespace = "raster2d") {
  return join(root, "tmp", "ai_studio", "assets", namespace);
}

export function ensureInsideTmp(root, pathValue, namespace = "raster2d") {
  if (!pathValue) throw new Error("missing tmp path");
  const resolved = safeResolve(root, pathValue);
  if (!resolved) throw new Error("path must stay inside repository");
  const base = resolve(tmpRoot(root, namespace));
  if (resolved !== base && !resolved.startsWith(base + sep)) {
    throw new Error(`asset tool files must stay under tmp/ai_studio/assets/${namespace}`);
  }
  return resolved;
}

export function sessionDirForPath(root, absPath, namespace = "raster2d") {
  const base = resolve(tmpRoot(root, namespace));
  const rel = relative(base, absPath);
  const [session] = rel.split(sep);
  if (!session || session.startsWith("..")) throw new Error(`invalid ${namespace} session path`);
  return join(base, session);
}

export function workspaceRel(root, absPath) {
  return relative(root, absPath).replaceAll("\\", "/");
}

export function tmpUrl(root, absPath) {
  const relToTmp = relative(join(root, "tmp"), absPath).split(sep).map(encodeURIComponent).join("/");
  return `/tmp/${relToTmp}`;
}

// ---------------------------------------------------------------------------
// HTTP request/response helpers
// ---------------------------------------------------------------------------

export function writeJson(res, status, data) {
  res.writeHead(status, { "content-type": "application/json; charset=utf-8" });
  res.end(JSON.stringify(data, null, 2));
}

export function readJsonBody(req) {
  return new Promise((resolveBody, rejectBody) => {
    let size = 0;
    let data = "";
    req.setEncoding("utf8");
    req.on("data", (chunk) => {
      size += Buffer.byteLength(chunk);
      if (size > maxBodyBytes) {
        rejectBody(new Error("request body too large"));
        req.destroy();
        return;
      }
      data += chunk;
    });
    req.on("end", () => {
      try {
        resolveBody(data ? JSON.parse(data) : {});
      } catch {
        rejectBody(new Error("invalid JSON body"));
      }
    });
    req.on("error", rejectBody);
  });
}

// ---------------------------------------------------------------------------
// Python interpreter (studio venv, config-only) + runner
// ---------------------------------------------------------------------------

function looksAbsolute(value) {
  return isAbsolute(value) || /^[a-zA-Z]:[\/]/.test(value);
}

// Absolute path to the studio Python interpreter, resolved from studio config
// ONLY. There is no PATH search and no candidate chain: a missing `pythonPath`,
// or a configured interpreter that does not exist on disk, is a hard error whose
// message tells the operator exactly how to create the venv.
export function resolvePythonPath(root) {
  return studioPythonPath(root);
}

// Run a Python tool script (args[0] = script path, rest = its argv) through the warm
// worker for the studio interpreter. The worker is spawned lazily and kept warm, so the
// second and later calls skip the interpreter-startup + numpy/PIL import floor; behavior
// is identical to a cold `python script.py <argv>` spawn (same argv/argparse main). A
// missing interpreter, a crashed worker, or a failing script are all LOUD errors — there
// is no silent fallback to a cold spawn (no-fallbacks law). A missing dependency keeps
// its historical, actionable message naming the venv + one-shot setup command.
export async function runPython(root, args) {
  const python = resolvePythonPath(root); // throws loud if the venv/interpreter is missing
  const scriptAbs = looksAbsolute(args[0]) ? resolve(args[0]) : resolve(root, args[0]);
  try {
    const { stdout } = await runPythonScript(root, python, scriptAbs, args.slice(1));
    return stdout;
  } catch (error) {
    const detail = String((error && (error.stderr || error.message)) || "").trim();
    if (/ModuleNotFoundError|No module named|ImportError/.test(detail)) {
      throw new Error(
        `${detail}\nMissing Python dependency in the studio venv (${python}); ` +
          `reinstall: ${IMAGE_PYTHON_SETUP_COMMAND}`,
      );
    }
    throw new Error(detail || (error && error.message) || "python worker failed");
  }
}

// ---------------------------------------------------------------------------
// JSON + image-size helpers
// ---------------------------------------------------------------------------

export function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

export function writeJsonFile(path, data) {
  writeFileSync(path, JSON.stringify(data, null, 2) + "\n", "utf8");
}

export function readImageSize(path) {
  const buffer = readFileSync(path);
  if (
    buffer.length >= 24 &&
    buffer[0] === 0x89 &&
    buffer[1] === 0x50 &&
    buffer[2] === 0x4e &&
    buffer[3] === 0x47
  ) {
    return { width: buffer.readUInt32BE(16), height: buffer.readUInt32BE(20) };
  }
  if (buffer.length >= 10 && buffer.toString("ascii", 0, 3) === "GIF") {
    return { width: buffer.readUInt16LE(6), height: buffer.readUInt16LE(8) };
  }
  if (buffer.length >= 4 && buffer[0] === 0xff && buffer[1] === 0xd8) {
    let offset = 2;
    while (offset + 9 < buffer.length) {
      if (buffer[offset] !== 0xff) break;
      const marker = buffer[offset + 1];
      const length = buffer.readUInt16BE(offset + 2);
      if (length < 2) break;
      if (marker >= 0xc0 && marker <= 0xc3) {
        return { width: buffer.readUInt16BE(offset + 7), height: buffer.readUInt16BE(offset + 5) };
      }
      offset += 2 + length;
    }
  }
  throw new Error("whole image mode supports PNG, GIF, and JPEG dimensions");
}

export function color(value, fallback = "#ff00ff") {
  const text = String(value || fallback).trim();
  return /^#[0-9a-fA-F]{6}$/.test(text) ? text : fallback;
}

export function intOption(value, fallback, max = 100000) {
  const number = Number.parseInt(String(value ?? fallback), 10);
  if (!Number.isFinite(number)) return fallback;
  return Math.max(0, Math.min(max, number));
}

export function decodeDataUrl(dataUrl) {
  const match = /^data:([^;,]+)?;base64,(.+)$/s.exec(String(dataUrl || ""));
  if (!match) throw new Error("source image must be a base64 data URL");
  return {
    mime: match[1] || "application/octet-stream",
    buffer: Buffer.from(match[2], "base64"),
  };
}

export function extensionForUpload(fileName, mime) {
  const ext = extname(String(fileName || "")).toLowerCase();
  if ([".png", ".jpg", ".jpeg", ".webp", ".gif"].includes(ext)) return ext;
  if (mime === "image/jpeg") return ".jpg";
  if (mime === "image/webp") return ".webp";
  if (mime === "image/gif") return ".gif";
  return ".png";
}
