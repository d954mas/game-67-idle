#!/usr/bin/env node
import { spawnSync } from "node:child_process";
import process from "node:process";

const result = spawnSync(process.execPath, ["tools/ai_profile/hook_record.mjs", "codex"], {
  cwd: process.cwd(),
  env: { ...process.env, AI_PROFILE_RECOVER_ONLY: "1" },
  input: "{}",
  encoding: "utf8",
  stdio: ["pipe", "inherit", "inherit"],
});

process.exit(result.status ?? 1);
