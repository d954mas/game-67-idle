#!/usr/bin/env node
import { spawnSync } from "node:child_process";

const JS_EXTENSIONS = /\.(?:cjs|js|mjs)$/i;

function runGit(args) {
  const result = spawnSync("git", args, {
    cwd: process.cwd(),
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.status !== 0) {
    if (result.stderr) process.stderr.write(result.stderr);
    process.exit(result.status || 1);
  }
  return result.stdout.split(/\r?\n/).map((line) => line.trim()).filter(Boolean);
}

function touchedFiles() {
  const tracked = runGit(["diff", "--name-only", "--diff-filter=ACMRTUXB", "HEAD", "--"]);
  const untracked = runGit(["ls-files", "--others", "--exclude-standard"]);
  return [...new Set([...tracked, ...untracked])]
    .filter((file) => JS_EXTENSIONS.test(file))
    .sort((a, b) => a.localeCompare(b));
}

const files = touchedFiles();
if (files.length === 0) {
  console.log("No touched JavaScript files to syntax-check.");
  process.exit(0);
}

let failed = false;
for (const file of files) {
  console.log(`node --check ${file}`);
  const result = spawnSync(process.execPath, ["--check", file], {
    cwd: process.cwd(),
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  if (result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
  if (result.status !== 0) failed = true;
}

process.exit(failed ? 1 : 0);
