// Export the portable AI pipeline base into a new game project.
//
//   node ai_studio/bootstrap/export_base.mjs --target C:\projects\new-game [--force]
//
// Copies the reusable pieces (skills, AI Studio modules, skill sync, design knowledge,
// task store conventions) and writes starter AGENTS.md /
// CLAUDE.md. Existing files in the target are preserved unless --force.
// See ai_studio/README.md and ai_studio/core_harness/profiling/README.md for what stays.

import { cpSync, mkdirSync, writeFileSync, existsSync } from "node:fs";
import { join, resolve, basename } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

const root = resolve(fileURLToPath(new URL("../..", import.meta.url)));

const args = process.argv.slice(2);
const force = args.includes("--force");
const targetIndex = args.indexOf("--target");
const target = targetIndex !== -1 ? args[targetIndex + 1] : null;
if (!target) {
  console.error("usage: node ai_studio/bootstrap/export_base.mjs --target <dir> [--force]");
  process.exit(1);
}
const dst = resolve(target);
if (dst === root) {
  console.error("error: target is this repository");
  process.exit(1);
}

const COPY = [
  ".codex/skills",
  "ai_studio/core_harness/profiling",
  "tools/requirements/ai-pipeline-full.txt",
  "tools/serve_tunnel.mjs",
  "tools/lib/hash.mjs",
  "tools/lib/cli.mjs",
  "tools/lib/cli.test.mjs",
  "tools/lib/json.mjs",
  "tools/lib/json.test.mjs",
  "tools/lib/paths.mjs",
  "tools/lib/paths.test.mjs",
  "tools/lib/mime.mjs",
  "tools/lib/mime.test.mjs",
  "tools/lib/tmp_exports.mjs",
  "tools/lib/tmp_exports.test.mjs",
  "tools/game_context",
  "tools/README.md",
  "gamedesign/README.md",
  "gamedesign/knowledge",
  "gamedesign/sources",
  "ai_studio",
  "docs/ai-pipeline",
  "AI_PIPELINE_HISTORY.md",
];

const AGENTS_TEMPLATE = `# AGENTS.md

## Project

- This is an AI-first game development project: improve the game and the AI workflow together.
- This is a clean starter project. Do not import old testbed game docs, task
  history, assets, or code unless the user explicitly asks.
- No game concept has been selected yet. Do not invent one; ask the user for the
  concept before creating GDD, assets, gameplay tasks, or implementation plans.
- Engine/runtime location is not selected yet. Once chosen, record its path and
  editing policy here.
- Game design lives in \`gamedesign/\`; game code lives in \`src/\`.
- Universal reusable design knowledge lives in \`gamedesign/knowledge/\`.
- Reusable project skills live in \`.codex/skills/\`; keep them generic enough to reuse in other games.
- Work items live in \`tasks/\`; follow \`ai_studio/taskboard/README.md\`.
- Current game routing lives in \`GAME_PROJECT.md\`.
- Temporary generation, scripts, rejected images, screenshots, and audit logs go in \`tmp/\` or another ignored temp folder.
- Final durable docs/data/assets go in their project folder.
- The shared human/agent process, including AI session profiling, lives in
  \`ai_studio/README.md\`; raw telemetry stays in \`tmp/session_profiles/\`.
- AI workflow history, retrospectives, and the external observability decision
  criteria live in \`AI_PIPELINE_HISTORY.md\`; start local-first and run only
  bounded pilots until a tool proves value.

## Direction

- No concept selected yet. First step: capture the user's concept, references,
  audience, platform, and no-go constraints, then create/refine one task or
  epic before implementation.

## Validation

- Primary runtime target is not selected yet. Once chosen, record the required
  build/run/test commands and proof requirements here.
- When validating playable or visual changes, capture screenshots and use emulated input.
- If a task reveals repeated friction, propose updating \`AGENTS.md\` or creating/updating a project skill.
`;

const CLAUDE_TEMPLATE = `Project rules, direction, and validation policy: @AGENTS.md
Shared human/agent process: @ai_studio/README.md

Work items (tasks, epics, deferred ideas) live in the \`tasks/\` store.
For status, task format, and workflow rules, follow \`ai_studio/taskboard/README.md\`:

- Process: \`.codex/skills/nt-taskboard-manager/SKILL.md\`
- Conventions: \`ai_studio/taskboard/README.md\`
- Current game routing: \`GAME_PROJECT.md\`
- CLI: \`node ai_studio/taskboard/cli.mjs <list|show|new|set|validate>\`
- Visual board for the user: \`node ai_studio/studio_shell/server.mjs\`, then open \`/taskboard/\`.
`;

const GAME_PROJECT_TEMPLATE = `# GAME_PROJECT

## Active Game

Status: none

There is no active game concept right now.

## When A Game Is Active

Keep only routing and current-game summary here:

- Game id:
- Game folder:
- Design docs:
- Task board:
- Current milestone:
- Hard game-specific constraints:

Full GDD, balance, lore, asset lists, and per-game implementation detail should
live in \`gamedesign/projects/<game-id>/\`, the game folder, or task files.
`;

mkdirSync(dst, { recursive: true });
for (const entry of COPY) {
  const fromRel = typeof entry === "string" ? entry : entry.from;
  const toRel = typeof entry === "string" ? entry : entry.to;
  const from = join(root, fromRel);
  if (!existsSync(from)) {
    console.warn(`warn: missing ${fromRel}, skipped`);
    continue;
  }
  const to = join(dst, toRel);
  mkdirSync(join(to, ".."), { recursive: true });
  cpSync(from, to, { recursive: true, force });
  console.log(fromRel === toRel ? `copied: ${fromRel}` : `copied: ${fromRel} -> ${toRel}`);
}

mkdirSync(join(dst, "tasks", "epics"), { recursive: true });
mkdirSync(join(dst, "tasks", "active"), { recursive: true });
mkdirSync(join(dst, "tasks", "archive"), { recursive: true });
mkdirSync(join(dst, "tmp"), { recursive: true });

for (const [name, content] of [
  ["AGENTS.md", AGENTS_TEMPLATE],
  ["CLAUDE.md", CLAUDE_TEMPLATE],
  ["GAME_PROJECT.md", GAME_PROJECT_TEMPLATE],
]) {
  const file = join(dst, name);
  if (existsSync(file) && !force) {
    console.log(`kept existing: ${name}`);
  } else {
    writeFileSync(file, content);
    console.log(`wrote: ${name}`);
  }
}

const sync = spawnSync(process.execPath, [join(dst, "ai_studio", "core_harness", "agent_surfaces", "sync.mjs")], {
  cwd: dst,
  stdio: "inherit",
});
if (sync.status !== 0) {
  console.warn("warn: agent surface sync failed in target; run it manually");
}

console.log(`\nexported AI base to ${dst} (${basename(root)} -> ${basename(dst)})`);
console.log("next: choose engine/runtime policy when known, ensure tmp/ is ignored, then capture first ideas:");
console.log("  node ai_studio/taskboard/cli.mjs new task --title \"...\" --status idea");
