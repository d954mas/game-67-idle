#!/usr/bin/env node
import { existsSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const venvPython = join(repoRoot, ".venv", process.platform === "win32" ? "Scripts/python.exe" : "bin/python");
const lockPath = join(repoRoot, "ai_studio", "python", "requirements.lock.txt");

function valueAfter(argv, flag) {
  const index = argv.indexOf(flag);
  return index >= 0 ? argv[index + 1] : "";
}

function run(command, args) {
  const result = spawnSync(command, args, { cwd: repoRoot, stdio: "inherit" });
  if (result.error || result.status !== 0) throw new Error(`command failed: ${command} ${args.join(" ")}`);
}

const basePython = valueAfter(process.argv.slice(2), "--base-python") || process.env.AI_STUDIO_BASE_PYTHON || "";
if (!existsSync(venvPython)) {
  if (!basePython || !existsSync(basePython)) {
    throw new Error("root .venv is missing; pass --base-python <absolute Python 3.12 executable> after repairing the user-level Python install");
  }
  run(resolve(basePython), ["-c", "import sys; raise SystemExit(0 if sys.version_info[:2] == (3, 12) else 1)"]);
  run(resolve(basePython), ["-m", "venv", ".venv"]);
}
run(venvPython, ["-m", "pip", "install", "-r", lockPath]);
run(process.execPath, ["ai_studio/dev_environment/python_check.mjs"]);
