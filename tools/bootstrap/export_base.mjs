// Export the portable AI pipeline base into a new game project.
//
//   node tools/bootstrap/export_base.mjs --target C:\projects\new-game [--force]
//
// Copies the reusable pieces (skills, taskboard, skill sync, design knowledge,
// pipeline doc, task store conventions) and writes starter AGENTS.md /
// CLAUDE.md. Existing files in the target are preserved unless --force.
// See AI_PIPELINE.md "Reuse in a new project" for what intentionally stays.

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
  console.error("usage: node tools/bootstrap/export_base.mjs --target <dir> [--force]");
  process.exit(1);
}
const dst = resolve(target);
if (dst === root) {
  console.error("error: target is this repository");
  process.exit(1);
}

const COPY = [
  ".codex/skills",
  "tools/skills_sync.mjs",
  "tools/taskboard",
  "tools/bootstrap/export_base.mjs",
  "gamedesing/knowledge",
  "AI_PIPELINE.md",
  "tasks/README.md",
];

const AGENTS_TEMPLATE = `# AGENTS.md

## Project

- This is an AI-first game development project: improve the game and the AI workflow together.
- TODO: engine location and editing policy.
- Game design lives in \`gamedesing/\`; game code lives in \`src/\`.
- Universal reusable design knowledge lives in \`gamedesing/knowledge/\`.
- Reusable project skills live in \`.codex/skills/\`; keep them generic enough to reuse in other games.
- Work items live in the task store \`tasks/\` (see \`tasks/README.md\`); manage them with \`.codex/skills/task-manager/\` and \`tools/taskboard/\`. Capture deferred ideas as tasks instead of losing them.
- Temporary generation, scripts, rejected images, screenshots, and audit logs go in \`tmp/\` or another ignored temp folder.
- Final durable docs/data/assets go in their project folder.
- The shared human/agent process lives in \`AI_PIPELINE.md\`.

## Direction

- TODO: current concept, references, scope guardrails.

## Validation

- TODO: primary runtime target and proof requirements.
- When validating playable or visual changes, capture screenshots and use emulated input.
- If a task reveals repeated friction, propose updating \`AGENTS.md\` or creating/updating a project skill.
`;

const CLAUDE_TEMPLATE = `Project rules, direction, and validation policy: @AGENTS.md
Shared human/agent process: @AI_PIPELINE.md

Work items (tasks, epics, deferred ideas) live in the \`tasks/\` store.
Before planning or finishing work, check it and capture anything deferred:

- Process: \`.codex/skills/task-manager/SKILL.md\`
- Conventions: \`tasks/README.md\`
- CLI: \`node tools/taskboard/cli.mjs <list|show|new|set|validate>\`
- Visual board for the user: \`node tools/taskboard/server.mjs\` -> http://127.0.0.1:8070/
`;

mkdirSync(dst, { recursive: true });
for (const rel of COPY) {
  const from = join(root, rel);
  if (!existsSync(from)) {
    console.warn(`warn: missing ${rel}, skipped`);
    continue;
  }
  const to = join(dst, rel);
  mkdirSync(join(to, ".."), { recursive: true });
  cpSync(from, to, { recursive: true, force });
  console.log(`copied: ${rel}`);
}

mkdirSync(join(dst, "tasks", "epics"), { recursive: true });
mkdirSync(join(dst, "tmp"), { recursive: true });

for (const [name, content] of [["AGENTS.md", AGENTS_TEMPLATE], ["CLAUDE.md", CLAUDE_TEMPLATE]]) {
  const file = join(dst, name);
  if (existsSync(file) && !force) {
    console.log(`kept existing: ${name}`);
  } else {
    writeFileSync(file, content);
    console.log(`wrote: ${name}`);
  }
}

const sync = spawnSync(process.execPath, [join(dst, "tools", "skills_sync.mjs")], {
  cwd: dst,
  stdio: "inherit",
});
if (sync.status !== 0) {
  console.warn("warn: skills_sync failed in target; run it manually");
}

console.log(`\nexported AI base to ${dst} (${basename(root)} -> ${basename(dst)})`);
console.log("next: fill the TODOs in AGENTS.md, add tmp/ to .gitignore, then capture first ideas:");
console.log("  node tools/taskboard/cli.mjs new task --title \"...\" --status idea");
