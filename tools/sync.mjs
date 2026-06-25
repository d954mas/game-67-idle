// Single cross-harness config sync (Claude + Codex). One command regenerates
// every generated config surface from its canonical source, so the two harnesses
// can never drift. Run after editing any canonical source:
//
//   node tools/sync.mjs            regenerate all surfaces
//   node tools/sync.mjs --check    report drift, write nothing, exit 1 on drift
//
// Surfaces and their single source of truth:
//   instructions  AGENTS.md (canonical) -> CLAUDE.md imports it via `@AGENTS.md`;
//                 Codex reads AGENTS.md natively. Nothing to generate.
//   skills        .codex/skills/* (canonical) -> .claude/skills/* thin pointers
//                 (tools/skills_sync.mjs).
//   hooks         one source in tools/hooks_sync.mjs -> .codex/hooks.json (Codex
//                 format) + .claude/settings.json hooks block (Claude format).
//
// To add a surface later (slash commands, subagents, MCP), add its generator to
// STEPS — keep each generator a standalone `--check`-aware script.

import { spawnSync } from "node:child_process";
import { resolve, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const args = process.argv.slice(2);

if (args.includes("--help") || args.includes("-h")) {
  console.log("usage: node tools/sync.mjs [--check]");
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
