#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import { appendRecord, buildRecord, parseArgs, stringArg } from "./profile_lib.mjs";

function usage() {
  console.error(`usage:
  node tools/ai_profile/context_command.mjs --phase <phase> --intent <text> [options] -- <command> [args...]

options:
  --category <category>                      default: context
  --result <pass|fail|mixed|blocked|skipped|unknown> default: pass/fail from command exit
  --value <productive|necessary_overhead|rework|waste|unknown> default: necessary_overhead
  --reason <text>                            default: command context
  --context-risk <low|medium|high|unknown>   default: auto from output chars
  --profile <path>                           default: tmp/session_profiles/session_profile_YYYY-MM-DD.jsonl
  --work-item <id>                           task/issue/phase id for segmenting long profiles
  --iteration <name>                         small iteration or batch label
  --notes <text>

Runs a read-only context command, prints its output, and records the measured
stdout/stderr character count as a context_inputs entry.`);
  process.exit(2);
}

function commandText(args) {
  return args.map((arg) => (/\s/.test(arg) ? JSON.stringify(arg) : arg)).join(" ");
}

function autoRisk(chars) {
  if (chars >= 50000) return "high";
  if (chars >= 10000) return "medium";
  return "low";
}

const { values, positionals } = parseArgs(process.argv.slice(2));
if (values.help || positionals.length === 0) usage();

if (!values.category) values.category = "context";
if (!values.value) values.value = "necessary_overhead";
if (!values.tool) values.tool = "ai_profile/context_command.mjs";

const command = positionals[0];
const commandArgs = positionals.slice(1);
const started = process.hrtime.bigint();
const result = spawnSync(command, commandArgs, {
  cwd: process.cwd(),
  env: process.env,
  shell: false,
  encoding: "utf8",
  stdio: ["ignore", "pipe", "pipe"],
});
const ended = process.hrtime.bigint();
const durationMs = Number((ended - started) / 1000000n);
const exitCode = typeof result.status === "number" ? result.status : 1;
const stdout = result.stdout || "";
const stderr = result.stderr || "";

if (stdout) process.stdout.write(stdout);
if (stderr) process.stderr.write(stderr);

const outputChars = stdout.length + stderr.length;
if (!values.result) values.result = exitCode === 0 ? "pass" : "fail";
if (!values["duration-ms"]) values["duration-ms"] = String(durationMs);
if (!values["context-risk"]) values["context-risk"] = autoRisk(outputChars);
if (!values.command) values.command = commandText(positionals);

const reason = stringArg(values, "reason", "command context");
const extra = {
  command_exit_code: exitCode,
  context_inputs: [{
    path: `command:${commandText(positionals)}`,
    chars: outputChars,
    reason,
  }],
};
if (result.error) extra.command_error = result.error.message;

try {
  const profilePath = stringArg(values, "profile", "");
  const target = appendRecord(profilePath, buildRecord(values, extra));
  console.error(`profile context command appended: ${target}`);
  console.error(`- command output: ${outputChars} chars (${reason})`);
} catch (error) {
  console.error(`profile context command write failed: ${error.message}`);
}

process.exit(exitCode);
