// Export the portable AI pipeline base into a new game project.
//
//   node tools/bootstrap/export_base.mjs --target C:\projects\new-game [--force]
//
// Copies the reusable pieces (skills, AI Studio modules, skill sync, design knowledge,
// task store conventions) and writes starter AGENTS.md /
// CLAUDE.md. Existing files in the target are preserved unless --force.
// See ai_studio/README.md and docs/ai-pipeline/profiling-reuse.md for what stays.

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
  "tools/context_budget.mjs",
  "tools/context_budget_config.mjs",
  "tools/context_budget.test.mjs",
  "tools/doc_reference_check.mjs",
  "tools/doc_reference_check.test.mjs",
  "tools/skills_eval.mjs",
  "tools/pipeline_validate.mjs",
  "tools/pipeline_validate.test.mjs",
  "tools/skills_sync.mjs",
  "tools/skills_sync.test.mjs",
  "tools/ai_profile",
  "tools/requirements/ai-pipeline-full.txt",
  "tools/assets/job/new_art_job.mjs",
  "tools/assets/job/plan_source_sheet_prompt.mjs",
  "tools/assets/job/plan_source_sheet_prompt.test.mjs",
  "tools/assets/job/new_generation_record.mjs",
  "tools/assets/job/new_generation_record.test.mjs",
  "tools/assets/job/validate_art_job.mjs",
  "tools/assets/job/validate_art_job.test.mjs",
  "tools/assets/job/audit_project_asset_boundaries.mjs",
  "tools/assets/job/audit_project_asset_boundaries.test.mjs",
  "tools/assets/pack/atlas_review_labels.py",
  "tools/assets/pack/atlas_review_labels_test.py",
  "tools/assets/pack/build_ui_atlas_pack.py",
  "tools/assets/pack/build_ui_atlas_pack_test.py",
  "tools/assets/pack/audit_ui_atlas_pack.py",
  "tools/assets/pack/audit_ui_atlas_pack_test.py",
  "tools/assets/crop/plan_runtime_crops_from_intake.py",
  "tools/assets/crop/plan_runtime_crops_from_intake_test.py",
  "tools/assets/assemble/build_runtime_assets_from_crop_plan.py",
  "tools/assets/assemble/build_runtime_assets_from_crop_plan_test.py",
  "tools/assets/source/find_assets.mjs",
  "tools/assets/source/import_poly_pizza.mjs",
  "tools/assets/restricted.mjs",
  "tools/assets/audit/restricted_assets_guard.mjs",
  "tools/assets/audit/restricted_assets_guard.test.mjs",
  "tools/assets/audit/restricted_assets_allowlist.json",
  "tools/assets/obj_to_glb.py",
  "tools/assets/render_thumbs.py",
  "tools/assets/render_library_previews.mjs",
  "tools/assets/make_studio_hdr.py",
  "tools/assets/studio_env.hdr",
  "tools/assets/intake/accept_incoming_asset.mjs",
  "tools/assets/intake/bootstrap_shared_asset_library.mjs",
  "tools/asset_review/build_review.mjs",
  "tools/asset_review/viewer.js",
  "tools/asset_review/viewer.css",
  "tools/asset_review/promote.mjs",
  "tools/asset_review/pull.mjs",
  "tools/asset_review/serve_gallery.mjs",
  "tools/serve_tunnel.mjs",
  "tools/assets/atomic_io.py",
  "tools/assets/atomic_io_test.py",
  "tools/assets/chroma_key_alpha.py",
  "tools/assets/chroma_key_alpha_test.py",
  "tools/assets/cutout/dual_plate_alpha.py",
  "tools/assets/cutout/dual_plate_alpha_test.py",
  "tools/assets/cutout/dual_plate_pair_gate.py",
  "tools/assets/cutout/dual_plate_pair_gate_test.py",
  "tools/assets/cutout/key_matte.py",
  "tools/assets/cutout/key_matte_test.py",
  "tools/assets/cutout/route_cutout.py",
  "tools/assets/cutout/route_cutout_test.py",
  "tools/assets/intake/normalize_source_sheet_chroma.py",
  "tools/assets/intake/normalize_source_sheet_chroma_test.py",
  "tools/assets/intake/audit_source_sheet_intake.py",
  "tools/assets/intake/audit_source_sheet_intake_test.py",
  "tools/assets/intake/download_source_asset.mjs",
  "tools/assets/intake/download_source_asset.test.mjs",
  "tools/assets/intake/ingest_archive.mjs",
  "tools/lib/hash.mjs",
  "tools/lib/cli.mjs",
  "tools/lib/cli.test.mjs",
  "tools/lib/json.mjs",
  "tools/lib/json.test.mjs",
  "tools/lib/licenses.mjs",
  "tools/lib/licenses.test.mjs",
  "tools/lib/paths.mjs",
  "tools/lib/paths.test.mjs",
  "tools/lib/mime.mjs",
  "tools/lib/mime.test.mjs",
  "tools/lib/tmp_exports.mjs",
  "tools/lib/tmp_exports.test.mjs",
  "tools/lib/validate_flags.mjs",
  "tools/lib/validate_flags.test.mjs",
  "tools/lib/asset_catalog.mjs",
  "tools/lib/asset_catalog.test.mjs",
  "tools/assets/intake/audit_tileable_texture.py",
  "tools/assets/intake/audit_tileable_texture_test.py",
  "tools/game_context",
  "tools/product_gate",
  "tools/architecture_map",
  "tools/README.md",
  "tools/bootstrap/export_base.mjs",
  "tools/bootstrap/export_base.test.mjs",
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
- Work items and live project status live in \`tasks/\`; follow \`ai_studio/taskboard/README.md\`.
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

- Process: \`.codex/skills/task-manager/SKILL.md\`
- Conventions: \`ai_studio/taskboard/README.md\`
- Live status: \`tasks/STATUS.md\`
- CLI: \`node ai_studio/taskboard/cli.mjs <list|show|new|set|validate>\`
- Visual board for the user: \`node ai_studio/taskboard/server.mjs\` -> http://127.0.0.1:8070/
`;

const STATUS_TEMPLATE = `# Project Status

Operational project-status index. Rules for this file live in
\`ai_studio/taskboard/README.md\`.

## Current Goal

No active game concept is selected yet. This is a clean AI-first game project
base waiting for the user's idea.

Sources: \`AGENTS.md\`, \`ai_studio/README.md\`.

## Active Work

None. Do not invent a game concept or create GDD/gameplay/content files until
the user provides the project idea.

Sources: \`AGENTS.md\`.

## Current Gate

Capture the user's game concept and create/refine exactly one scoped task or
epic before implementation.

Source: \`ai_studio/taskboard/README.md\`.

## Required Validation

\`\`\`powershell
node ai_studio/taskboard/cli.mjs summary
node tools/pipeline_validate.mjs --full
\`\`\`

Sources: \`ai_studio/taskboard/README.md\`, \`ai_studio/README.md\`.

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
console.log("  node ai_studio/taskboard/cli.mjs new task --title \"...\" --status idea");
