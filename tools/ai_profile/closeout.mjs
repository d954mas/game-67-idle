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
  --notes <text>

The command appends one closeout event, writes a summary markdown artifact, and
prints the profile and summary paths. Both default to tmp/session_profiles/.`);
  process.exit(2);
}

function defaultSummaryPath(profilePath) {
  const parsed = basename(profilePath).replace(/\.jsonl$/i, "");
  return join(dirname(profilePath), `${parsed}.summary.md`);
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

const profilePath = resolve(stringArg(values, "profile", defaultProfilePath()));
const outputPath = resolve(stringArg(values, "output", defaultSummaryPath(profilePath)));

if (!values.phase) values.phase = "session_closeout";
if (!values.category) values.category = "reflection";
if (!values.intent) values.intent = "Close out profiled AI development session";
if (!values.result) values.result = "pass";
if (!values.value) values.value = "necessary_overhead";
if (!values["context-risk"]) values["context-risk"] = "low";
if (!values.evidence) values.evidence = outputPath;

try {
  appendRecord(profilePath, buildRecord(values));
} catch (error) {
  console.error(`profile closeout event failed: ${error.message}`);
  process.exit(1);
}

const summary = spawnSync(process.execPath, [
  "tools/ai_profile/summarize_session_profile.mjs",
  profilePath,
  "--output",
  outputPath,
], {
  cwd: process.cwd(),
  env: process.env,
  encoding: "utf8",
  stdio: ["ignore", "pipe", "pipe"],
});

if (summary.stdout) process.stdout.write(summary.stdout);
if (summary.stderr) process.stderr.write(summary.stderr);
if (summary.status !== 0) {
  process.exit(summary.status || 1);
}

console.log(`\nProfile: ${profilePath}`);
console.log(`Summary: ${outputPath}`);

