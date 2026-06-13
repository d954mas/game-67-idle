#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import { appendRecord, buildRecord, parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/run.mjs --phase <phase> --category <category> --intent <text> [options] -- <command> [args...]

options:
  --value <productive|necessary_overhead|rework|waste|unknown> default: productive
  --profile <path>                                            default: tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
  --context-risk <low|medium|high|unknown>
  --file-read <path>            repeatable
  --file-written <path>         repeatable
  --evidence <path>             repeatable
  --context-input <path:chars:reason> repeatable
  --waste-reason <text>
  --blocked-by <text>
  --notes <text>

The wrapped command inherits stdio. The profile event is appended after the
command exits and records duration, exit code, and pass/fail result.`);
  process.exit(2);
}

function commandText(args) {
  return args.map((arg) => (/\s/.test(arg) ? JSON.stringify(arg) : arg)).join(" ");
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help || positionals.length === 0) usage();

const command = positionals[0];
const commandArgs = positionals.slice(1);
const started = process.hrtime.bigint();
const result = spawnSync(command, commandArgs, {
  cwd: process.cwd(),
  env: process.env,
  shell: false,
  stdio: "inherit",
});
const ended = process.hrtime.bigint();
const durationMs = Number((ended - started) / 1000000n);
const exitCode = typeof result.status === "number" ? result.status : 1;

if (!values.result) values.result = exitCode === 0 ? "pass" : "fail";
if (!values.value) values.value = "productive";
if (!values["duration-ms"]) values["duration-ms"] = String(durationMs);
if (!values.tool) values.tool = "shell_command";
if (!values.command) values.command = commandText(positionals);

const extra = {
  command_exit_code: exitCode,
};
if (result.error) {
  extra.command_error = result.error.message;
}

try {
  const profilePath = stringArg(values, "profile", "");
  const target = appendRecord(profilePath, buildRecord(values, extra));
  console.error(`profile run appended: ${target}`);
} catch (error) {
  console.error(`profile run write failed: ${error.message}`);
}

process.exit(exitCode);

