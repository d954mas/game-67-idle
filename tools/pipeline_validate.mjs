#!/usr/bin/env node
// Validate the reusable AI pipeline base in this repo and in a fresh export.
//
//   node tools/pipeline_validate.mjs

import { existsSync, rmSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));
const stamp = new Date().toISOString().replace(/[:.]/g, "-");
const exportDir = join(root, "tmp", `pipeline-validate-${stamp}`);

function run(label, args, opts = {}) {
  const { cwd = root, exe = process.execPath } = opts;
  console.log(`\n== ${label}`);
  console.log(`$ ${[exe, ...args].map((arg) => (/\s/.test(arg) ? JSON.stringify(arg) : arg)).join(" ")}`);
  const result = spawnSync(exe, args, {
    cwd,
    stdio: "inherit",
    shell: false,
  });
  if (result.error) {
    console.error(`error: ${label} failed to start: ${result.error.message}`);
    process.exit(1);
  }
  if (result.status !== 0) {
    console.error(`error: ${label} failed with exit code ${result.status}`);
    process.exit(result.status || 1);
  }
}

function findPythonRunner() {
  const candidates = [
    { exe: "py", args: ["-3.12"] },
    { exe: "python", args: [] },
    { exe: "python3", args: [] },
  ];
  for (const candidate of candidates) {
    const result = spawnSync(candidate.exe, [...candidate.args, "--version"], {
      cwd: root,
      encoding: "utf8",
      shell: false,
      stdio: "pipe",
    });
    if (result.status === 0) {
      const version = `${result.stdout || result.stderr}`.trim();
      console.log(`python runner: ${candidate.exe} ${candidate.args.join(" ")} ${version}`.replace(/\s+/g, " "));
      return candidate;
    }
  }
  console.error("error: no working Python runner found; tried py -3.12, python, and python3");
  process.exit(1);
}

if (existsSync(exportDir)) {
  rmSync(exportDir, { recursive: true, force: true });
}

run("taskboard list", ["tools/taskboard/cli.mjs", "list"]);
run("skill eval", ["tools/skills_eval.mjs"]);
run("taskboard validate", ["tools/taskboard/cli.mjs", "validate"]);
run("taskboard tests", ["--test", "tools/taskboard/test.mjs"]);

// Runtime seed checks. Skipped automatically in workflow-only exports, which
// have no state schema or CMake presets.
if (existsSync(join(root, "state", "game_state.schema.json"))) {
  const python = findPythonRunner();
  run("state codegen", [...python.args, "tools/state_codegen/generate_state.py"], { exe: python.exe });
}
if (existsSync(join(root, "CMakePresets.json"))) {
  run("cmake configure", ["--preset", "native-debug"], { exe: "cmake" });
}

run("portable export", ["tools/bootstrap/export_base.mjs", "--target", exportDir]);
run("exported skill eval", ["tools/skills_eval.mjs"], { cwd: exportDir });
run("exported taskboard validate", ["tools/taskboard/cli.mjs", "validate"], { cwd: exportDir });
run("exported taskboard tests", ["--test", "tools/taskboard/test.mjs"], { cwd: exportDir });

console.log(`\nok: reusable pipeline validation passed`);
console.log(`export: ${exportDir}`);
