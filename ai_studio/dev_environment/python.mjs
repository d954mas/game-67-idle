import { existsSync, realpathSync, statSync } from "node:fs";
import { isAbsolute, join, posix, resolve, win32 } from "node:path";

import { STUDIO_CONFIG_SCHEMA, loadStudioConfig } from "../config.mjs";

function comparablePath(value) {
  const text = resolve(value).replace(/\\/g, "/");
  return process.platform === "win32" ? text.toLowerCase() : text;
}

// A drive root (a subst workspace mount such as `X:\`) already ends in a
// separator, so appending one to it as a prefix would reject every child.
export function pathInside(child, parent, platform = process.platform) {
  const api = platform === "win32" ? win32 : posix;
  const rel = api.relative(parent, child);
  return rel !== "" && rel !== ".." && !rel.startsWith(`..${api.sep}`) && !api.isAbsolute(rel);
}

function looksAbsolute(value) {
  return isAbsolute(value) || /^[a-zA-Z]:[\\/]/.test(value);
}

export function studioPythonPath(root, platform = process.platform) {
  const raw = String(loadStudioConfig(root).pythonPath || "").trim();
  if (!raw) throw new Error(`studio config is missing pythonPath (schema ${STUDIO_CONFIG_SCHEMA})`);
  const venvRoot = resolve(root, ".venv");
  const configured = looksAbsolute(raw) ? resolve(raw) : resolve(root, raw);
  if (comparablePath(configured) !== comparablePath(venvRoot)) {
    throw new Error(`studio pythonPath must name the repository root .venv: ${configured}`);
  }
  const python = join(venvRoot, platform === "win32" ? "Scripts/python.exe" : "bin/python");
  const comparablePython = comparablePath(python);
  const comparableVenv = comparablePath(venvRoot);
  if (!comparablePython.startsWith(`${comparableVenv}/`)) {
    throw new Error(`studio pythonPath must resolve inside the repository root .venv: ${python}`);
  }
  if (!existsSync(join(venvRoot, "pyvenv.cfg")) || !existsSync(python) || !statSync(python).isFile()) {
    throw new Error(`studio Python interpreter not found at ${python}; run node ai_studio/dev_environment/python_setup.mjs`);
  }
  if (!pathInside(realpathSync(venvRoot), realpathSync(root))) {
    throw new Error(`studio pythonPath resolves outside the repository: ${venvRoot}`);
  }
  return python;
}
