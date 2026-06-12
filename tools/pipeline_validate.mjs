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

function run(label, args, cwd = root) {
  console.log(`\n== ${label}`);
  console.log(`$ ${args.map((arg) => (/\s/.test(arg) ? JSON.stringify(arg) : arg)).join(" ")}`);
  const result = spawnSync(process.execPath, args, {
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

if (existsSync(exportDir)) {
  rmSync(exportDir, { recursive: true, force: true });
}

run("taskboard list", ["tools/taskboard/cli.mjs", "list"]);
run("skill eval", ["tools/skills_eval.mjs"]);
run("taskboard validate", ["tools/taskboard/cli.mjs", "validate"]);
run("taskboard tests", ["--test", "tools/taskboard/test.mjs"]);
run("portable export", ["tools/bootstrap/export_base.mjs", "--target", exportDir]);
run("exported skill eval", ["tools/skills_eval.mjs"], exportDir);
run("exported taskboard validate", ["tools/taskboard/cli.mjs", "validate"], exportDir);
run("exported taskboard tests", ["--test", "tools/taskboard/test.mjs"], exportDir);

console.log(`\nok: reusable pipeline validation passed`);
console.log(`export: ${exportDir}`);
