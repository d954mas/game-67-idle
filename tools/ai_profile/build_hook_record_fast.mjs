#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const source = join(here, "hook_record_fast.c");
const output = join(here, process.platform === "win32" ? "hook_record_fast.exe" : "hook_record_fast");
const candidates = process.env.CC ? [process.env.CC] : process.platform === "win32"
  ? ["clang", "cl"]
  : ["cc", "clang", "gcc"];

function run(exe, args) {
  return spawnSync(exe, args, { cwd: resolve(here, "..", ".."), stdio: "inherit", shell: process.platform === "win32" });
}

let lastStatus = 1;
for (const cc of candidates) {
  const args = cc === "cl"
    ? ["/nologo", "/O2", "/std:c17", source, `/Fe:${output}`]
    : ["-O2", "-std=c17", source, "-o", output];
  const result = run(cc, args);
  lastStatus = result.status ?? 1;
  if (lastStatus === 0 && existsSync(output)) {
    console.log(`built ${output}`);
    process.exit(0);
  }
}

console.error("failed to build hook_record_fast; install clang/cc or set CC");
process.exit(lastStatus || 1);
