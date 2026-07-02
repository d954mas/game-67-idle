#!/usr/bin/env node
// One-shot studio Python environment setup for the image asset tools.
//
// Creates a repo-local, gitignored `.venv/` from `py -3.12` and installs the
// pinned deps in `ai_studio/assets/tools/image/requirements.txt`. The image tools
// resolve their interpreter from `ai_studio/studio.config.json` -> `pythonPath`
// (`.venv/Scripts/python.exe`) ONLY, so this script is the single supported way
// to make those tools runnable on a fresh checkout.
//
// LAW (lead, 2026-07-02): missing venv/dep => loud error naming the fix. This is
// that fix. Run it from the repository root:
//   node ai_studio/assets/tools/image/_bridge/setup_python.mjs
//
// Avast Web Shield does TLS MITM on this box and can break pip's certificate
// chain; if a plain install fails with an SSL/TLS error we retry once with
// pip's --trusted-host escape hatch for pypi.
import { execFileSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const scriptDir = fileURLToPath(new URL(".", import.meta.url));
// _bridge -> image -> tools -> assets -> ai_studio -> repo root
const repoRoot = resolve(scriptDir, "..", "..", "..", "..", "..");
const venvDir = join(repoRoot, ".venv");
const venvPython = join(venvDir, "Scripts", "python.exe");
const requirements = join(repoRoot, "ai_studio", "assets", "tools", "image", "requirements.txt");

function run(command, args, options = {}) {
  console.log(`> ${command} ${args.join(" ")}`);
  execFileSync(command, args, { cwd: repoRoot, stdio: "inherit", ...options });
}

function pipInstall() {
  try {
    run(venvPython, ["-m", "pip", "install", "--upgrade", "pip"]);
    run(venvPython, ["-m", "pip", "install", "-r", requirements]);
  } catch (error) {
    console.warn(`\npip install failed (${error.message}); retrying with trusted hosts (Avast TLS MITM workaround)...\n`);
    const trusted = ["--trusted-host", "pypi.org", "--trusted-host", "files.pythonhosted.org"];
    run(venvPython, ["-m", "pip", "install", ...trusted, "--upgrade", "pip"]);
    run(venvPython, ["-m", "pip", "install", ...trusted, "-r", requirements]);
  }
}

function main() {
  if (!existsSync(requirements)) {
    throw new Error(`requirements file missing: ${requirements}`);
  }
  if (!existsSync(venvPython)) {
    console.log(`Creating studio venv at ${venvDir} ...`);
    run("py", ["-3.12", "-m", "venv", ".venv"]);
  } else {
    console.log(`Reusing existing venv at ${venvDir}`);
  }
  pipInstall();
  console.log("\nVerifying imports ...");
  run(venvPython, ["-c", "import numpy, scipy, PIL; print('image tools venv OK:', numpy.__version__, scipy.__version__, PIL.__version__)"]);
  console.log(`\nDone. Point studio.config.json pythonPath at: .venv/Scripts/python.exe`);
}

main();
