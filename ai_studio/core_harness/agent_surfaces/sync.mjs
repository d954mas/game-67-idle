// Sync generated Codex/Claude agent surfaces from canonical sources.
//
//   node ai_studio/core_harness/agent_surfaces/sync.mjs
//   node ai_studio/core_harness/agent_surfaces/sync.mjs --check
//
// Surfaces:
//   instructions  AGENTS.md -> CLAUDE.md imports it via @AGENTS.md.
//   skills        .codex/skills/* -> .claude/skills/* thin pointers.
//   hooks         hook source -> .codex/hooks.json + .claude/settings.json hooks.
//
// Add new generated surfaces as standalone --check-aware steps.

import { spawnSync } from "node:child_process";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);

if (args.includes("--help") || args.includes("-h")) {
  console.log("usage: node ai_studio/core_harness/agent_surfaces/sync.mjs [--check]");
  process.exit(0);
}
const check = args.includes("--check");

const STEPS = [
  ["skills", "skills_sync.mjs"],
  ["hooks", "hooks_sync.mjs"],
];

let failed = 0;
for (const [name, script] of STEPS) {
  const result = spawnSync(
    process.execPath,
    [resolve(here, script), ...(check ? ["--check"] : [])],
    { stdio: "inherit" },
  );
  if (result.status !== 0) {
    failed += 1;
    console.error(`sync: ${name} ${check ? "drift" : "failed"}`);
  }
}
process.exit(failed ? 1 : 0);
