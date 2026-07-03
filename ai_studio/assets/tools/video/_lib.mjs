// Shared plumbing for the Track B video-animation pipeline
// (ai_studio/assets/tools/video/*). One place that owns the environment-coupled
// glue every stage needs: repo-root + studio-config resolution, the two Python
// interpreters this pipeline straddles (the repo `.venv` for in-repo tools, and
// the isolated experiment's interpreters for ComfyUI/CorridorKey), a tiny argv
// parser, run-folder naming, JSON provenance writers, and a spawn wrapper.
//
// LAWS carried from the image tools (lead, 2026-07-02): NO silent fallbacks. An
// interpreter or a tool that is missing is a LOUD error naming the exact fix.
// Every stage output carries provenance. This module never autostarts ComfyUI
// and never installs anything — v1 is orchestration only.
//
// Interpreter map (why three):
//   - repo `.venv` (studio.config pythonPath): in-repo tools that must not
//     depend on the experiment (sheet packer, key_matte matte path, the
//     CorridorKey alpha-hint prep — numpy/PIL/scipy).
//   - experiment embedded python (ComfyUI portable): owns PyAV, used for frame
//     extraction (the repo venv deliberately has no heavy video deps).
//   - CorridorKey venv (uv-managed, torch cu128 + cv2/OpenEXR): the neural
//     matte + its EXR->RGBA conversion.
import { spawn } from "node:child_process";
import { existsSync, mkdirSync, rmSync, writeFileSync } from "node:fs";
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";

import {
  STUDIO_CONFIG_SCHEMA,
  loadStudioConfig,
  videoGenRoot,
} from "../../../core_harness/tool_lib/studio_config.mjs";

export { videoGenRoot };

// _lib.mjs lives at ai_studio/assets/tools/video/ -> 4 up is the repo root.
const LIB_DIR = fileURLToPath(new URL(".", import.meta.url));
export const REPO_ROOT = resolve(LIB_DIR, "..", "..", "..", "..");

// The hardened prompt PREFIX proven in T0257 phase-3 R1 (+0.67-0.84 identity
// score, no photoreal collapse). Track B ALWAYS prepends it to the motion text;
// it is baked into the committed workflow node 8 too, but the generate stage
// rebuilds node 8 from prefix + motion so a caller's motion text can never drop
// it. The matching photoreal NEGATIVE lives in the workflow node 9 and is left
// untouched.
export const HARDENED_PREFIX =
  "2d game art, flat colors, hand-drawn illustration, no photorealism";

export function looksAbsolute(value) {
  return /^([a-zA-Z]:[\\/]|[\\/])/.test(String(value || ""));
}

// Absolute path to the studio (repo) Python interpreter, resolved from studio
// config `pythonPath` ONLY — same no-fallback contract the image _bridge uses.
// A missing config value or a configured interpreter absent on disk is a hard
// error naming the one-shot setup command.
export function resolveRepoPython(root = REPO_ROOT) {
  const raw = String(loadStudioConfig(root).pythonPath || "").trim();
  if (!raw) {
    throw new Error(
      `studio config is missing pythonPath (schema ${STUDIO_CONFIG_SCHEMA}); ` +
        `create the studio venv: node ai_studio/assets/tools/image/_bridge/setup_python.mjs`,
    );
  }
  const abs = looksAbsolute(raw) ? resolve(raw) : resolve(root, raw);
  if (!existsSync(abs)) {
    throw new Error(
      `repo Python interpreter not found at ${abs}; ` +
        `create the studio venv: node ai_studio/assets/tools/image/_bridge/setup_python.mjs`,
    );
  }
  return abs;
}

// --- Isolated-experiment paths (all under videoGenRoot; machine-local) --------

export function comfyPortableDir(root = REPO_ROOT) {
  return resolve(videoGenRoot(root), "ComfyUI_windows_portable");
}
export function comfyDir(root = REPO_ROOT) {
  return resolve(comfyPortableDir(root), "ComfyUI");
}
export function comfyInputDir(root = REPO_ROOT) {
  return resolve(comfyDir(root), "input");
}
export function comfyOutputDir(root = REPO_ROOT) {
  return resolve(comfyDir(root), "output");
}

// The ComfyUI portable embedded Python — owns PyAV (frame extraction). Launched
// with -s by the server; we invoke it the same way for import parity.
export function embeddedPython(root = REPO_ROOT) {
  const abs = resolve(comfyPortableDir(root), "python_embeded", "python.exe");
  if (!existsSync(abs)) {
    throw new Error(
      `ComfyUI embedded Python not found at ${abs}; the video-gen experiment is not installed. ` +
        `See ${videoGenRoot(root)}\\README.md (portable nvidia_cu126 install).`,
    );
  }
  return abs;
}

export function corridorKeyDir(root = REPO_ROOT) {
  return resolve(videoGenRoot(root), "tools", "CorridorKey");
}
// CorridorKey's uv-managed venv interpreter. Invoked DIRECTLY (not via `uv run`)
// on purpose: `uv run` re-syncs the pyproject and can strip the cuda extra (the
// T0261 lesson). A missing venv is a LOUD error pointing at the install script.
export function corridorKeyPython(root = REPO_ROOT) {
  const abs = resolve(corridorKeyDir(root), ".venv", "Scripts", "python.exe");
  if (!existsSync(abs)) {
    throw new Error(
      `CorridorKey venv not found at ${abs}; install it by running ` +
        `"${resolve(corridorKeyDir(root), "Install_CorridorKey_Windows.bat")}" ` +
        `(or "uv sync --extra cuda" inside ${corridorKeyDir(root)}). ` +
        `CorridorKey is CC-BY-NC-SA-4.0 (asset-processing carve-out).`,
    );
  }
  return abs;
}

// --- tiny helpers -------------------------------------------------------------

// Minimal "--flag value" / "--bool" argv parser. Flags without a following
// value (next token is another --flag or end) become boolean true.
export function parseArgs(argv) {
  const out = {};
  for (let i = 0; i < argv.length; i += 1) {
    const tok = argv[i];
    if (!tok.startsWith("--")) continue;
    const key = tok.slice(2);
    const next = argv[i + 1];
    if (next === undefined || next.startsWith("--")) {
      out[key] = true;
    } else {
      out[key] = next;
      i += 1;
    }
  }
  return out;
}

export function sanitizeSlug(value, fallback = "run") {
  return (
    String(value || fallback)
      .trim()
      .replace(/[^a-zA-Z0-9_.-]+/g, "_")
      .replace(/^_+|_+$/g, "")
      .slice(0, 80) || fallback
  );
}

// UTC compact stamp for run-folder names: 20260704T131507Z.
export function utcStamp(date = new Date()) {
  return date.toISOString().replace(/[-:]/g, "").replace(/\.\d+Z$/, "Z");
}

export function ensureDir(dir) {
  mkdirSync(dir, { recursive: true });
  return dir;
}

export function resetDir(dir) {
  rmSync(dir, { recursive: true, force: true });
  mkdirSync(dir, { recursive: true });
  return dir;
}

export function writeJson(path, obj) {
  writeFileSync(path, JSON.stringify(obj, null, 2) + "\n", "utf8");
  return path;
}

// Spawn a child, stream its stdout/stderr to the console (unless quiet), and
// resolve {code, stdout, stderr}. Rejects only on spawn failure; a nonzero exit
// resolves with the code so callers raise a LOUD, contextual error themselves.
export function runProcess(command, args, { cwd, env, quiet = false } = {}) {
  return new Promise((resolvePromise, rejectPromise) => {
    let child;
    try {
      child = spawn(command, args, {
        cwd,
        env: { ...process.env, ...(env || {}) },
        windowsHide: true,
      });
    } catch (error) {
      rejectPromise(error);
      return;
    }
    let stdout = "";
    let stderr = "";
    child.stdout.on("data", (chunk) => {
      stdout += chunk;
      if (!quiet) process.stdout.write(chunk);
    });
    child.stderr.on("data", (chunk) => {
      stderr += chunk;
      if (!quiet) process.stderr.write(chunk);
    });
    child.on("error", rejectPromise);
    child.on("close", (code) => resolvePromise({ code, stdout, stderr }));
  });
}

// Run a repo-venv Python script, LOUD on nonzero exit or missing-module. Mirrors
// the image _bridge's actionable "reinstall the venv" message.
export async function runRepoPython(scriptAbs, args, { root = REPO_ROOT, cwd, env } = {}) {
  const python = resolveRepoPython(root);
  const { code, stdout, stderr } = await runProcess(python, [scriptAbs, ...args], { cwd, env });
  if (code !== 0) {
    const detail = (stderr || stdout || "").trim();
    if (/ModuleNotFoundError|No module named|ImportError/.test(detail)) {
      throw new Error(
        `${detail}\nMissing Python dependency in the studio venv (${python}); ` +
          `reinstall: node ai_studio/assets/tools/image/_bridge/setup_python.mjs`,
      );
    }
    throw new Error(`repo python failed (exit ${code}) for ${scriptAbs}:\n${detail}`);
  }
  return stdout;
}
