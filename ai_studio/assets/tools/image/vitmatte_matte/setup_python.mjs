#!/usr/bin/env node
// One-shot Python environment setup for the vitmatte_matte image tool.
//
// Mirrors ai_studio/assets/tools/image/_bridge/setup_python.mjs, but creates
// THIS TOOL's OWN venv INSIDE its own folder (vitmatte_matte/.venv/,
// gitignored) instead of the shared repo .venv/. GPU torch (cu128, ~2.7GB)
// must never enter the shared venv every other image tool uses -- that is the
// entire reason this tool gets its own environment.
//
// LAW (lead, 2026-07-02): missing venv/dep => loud error naming the fix. This
// is that fix. Run it from the repository root (or anywhere; paths are
// resolved relative to this script, not cwd):
//   node ai_studio/assets/tools/image/vitmatte_matte/setup_python.mjs
//
// Avast Web Shield does TLS MITM on this box and can break pip's own
// certificate chain; --use-feature=truststore makes pip use the OS trust
// store for its own network requests, and the app itself does the same at
// runtime (truststore.inject_into_ssl(), see vitmatte_matte.py) for its first
// Hugging Face Hub download. If a plain install still fails on an SSL/TLS
// error, retry once with pip's --trusted-host escape hatch.
import { execFileSync } from "node:child_process";
import { existsSync } from "node:fs";
import { join } from "node:path";
import { fileURLToPath } from "node:url";

const toolDir = fileURLToPath(new URL(".", import.meta.url));
const venvDir = join(toolDir, ".venv");
const venvPython = join(venvDir, "Scripts", "python.exe");
const requirements = join(toolDir, "requirements.txt");

function run(command, args, options = {}) {
  console.log(`> ${command} ${args.join(" ")}`);
  execFileSync(command, args, { cwd: toolDir, stdio: "inherit", ...options });
}

function pipInstall() {
  const base = ["-m", "pip", "install", "-r", requirements, "--use-feature=truststore"];
  try {
    run(venvPython, ["-m", "pip", "install", "--upgrade", "pip"]);
    run(venvPython, base);
  } catch (error) {
    console.warn(`\npip install failed (${error.message}); retrying with trusted hosts (Avast TLS MITM workaround)...\n`);
    const trusted = [
      "--trusted-host", "pypi.org",
      "--trusted-host", "files.pythonhosted.org",
      "--trusted-host", "download.pytorch.org",
    ];
    run(venvPython, ["-m", "pip", "install", ...trusted, "--upgrade", "pip"]);
    run(venvPython, [...base, ...trusted]);
  }
}

function main() {
  if (!existsSync(requirements)) {
    throw new Error(`requirements file missing: ${requirements}`);
  }
  if (!existsSync(venvPython)) {
    console.log(`Creating vitmatte_matte's OWN venv at ${venvDir} ...`);
    run("py", ["-3.12", "-m", "venv", ".venv"]);
  } else {
    console.log(`Reusing existing venv at ${venvDir}`);
  }
  pipInstall();

  console.log("\nVerifying imports ...");
  run(venvPython, [
    "-c",
    "import numpy, scipy, PIL, torch, transformers, truststore; " +
      "print('vitmatte_matte venv OK:', " +
      "'numpy', numpy.__version__, 'scipy', scipy.__version__, 'Pillow', PIL.__version__, " +
      "'torch', torch.__version__, 'transformers', transformers.__version__, " +
      "'truststore', truststore.__version__); " +
      "print('cuda available:', torch.cuda.is_available()); " +
      "print('cuda device:', torch.cuda.get_device_name(0) if torch.cuda.is_available() else 'none')",
  ]);
  console.log(
    `\nDone. vitmatte_matte.py / vitmatte_smoke.py resolve THIS venv on their own ` +
      `(no studio.config.json entry) -- invoke them with:\n  ${venvPython}`,
  );
}

main();
