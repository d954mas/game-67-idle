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
  "tools/ai.mjs",
  "tools/ai.test.mjs",
  "tools/skills_eval.mjs",
  "tools/pipeline_validate.mjs",
  "tools/skills_sync.mjs",
  "tools/ai_profile",
  "tools/assets/new_art_job.mjs",
  "tools/assets/plan_source_sheet_prompt.mjs",
  "tools/assets/plan_source_sheet_prompt.test.mjs",
  "tools/assets/plan_missing_source_family_prompts.mjs",
  "tools/assets/plan_missing_source_family_prompts.test.mjs",
  "tools/assets/new_generation_record.mjs",
  "tools/assets/new_generation_record.test.mjs",
  "tools/assets/validate_art_job.mjs",
  "tools/assets/validate_art_job.test.mjs",
  "tools/assets/audit_slice9_design_policy.mjs",
  "tools/assets/audit_slice9_design_policy.test.mjs",
  "tools/assets/audit_atlas_metadata.mjs",
  "tools/assets/audit_atlas_metadata.test.mjs",
  "tools/assets/build_ui_atlas_pack.py",
  "tools/assets/build_ui_atlas_pack_test.py",
  "tools/assets/audit_ui_atlas_pack.py",
  "tools/assets/audit_ui_atlas_pack_test.py",
  "tools/assets/audit_runtime_ui_asset_usage.mjs",
  "tools/assets/audit_runtime_ui_asset_usage.test.mjs",
  "tools/assets/audit_source_family_coverage.mjs",
  "tools/assets/audit_source_family_coverage.test.mjs",
  "tools/assets/chroma_key_alpha.py",
  "tools/assets/chroma_key_alpha_test.py",
  "tools/assets/dual_plate_alpha.py",
  "tools/assets/dual_plate_alpha_test.py",
  "tools/assets/normalize_source_sheet_chroma.py",
  "tools/assets/normalize_source_sheet_chroma_test.py",
  "tools/assets/audit_source_sheet_intake.py",
  "tools/assets/audit_source_sheet_intake_test.py",
  "tools/assets/audit_generated_ui_assets.py",
  "tools/assets/audit_generated_ui_assets_test.py",
  "tools/assets/render_ui_asset_edge_proof.py",
  "tools/assets/render_ui_asset_edge_proof_test.py",
  "tools/assets/render_ui_composition_proof.py",
  "tools/assets/render_ui_composition_proof_test.py",
  "tools/assets/audit_generated_source_derivation.py",
  "tools/assets/audit_generated_source_derivation_test.py",
  "tools/game_context",
  "tools/product_gate",
  "tools/README.md",
  "tools/tool_layers.json",
  "tools/taskboard",
  "tools/bootstrap/export_base.mjs",
  "gamedesign/knowledge",
  "AI_PIPELINE.md",
  "AI_PIPELINE_SESSION_PROFILING.md",
  "AI_PIPELINE_OBSERVABILITY_TOOLS.md",
  "tasks/README.md",
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
- Work items and live project status live in \`tasks/\`; follow \`tasks/README.md\`.
- Temporary generation, scripts, rejected images, screenshots, and audit logs go in \`tmp/\` or another ignored temp folder.
- Final durable docs/data/assets go in their project folder.
- The shared human/agent process lives in \`AI_PIPELINE.md\`.
- Long-session AI development profiling uses \`AI_PIPELINE_SESSION_PROFILING.md\`;
  raw telemetry stays in \`tmp/session_profiles/\`.
- External AI observability/eval tools are gated by
  \`AI_PIPELINE_OBSERVABILITY_TOOLS.md\`; start local-first and run only
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
Shared human/agent process: @AI_PIPELINE.md

Work items (tasks, epics, deferred ideas) live in the \`tasks/\` store.
For status, task format, and workflow rules, follow \`tasks/README.md\`:

- Process: \`.codex/skills/task-manager/SKILL.md\`
- Conventions: \`tasks/README.md\`
- Live status: \`tasks/STATUS.md\`
- CLI: \`node tools/taskboard/cli.mjs <list|show|new|set|validate>\`
- Visual board for the user: \`node tools/taskboard/server.mjs\` -> http://127.0.0.1:8070/
`;

const STATUS_TEMPLATE = `# Project Status

Operational project-status index. Rules for this file live in
\`tasks/README.md\`.

## Current Goal

No active game concept is selected yet. This is a clean AI-first game project
base waiting for the user's idea.

Sources: \`AGENTS.md\`, \`AI_PIPELINE.md\`.

## Active Work

None. Do not invent a game concept or create GDD/gameplay/content files until
the user provides the project idea.

Sources: \`AGENTS.md\`.

## Current Gate

Capture the user's game concept and create/refine exactly one scoped task or
epic before implementation.

Source: \`tasks/README.md\`.

## Required Validation

\`\`\`powershell
node tools/taskboard/cli.mjs summary
node tools/pipeline_validate.mjs
\`\`\`

Sources: \`tasks/README.md\`, \`AI_PIPELINE.md\`.

## Last Known Good Evidence

Fresh export validation should be recorded here after first local setup.

Source: first local validation after project setup.

## Blockers

No user game concept has been provided yet.

## Non-blocking Debt

None.

## Next Priorities

1. Ask the user for the game concept.
2. Capture the concept as one scoped task or epic.
3. Start the primary GDD pipeline only after the user provides the concept.
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
  ["tasks/STATUS.md", STATUS_TEMPLATE],
]) {
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
console.log("next: choose engine/runtime policy when known, ensure tmp/ is ignored, then capture first ideas:");
console.log("  node tools/taskboard/cli.mjs new task --title \"...\" --status idea");
