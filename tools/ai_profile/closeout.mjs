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
  --review-output <path> default: <profile basename>.review.md next to profile
  --review-json-output <path> default: <profile basename>.review.json next to profile
  --followups-output <path> default: <profile basename>.followups.md next to profile
  --followups-json-output <path> default: <profile basename>.followups.json next to profile
  --no-review            only write closeout event and summary
  --no-followups         write review artifacts but skip follow-up drafts
  --phase <phase>        default: session_closeout
  --intent <text>        default: Close out profiled AI development session
  --result <...>         default: pass
  --value <...>          default: necessary_overhead
  --context-risk <...>   default: low
  --work-item <id>       task/issue/phase id for segmenting long profiles
  --iteration <name>     small iteration or batch label
  --notes <text>

The command appends one closeout event, writes summary/review/follow-up scratch
artifacts, and prints their paths. All outputs default to tmp/session_profiles/.

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
const reviewEnabled = values["no-review"] !== true;
const followupsEnabled = reviewEnabled && values["no-followups"] !== true;
const reviewOutputPath = resolve(stringArg(values, "review-output", defaultArtifactPath(profilePath, "review.md")));
const reviewJsonOutputPath = resolve(stringArg(values, "review-json-output", defaultArtifactPath(profilePath, "review.json")));
const followupsOutputPath = resolve(stringArg(values, "followups-output", defaultArtifactPath(profilePath, "followups.md")));
const followupsJsonOutputPath = resolve(stringArg(values, "followups-json-output", defaultArtifactPath(profilePath, "followups.json")));

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
  if (reviewEnabled) values.tool.push("ai_profile/review.mjs");
  if (followupsEnabled) values.tool.push("ai_profile/followups.mjs");
}
if (!values.evidence) {
  values.evidence = [outputPath];
  if (reviewEnabled) values.evidence.push(reviewOutputPath, reviewJsonOutputPath);
  if (followupsEnabled) values.evidence.push(followupsOutputPath, followupsJsonOutputPath);
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

if (reviewEnabled) {
  runNodeTool([
    "tools/ai_profile/review.mjs",
    profilePath,
    "--output",
    reviewOutputPath,
    "--json-output",
    reviewJsonOutputPath,
  ], "profile review");
}

if (followupsEnabled) {
  runNodeTool([
    "tools/ai_profile/followups.mjs",
    reviewJsonOutputPath,
    "--output",
    followupsOutputPath,
    "--json-output",
    followupsJsonOutputPath,
  ], "profile followups");
}

console.log(`\nProfile: ${profilePath}`);
console.log(`Summary: ${outputPath}`);
if (reviewEnabled) {
  console.log(`Review: ${reviewOutputPath}`);
  console.log(`Review JSON: ${reviewJsonOutputPath}`);
}
if (followupsEnabled) {
  console.log(`Follow-ups: ${followupsOutputPath}`);
  console.log(`Follow-ups JSON: ${followupsJsonOutputPath}`);
}
