#!/usr/bin/env node
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

import { studioPythonPath } from "../core_harness/tool_lib/studio_config.mjs";

const repoRoot = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const requirementsPath = join(repoRoot, "ai_studio", "python", "requirements.direct.txt");
const venvRoot = join(repoRoot, ".venv");
const pins = readFileSync(requirementsPath, "utf8").split(/\r?\n/)
  .map((line) => line.trim()).filter((line) => line && !line.startsWith("#"));
const python = studioPythonPath(repoRoot);
const script = [
  "import importlib.metadata as m, json, sys",
  `pins=${JSON.stringify(pins)}`,
  `expected_prefix=${JSON.stringify(venvRoot)}`,
  "bad=[]",
  "[(bad.append(f'{p}: installed={m.version(p.split(\"==\",1)[0])}') if m.version(p.split('==',1)[0]).lower()!=p.split('==',1)[1].lower() else None) for p in pins]",
  "prefix_ok=__import__('os').path.normcase(__import__('os').path.realpath(sys.prefix))==__import__('os').path.normcase(__import__('os').path.realpath(expected_prefix))",
  "print(json.dumps({'python':sys.version.split()[0],'executable':sys.executable,'prefix':sys.prefix,'prefix_ok':prefix_ok,'pins':pins,'mismatches':bad}))",
  "raise SystemExit(1 if sys.version_info[:2] != (3,12) or bad or not prefix_ok else 0)",
].join(";");
const result = spawnSync(python, ["-c", script], { cwd: repoRoot, encoding: "utf8" });
if (result.stdout) process.stdout.write(result.stdout);
if (result.stderr) process.stderr.write(result.stderr);
process.exitCode = result.status ?? 1;
