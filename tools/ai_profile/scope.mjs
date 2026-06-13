#!/usr/bin/env node
import { existsSync, mkdirSync, unlinkSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";
import { parseArgs, profileScopePath, readProfileScope, stringArg, timestamp } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/scope.mjs show [--scope <path>]
  node tools/ai_profile/scope.mjs set --work-item <id> [--iteration <name>] [--scope <path>]
  node tools/ai_profile/scope.mjs clear [--scope <path>]

Writes current profiling metadata defaults to tmp/session_profiles/current_scope.json
by default. Profile records use precedence:
  explicit CLI flags > AI_PROFILE_* env vars > persistent scope file.`);
  process.exit(2);
}

function scopePath(values) {
  return resolve(stringArg(values, "scope", profileScopePath()));
}

function render(scope) {
  const lines = [];
  lines.push(`# AI Profile Scope`);
  lines.push("");
  lines.push(`Path: ${scope.path}`);
  lines.push(`Exists: ${scope.exists ? "yes" : "no"}`);
  lines.push(`Valid: ${scope.valid ? "yes" : "no"}`);
  lines.push(`Work item: ${scope.work_item || "(none)"}`);
  lines.push(`Iteration: ${scope.iteration || "(none)"}`);
  if (scope.updated_at) lines.push(`Updated: ${scope.updated_at}`);
  if (scope.error) lines.push(`Error: ${scope.error}`);
  return `${lines.join("\n")}\n`;
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help) usage();
const command = positionals[0] || "show";
const target = scopePath(values);

if (command === "show") {
  process.stdout.write(render(readProfileScope(target)));
} else if (command === "set") {
  const workItem = stringArg(values, "work-item", "").trim();
  const iteration = stringArg(values, "iteration", "").trim();
  if (!workItem) usage();
  const payload = {
    schema_version: 1,
    work_item: workItem,
    iteration,
    updated_at: timestamp(),
  };
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(payload, null, 2)}\n`, "utf8");
  process.stdout.write(render(readProfileScope(target)));
} else if (command === "clear") {
  if (existsSync(target)) unlinkSync(target);
  process.stdout.write(render(readProfileScope(target)));
} else {
  usage();
}
