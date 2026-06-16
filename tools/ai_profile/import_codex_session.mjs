#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import process from "node:process";

function flagValue(args, flag) {
  const index = args.indexOf(flag);
  if (index === -1) return "";
  const value = args[index + 1];
  return value && !value.startsWith("--") ? value : "";
}

const args = process.argv.slice(2);
if (args.includes("--help") || args.includes("-h")) {
  console.error(`usage:
  node tools/ai_profile/import_codex_session.mjs [--profile <profile.jsonl>] [--session <codex-session.jsonl>]

Imports missed failed Codex shell commands from the local Codex session JSONL
into the normal tmp/session_profiles profile. This is an analysis-time step,
not a hot-path hook.`);
  process.exit(2);
}

const env = { ...process.env, AI_PROFILE_RECOVER_ONLY: "1" };
const profile = flagValue(args, "--profile");
if (profile) env.AI_PROFILE_FILE = profile;
const session = flagValue(args, "--session");
if (session) env.CODEX_SESSION_FILE = session;

const result = spawnSync(process.execPath, ["tools/ai_profile/hook_record.mjs", "codex"], {
  cwd: process.cwd(),
  env,
  input: "{}",
  encoding: "utf8",
  stdio: ["pipe", "inherit", "inherit"],
});

process.exit(result.status ?? 1);
