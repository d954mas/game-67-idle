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

function usage(stream = console.error) {
  stream("usage: node ai_studio/core_harness/agent_surfaces/sync.mjs [--check]");
}

if (args.includes("--help") || args.includes("-h")) {
  usage(console.log);
  process.exit(0);
}
const check = args.includes("--check");
if (check) args.splice(args.indexOf("--check"), 1);
if (args.length > 0) {
  usage();
  process.exit(2);
}

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
