#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { defaultProfilePath, parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/prepare_reflection.mjs [--profile <profile.jsonl>] [--json-output <status.json>] [--allow-regression] [--verbose]

Refreshes the reflection handoff chain using existing tools:
closeout -> baseline comparison -> reflection packet -> reflection draft.

It does not auto-capture baselines. If no baseline is captured, run
capture_baseline.mjs deliberately after reviewing whether the profile is clean.`);
  process.exit(2);
}

function runNode(args, label, { quiet = false } = {}) {
  const result = spawnSync(process.execPath, args, {
    cwd: process.cwd(),
    env: process.env,
    encoding: "utf8",
    stdio: ["ignore", "pipe", "pipe"],
  });
  if (!quiet && result.stdout) process.stdout.write(result.stdout);
  if (result.stderr) process.stderr.write(result.stderr);
  if (result.status !== 0) {
    if (quiet && result.stdout) process.stdout.write(result.stdout);
    console.error(`${label} failed`);
    process.exit(result.status || 1);
  }
  return result;
}

let verboseOutput = false;

function readJson(path) {
  return JSON.parse(readFileSync(path, "utf8"));
}

function statusPath(profilePath) {
  const base = profilePath.replace(/\.jsonl$/i, "");
  return `${base}.prepare_reflection.status.json`;
}

function writeStatus(profilePath, outputPath = "") {
  const target = resolve(outputPath || statusPath(profilePath));
  mkdirSync(dirname(target), { recursive: true });
  runNode(["tools/ai_profile/status.mjs", "--profile", profilePath, "--json-output", target], "profile status", { quiet: true });
  return readJson(target);
}

function needsBundle(status) {
  return status.exists && status.valid && (!status.closeout_seen || !status.bundle.complete || !status.bundle.fresh);
}

function runCloseout(profilePath) {
  runNode([
    "tools/ai_profile/closeout.mjs",
    "--profile",
    profilePath,
    "--intent",
    "Prepare AI reflection handoff",
    "--notes",
    "prepare_reflection.mjs refreshed closeout bundle",
  ], "profile closeout", { quiet: !verboseOutput });
}

function runCompare(status) {
  runNode([
    "tools/ai_profile/compare_reviews.mjs",
    status.baselines.latest_manifest.baseline_review,
    status.comparison.paths.review_json,
    "--output",
    status.comparison.paths.compare_md,
    "--json-output",
    status.comparison.paths.compare_json,
  ], "baseline comparison", { quiet: !verboseOutput });
}

function runPacket(status, profilePath) {
  runNode([
    "tools/ai_profile/reflection_packet.mjs",
    profilePath,
    "--output",
    status.reflection.packet.markdown,
    "--json-output",
    status.reflection.packet.json,
  ], "reflection packet", { quiet: !verboseOutput });
}

function runDraft(status) {
  runNode([
    "tools/ai_profile/reflection_draft.mjs",
    status.reflection.packet.json,
    "--output",
    status.reflection.draft.markdown,
    "--json-output",
    status.reflection.draft.json,
  ], "reflection draft", { quiet: !verboseOutput });
}

function ensureProfile(status) {
  if (!status.exists) {
    console.error("profile does not exist; start profiling before preparing reflection");
    process.exit(1);
  }
  if (!status.valid) {
    console.error("profile has invalid JSONL; fix it before preparing reflection");
    process.exit(1);
  }
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();

const profilePath = resolve(stringArg(values, "profile", defaultProfilePath()));
const finalStatusOutput = stringArg(values, "json-output", "");
const allowRegression = values["allow-regression"] === true;
verboseOutput = values.verbose === true;
const tempStatus = statusPath(profilePath);
const steps = [];

let status = writeStatus(profilePath);
ensureProfile(status);

if (needsBundle(status)) {
  steps.push("closeout");
  runCloseout(profilePath);
  status = writeStatus(profilePath);
}

if (!status.baselines.latest_manifest) {
  console.error(`no captured baseline; review ${status.bundle.artifacts.find((artifact) => artifact.name === "review_json")?.path || "review JSON"} and run capture_baseline.mjs deliberately`);
  process.exit(1);
}

if (status.comparison.status === "missing" || status.comparison.status === "stale" || status.comparison.status === "invalid") {
  steps.push("compare");
  runCompare(status);
  status = writeStatus(profilePath);
}

if (status.comparison.status === "regressed" && !allowRegression) {
  console.error(`current-scope regressions present; inspect ${status.comparison.paths.compare_json} or rerun with --allow-regression`);
  process.exit(1);
}

if (status.reflection.packet.status !== "fresh") {
  steps.push("packet");
  runPacket(status, profilePath);
  status = writeStatus(profilePath);
}

if (status.reflection.draft.status !== "fresh") {
  steps.push("draft");
  runDraft(status);
  status = writeStatus(profilePath);
}

if (finalStatusOutput) {
  const target = resolve(finalStatusOutput);
  mkdirSync(dirname(target), { recursive: true });
  runNode(["tools/ai_profile/status.mjs", "--profile", profilePath, "--json-output", target], "final profile status", { quiet: true });
  status = readJson(target);
} else if (existsSync(tempStatus)) {
  rmSync(tempStatus, { force: true });
}

console.log("# AI Reflection Prep");
console.log(`Profile: ${profilePath}`);
console.log(`Steps: ${steps.length > 0 ? steps.join(", ") : "none"}`);
console.log(`Packet: ${status.reflection.packet.status} ${status.reflection.packet.markdown}`);
console.log(`Draft: ${status.reflection.draft.status} ${status.reflection.draft.markdown}`);
console.log(`Next: ${status.next_action}`);
