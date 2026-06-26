#!/usr/bin/env node
// Pre-implementation workflow guard for active game work.
//
// This catches process failures before they turn into more runtime code:
// unresolved lead rejection + feature expansion, explicitly unready reference
// grounding + implementation work, and oversized active runtime files without a
// decomposition/recovery task.

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join, relative, resolve } from "node:path";
import { findRoot, listTasks } from "../../ai_studio/taskboard/lib.mjs";

const DEFAULT_RUNTIME_MAX_LINES = 900;

function usage() {
  console.error(`usage:
  node tools/game_context/workflow_guard.mjs [--root <repo>] [--runtime-max-lines <n>]

Runs active-game workflow guards. Dormant when no active game concept is set.`);
  process.exit(2);
}

function parseArgs(argv) {
  const values = { root: null, runtimeMaxLines: DEFAULT_RUNTIME_MAX_LINES };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") usage();
    if (arg === "--root") {
      values.root = argv[++i] || usage();
    } else if (arg === "--runtime-max-lines") {
      const parsed = Number.parseInt(argv[++i], 10);
      if (!Number.isInteger(parsed) || parsed < 100) usage();
      values.runtimeMaxLines = parsed;
    } else {
      usage();
    }
  }
  return values;
}

function readText(path) {
  try {
    return existsSync(path) ? readFileSync(path, "utf8") : "";
  } catch {
    return "";
  }
}

function activeConcept(root) {
  const gameProject = readText(join(root, "GAME_PROJECT.md"));
  if (/status:\s*none|no active game concept|no active concept is selected/i.test(gameProject)) {
    return { active: false, id: "", evidence: "GAME_PROJECT.md says no active game concept" };
  }
  const idFromField = gameProject.match(/game id:\s*`?([a-z0-9][a-z0-9-]{1,64})`?/i)?.[1] || "";
  if (idFromField) return { active: true, id: idFromField, evidence: `GAME_PROJECT.md game id: ${idFromField}` };
  const folder = gameProject.match(/gamedesign[\\/]+projects[\\/]+([a-z0-9][a-z0-9-]{1,64})/i)?.[1] || "";
  if (folder) return { active: true, id: folder, evidence: `GAME_PROJECT.md game folder: ${folder}` };
  const projectDirs = directories(join(root, "gamedesign", "projects"));
  return { active: projectDirs.length === 1, id: projectDirs.length === 1 ? projectDirs[0] : "", evidence: projectDirs.join(", ") };
}

function directories(path) {
  try {
    return readdirSync(path, { withFileTypes: true }).filter((entry) => entry.isDirectory()).map((entry) => entry.name);
  } catch {
    return [];
  }
}

function normalize(text) {
  return String(text || "")
    .toLowerCase()
    .replace(/[`*_()[\]{}"'.,;:!?/\\|]+/g, " ")
    .replace(/\s+/g, " ")
    .trim();
}

function taskText(task) {
  return normalize([task.fields.title, ...(task.fields.tags || []), task.body].join(" "));
}

function activeTasks(root) {
  return listTasks(root).filter((task) => !["done", "dropped", "idea"].includes(String(task.fields.status || "")));
}

function taskPath(root, task) {
  return relative(root, task.file).replace(/\\/g, "/");
}

function leadRejectionTask(task) {
  const text = taskText(task);
  return /\blead rejection\b|\blead rejected\b|\bvisual rejection\b|\blead-rejection\b/.test(text);
}

function rejectionResolved(task) {
  const text = taskText(task);
  return /\bresolved rejection\b|\blead accepted\b|\blead acceptance\b|\bexplicit lead acceptance\b|\bknown red\b|\baccepted debt\b/.test(text);
}

function featureExpansionTask(task) {
  const text = taskText(task);
  const expansion =
    /\b(content|feature|features|gameplay|combat|enemy|enemies|quest|quests|item|items|economy|progression|level|levels|location|locations|map|depth|reward|systems|playable)\b/.test(text);
  const recovery =
    /\b(lead-rejection|visual|ux|reference|references|digest|mismatch|review|pipeline|architecture|decomposition|monolith|refactor|tooling|cleanup|asset|assets|art|source)\b/.test(text);
  return expansion && !recovery;
}

function implementationTask(task) {
  const text = taskText(task);
  const implementation =
    /\b(native-first|runtime|implementation|implement|code|gameplay|combat|content|feature|features|ui|scene|playable|prototype|quest|item|economy|progression)\b/.test(text);
  const nonImplementation =
    /\b(reference|references|digest|gdd|core loop|core-loop|planning|plan|review|pipeline|architecture|decomposition|monolith|tooling|cleanup)\b/.test(text);
  return implementation && !nonImplementation;
}

function architectureRecoveryTask(task) {
  const text = taskText(task);
  return /\b(architecture|decomposition|decompose|monolith|refactor|runtime split|systems|entities)\b/.test(text);
}

function loadJson(path) {
  try {
    return existsSync(path) ? JSON.parse(readFileSync(path, "utf8")) : null;
  } catch {
    return null;
  }
}

function referenceGroundingStatus(root, conceptId) {
  if (!conceptId) return null;
  const model = loadJson(join(root, "gamedesign", "projects", conceptId, "data", "core_loop.json"));
  if (!model) return null;
  const grounding = model.reference_grounding || model.references || {};
  if (grounding.ready === false) return "not_ready";
  if (typeof grounding.status === "string") return grounding.status;
  if (typeof model.reference_status === "string") return model.reference_status;
  return null;
}

function referenceNotReady(status) {
  return /^(not[-_ ]ready|not_ready_for_implementation|pending|missing|blocked|unready)$/i.test(String(status || ""));
}

function lineCount(path) {
  const text = readText(path);
  return text ? text.split(/\r?\n/).length : 0;
}

function checkLeadRejection(root, tasks, problems) {
  const unresolved = tasks.filter((task) => leadRejectionTask(task) && !rejectionResolved(task));
  if (unresolved.length === 0) return;
  for (const task of tasks) {
    if (unresolved.includes(task)) continue;
    if (!featureExpansionTask(task)) continue;
    problems.push(
      `feature/content expansion is active while lead rejection is unresolved: ${task.fields.id} (${taskPath(root, task)}); fix/review ${unresolved.map((item) => item.fields.id).join(", ")} first.`
    );
  }
}

function checkReferenceReadiness(root, concept, tasks, problems) {
  const status = referenceGroundingStatus(root, concept.id);
  if (!referenceNotReady(status)) return;
  for (const task of tasks) {
    if (!implementationTask(task)) continue;
    problems.push(
      `implementation task ${task.fields.id} is active but ${concept.id}/data/core_loop.json reference_grounding.status is "${status}". Finish reference digest/grounding before runtime work (${taskPath(root, task)}).`
    );
  }
}

function checkRuntimeMonolith(root, tasks, maxLines, problems) {
  const runtime = join(root, "src", "clean_seed_main.c");
  const lines = lineCount(runtime);
  if (lines <= maxLines) return;
  if (tasks.some(architectureRecoveryTask)) return;
  problems.push(
    `active runtime src/clean_seed_main.c is ${lines} lines (> ${maxLines}) with no active architecture/decomposition task. Split systems/entities/files before adding more runtime behavior.`
  );
}

function run(root, options) {
  const concept = activeConcept(root);
  if (!concept.active) {
    return { problems: [], message: "ok: game workflow guard skipped (no active game concept)" };
  }
  const tasks = activeTasks(root);
  const problems = [];
  checkLeadRejection(root, tasks, problems);
  checkReferenceReadiness(root, concept, tasks, problems);
  checkRuntimeMonolith(root, tasks, options.runtimeMaxLines, problems);
  return { problems, message: `ok: game workflow guard passed for active concept${concept.id ? ` ${concept.id}` : ""}` };
}

const options = parseArgs(process.argv.slice(2));
const root = resolve(options.root || process.env.TASKBOARD_ROOT || findRoot());
const result = run(root, options);

if (result.problems.length > 0) {
  for (const problem of result.problems) console.error(`problem: ${problem}`);
  console.error(
    "hint: switch to references/visual-UX fix/architecture recovery, or record explicit lead acceptance before expansion."
  );
  process.exit(1);
}

console.log(result.message);
