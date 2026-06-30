#!/usr/bin/env node
// Housekeeping for the ignored tmp/ scratch folder.
//
//   node ai_studio/core_harness/tool_lib/tmp_sweep.mjs [--list] [--all-scratch] [--keep-validate <n>] [--dry-run] [--root <dir>]
//
// tmp/ is gitignored disposable scratch; durable evidence lives under
// gamedesign/projects/<id>/. Legacy pipeline validation exports are pruned
// tmp/pipeline-validate-* dirs (T0043). This sweep is the EXPLICIT, opt-in way
// to clear the rest (closed-prototype renders, generation pipelines, atlas
// review dirs) at prototype close. Default is --list (reports, deletes nothing).

import { existsSync, readdirSync, rmSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { isValidateExportName, partitionByKeep } from "./tmp_exports.mjs";

const repoRoot = resolve(fileURLToPath(new URL("../../..", import.meta.url)));
const args = process.argv.slice(2);

function flagValue(name, fallback) {
  const idx = args.indexOf(name);
  if (idx === -1) return fallback;
  return args[idx + 1];
}

const allowed = new Set(["--list", "--all-scratch", "--keep-validate", "--dry-run", "--root", "--help", "-h"]);
for (let i = 0; i < args.length; i += 1) {
  const a = args[i];
  if (a === "--keep-validate" || a === "--root") { i += 1; continue; }
  if (!allowed.has(a)) {
    console.error(`unknown arg: ${a}`);
    process.exit(2);
  }
}
if (args.includes("--help") || args.includes("-h")) {
  console.log("usage: node ai_studio/core_harness/tool_lib/tmp_sweep.mjs [--list] [--all-scratch] [--keep-validate <n>] [--dry-run] [--root <dir>]");
  process.exit(0);
}

const root = resolve(flagValue("--root", repoRoot));
const tmpDir = join(root, "tmp");
const keepValidate = Math.max(0, Number.parseInt(flagValue("--keep-validate", "3"), 10) || 0);
const allScratch = args.includes("--all-scratch");
const dryRun = args.includes("--dry-run");

function dirSizeBytes(path) {
  let total = 0;
  let stack = [path];
  while (stack.length) {
    const current = stack.pop();
    let st;
    try { st = statSync(current); } catch { continue; }
    if (st.isDirectory()) {
      for (const name of readdirSync(current)) stack.push(join(current, name));
    } else {
      total += st.size;
    }
  }
  return total;
}

function human(bytes) {
  if (bytes >= 1024 * 1024 * 1024) return `${(bytes / 1024 / 1024 / 1024).toFixed(1)}G`;
  if (bytes >= 1024 * 1024) return `${(bytes / 1024 / 1024).toFixed(1)}M`;
  if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)}K`;
  return `${bytes}B`;
}

if (!existsSync(tmpDir)) {
  console.log(`no tmp/ at ${tmpDir}; nothing to sweep`);
  process.exit(0);
}

const entries = readdirSync(tmpDir).sort();
// Prefix + "keep newest N" contract for old pipeline-validate export dirs.
const validateDirs = entries.filter(isValidateExportName);
const keptValidate = new Set(partitionByKeep(validateDirs, keepValidate).kept);

// Scratch = everything except the newest N pipeline-validate dirs we keep.
const scratch = entries.filter((n) => !keptValidate.has(n));

if (!allScratch) {
  let total = 0;
  console.log(`tmp/ scratch report (${tmpDir}); --all-scratch to delete, keeping newest ${keepValidate} pipeline-validate dir(s):`);
  for (const name of scratch) {
    const bytes = dirSizeBytes(join(tmpDir, name));
    total += bytes;
    console.log(`  ${human(bytes).padStart(7)}  ${name}`);
  }
  console.log(`reclaimable: ${human(total)} across ${scratch.length} entr${scratch.length === 1 ? "y" : "ies"} (kept ${keptValidate.size} validate dir(s))`);
  process.exit(0);
}

let removed = 0;
let freed = 0;
for (const name of scratch) {
  const path = join(tmpDir, name);
  const bytes = dirSizeBytes(path);
  if (dryRun) {
    console.log(`would remove ${human(bytes).padStart(7)}  ${name}`);
  } else {
    rmSync(path, { recursive: true, force: true });
    console.log(`removed ${human(bytes).padStart(7)}  ${name}`);
  }
  removed += 1;
  freed += bytes;
}
console.log(`${dryRun ? "would free" : "freed"} ${human(freed)} across ${removed} entr${removed === 1 ? "y" : "ies"}; kept ${keptValidate.size} newest pipeline-validate dir(s)`);
