#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import { basename, dirname, join, resolve } from "node:path";
import {
  appendRecord,
  buildRecord,
  defaultProfilePath,
  parseArgs,
  stringArg,
} from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/closeout.mjs [options]

options:
  --profile <path>       default: tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
  --output <path>        default: <profile basename>.summary.md next to profile
  --phase <phase>        default: session_closeout
  --intent <text>        default: Close out profiled AI development session
  --result <...>         default: pass
  --value <...>          default: necessary_overhead
  --context-risk <...>   default: low
  --work-item <id>       task/issue/phase id for segmenting long profiles
  --iteration <name>     small iteration or batch label
  --notes <text>

The command appends one short session_closeout event and writes the session
summary. Outputs default to tmp/session_profiles/.

Environment defaults:
  AI_PROFILE_WORK_ITEM       fallback for --work-item
  AI_PROFILE_ITERATION       fallback for --iteration
  tools/ai_profile/scope.mjs fallback after env vars`);
  process.exit(2);
}

function basenameWithoutJsonl(profilePath) {
  const parsed = basename(profilePath).replace(/\.jsonl$/i, "");
  return parsed;
}

function defaultArtifactPath(profilePath, suffix) {
  return join(dirname(profilePath), `${basenameWithoutJsonl(profilePath)}.${suffix}`);
}

function runNodeTool(args, label, { printStdout = false } = {}) {
  const result = spawnSync(process.execPath, args, {
    cwd: process.cwd(),
    env: process.env,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  if (printStdout && result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
  if (result.status !== 0) {
    if (!printStdout && result.stdout) process.stdout.write(result.stdout);
    console.error(`${label} failed`);
    process.exit(result.status || 1);
  }
  return result;
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

const profilePath = resolve(stringArg(values, "profile", defaultProfilePath()));
const outputPath = resolve(stringArg(values, "output", defaultArtifactPath(profilePath, "summary.md")));

if (!values.phase) values.phase = "session_closeout";
if (!values.category) values.category = "reflection";
if (!values.intent) values.intent = "Close out profiled AI development session";
if (!values.result) values.result = "pass";
if (!values.value) values.value = "necessary_overhead";
if (!values["context-risk"]) values["context-risk"] = "low";
if (!values.tool) {
  values.tool = [
    "ai_profile/closeout.mjs",
    "ai_profile/summarize_session_profile.mjs",
  ];
}
if (!values.evidence) {
  values.evidence = [outputPath];
}

try {
  appendRecord(profilePath, buildRecord(values));
} catch (error) {
  console.error(`profile closeout event failed: ${error.message}`);
  process.exit(1);
}

runNodeTool([
  "tools/ai_profile/summarize_session_profile.mjs",
  profilePath,
  "--output",
  outputPath,
], "profile summary", { printStdout: true });

console.log(`\nProfile: ${profilePath}`);
console.log(`Summary: ${outputPath}`);
