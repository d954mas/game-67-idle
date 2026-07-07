#!/usr/bin/env node
// One-shot migration for the export opt-in inversion (T0332 B1, build-spec "ЭКСПОРТ —
// ИНВЕРСИЯ НА OPT-IN", lead 2026-07-07). Before this increment, exportProject exported
// every visible TOP-LEVEL group except a recipe/style card; after it, exportProject
// exports ONLY a group carrying the explicit `screen === true` flag (patchGroup/
// group-set --screen). For an EXISTING project this script restores today's exact export
// set by flagging every top-level VISIBLE group that carries none of recipe/style/
// pack_run (ops.migrateScreenFlags does the actual per-project work — see its own doc for
// the exact rule, including why a pack_run run-group is deliberately NOT auto-flagged even
// though the OLD filter would technically have exported one).
//
// Dry-run by default (prints what WOULD be flagged, writes nothing) — pass --apply to
// actually write. DO NOT run this with --apply against the real canvasProjectsRoot
// without the lead's go-ahead; verification during development is via a tmp
// CANVAS_PROJECTS_ROOT fixture only (see tests/migrate_screen_flags.test.mjs).
//
// Usage:
//   node ai_studio/assets/canvas/tools/migrate_screen_flags.mjs          (dry-run; prints the plan)
//   node ai_studio/assets/canvas/tools/migrate_screen_flags.mjs --apply  (writes; one journal entry per touched project)
import { resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { isMain } from "../../../core_harness/tool_lib/cli.mjs";
import { listProjects, migrateScreenFlags } from "../ops.mjs";

const repoRoot = resolve(fileURLToPath(new URL("../../../..", import.meta.url)));

// Preview-only: replays migrateScreenFlags' own qualifying-group rule WITHOUT calling it
// (no write), so --apply and the dry-run print byte-identical group lists — one rule,
// read here for the printout, applied for real by the op itself on --apply.
function planFor(project) {
  const groups = Array.isArray(project.groups) ? project.groups : [];
  return groups
    .filter((group) => group.parentId == null && group.visible !== false && !("screen" in group) && !group.recipe && !group.style && !group.pack_run)
    .map((group) => ({ id: group.id, name: group.name || group.id }));
}

export function run(root, { apply = false } = {}) {
  const projects = listProjects(root);
  const summary = { projectsScanned: projects.length, projectsTouched: 0, groupsFlagged: 0, details: [] };

  for (const project of projects) {
    const plan = planFor(project);
    if (!plan.length) continue;

    if (apply) {
      const { flagged } = migrateScreenFlags(root, { projectId: project.id });
      if (!flagged.length) continue; // idempotent re-run / raced with a manual edit: nothing landed
      summary.projectsTouched += 1;
      summary.groupsFlagged += flagged.length;
      summary.details.push({ projectId: project.id, title: project.title || project.id, groups: flagged });
    } else {
      summary.projectsTouched += 1;
      summary.groupsFlagged += plan.length;
      summary.details.push({ projectId: project.id, title: project.title || project.id, groups: plan.map((g) => g.id) });
    }
  }
  return summary;
}

function main() {
  const apply = process.argv.includes("--apply");
  console.log(`migrate_screen_flags: scanning canvas projects [${apply ? "APPLY" : "DRY-RUN"}]`);
  const summary = run(repoRoot, { apply });
  for (const row of summary.details) {
    console.log(`  ${row.title} (${row.projectId}): ${row.groups.length} group(s) -> screen:true [${row.groups.join(", ")}]`);
  }
  console.log(
    `\n${apply ? "flagged" : "would flag"} ${summary.groupsFlagged} group(s) across ${summary.projectsTouched}/${summary.projectsScanned} project(s).`,
  );
  if (!apply) console.log("dry-run only - pass --apply to write.");
}

if (isMain(import.meta.url)) {
  main();
}
