#!/usr/bin/env node
// Guard hot AI context entrypoints from silently growing back into large docs.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import {
  DEFAULT_HOT_DOC_MAX_CHARS,
  HOT_DOC_BUDGETS,
  HOT_DOC_TOTAL_MAX_CHARS,
  REVIEW_DEFAULT_HOT_DOC_MAX_CHARS,
  REVIEW_HOT_DOC_TOTAL_MAX_CHARS,
  REVIEW_SKILL_ENTRYPOINT_MAX_CHARS,
  REVIEW_SKILL_ENTRYPOINT_TOTAL_MAX_CHARS,
  SKILL_ENTRYPOINT_MAX_CHARS,
  SKILL_ENTRYPOINT_TOTAL_MAX_CHARS,
} from "./context_budget_config.mjs";

const args = process.argv.slice(2);

function usage() {
  console.error(`usage:
  node tools/context_budget.mjs [--root <dir>] [--review] [--max-skill-chars <n>] [--max-doc-chars <n>] [--json]

Defaults:
  --max-skill-chars ${SKILL_ENTRYPOINT_MAX_CHARS}   max chars for each .codex/skills/*/SKILL.md entrypoint
  --max-doc-chars   ${DEFAULT_HOT_DOC_MAX_CHARS}   max chars for hot root/status instruction docs;
                            when omitted, tighter per-file defaults apply`);
  process.exit(2);
}

function takeNumber(name, fallback) {
  const index = args.indexOf(name);
  if (index === -1) return fallback;
  const value = Number.parseInt(args[index + 1], 10);
  if (!Number.isInteger(value) || value < 1) usage();
  args.splice(index, 2);
  return value;
}

function takeString(name, fallback) {
  const index = args.indexOf(name);
  if (index === -1) return fallback;
  const value = args[index + 1];
  if (!value || value.startsWith("--")) usage();
  args.splice(index, 2);
  return value;
}

const root = resolve(takeString("--root", process.cwd()));
const reviewMode = args.includes("--review");
if (reviewMode) args.splice(args.indexOf("--review"), 1);
const maxSkillChars = takeNumber("--max-skill-chars", reviewMode ? REVIEW_SKILL_ENTRYPOINT_MAX_CHARS : SKILL_ENTRYPOINT_MAX_CHARS);
const maxDocCharsProvided = args.includes("--max-doc-chars");
const maxDocChars = takeNumber("--max-doc-chars", reviewMode ? REVIEW_DEFAULT_HOT_DOC_MAX_CHARS : DEFAULT_HOT_DOC_MAX_CHARS);
const json = args.includes("--json");
if (json) args.splice(args.indexOf("--json"), 1);
if (args.includes("--help") || args.includes("-h")) usage();
if (args.length > 0) usage();

function measure(relPath, kind, maxChars) {
  const abs = join(root, relPath);
  if (!existsSync(abs)) return null;
  const text = readFileSync(abs, "utf8");
  return {
    path: relPath.replaceAll("\\", "/"),
    kind,
    chars: text.length,
    lines: text.split(/\r?\n/).length,
    max_chars: maxChars,
    ok: text.length <= maxChars,
  };
}

function skillEntrypoints() {
  const skillsDir = join(root, ".codex", "skills");
  if (!existsSync(skillsDir)) return [];
  return readdirSync(skillsDir)
    .map((name) => join(".codex", "skills", name, "SKILL.md"))
    .filter((relPath) => {
      try {
        return statSync(join(root, relPath)).isFile();
      } catch {
        return false;
      }
    });
}

const records = [
  ...HOT_DOC_BUDGETS.map((doc) => {
    const perFileMax = reviewMode ? (doc.reviewMaxChars || doc.maxChars) : doc.maxChars;
    const limit = maxDocCharsProvided ? maxDocChars : Math.min(maxDocChars, perFileMax);
    return measure(doc.path, "hot_doc", limit);
  }).filter(Boolean),
  ...skillEntrypoints().map((path) => measure(path, "skill_entrypoint", maxSkillChars)).filter(Boolean),
].sort((a, b) => b.chars - a.chars || a.path.localeCompare(b.path));

const hotDocTotal = records.filter((record) => record.kind === "hot_doc").reduce((sum, record) => sum + record.chars, 0);
const skillTotal = records.filter((record) => record.kind === "skill_entrypoint").reduce((sum, record) => sum + record.chars, 0);
const hotDocTotalMax = reviewMode ? REVIEW_HOT_DOC_TOTAL_MAX_CHARS : HOT_DOC_TOTAL_MAX_CHARS;
const skillTotalMax = reviewMode ? REVIEW_SKILL_ENTRYPOINT_TOTAL_MAX_CHARS : SKILL_ENTRYPOINT_TOTAL_MAX_CHARS;
const totals = [
  { path: "<hot-doc-total>", kind: "aggregate", chars: hotDocTotal, max_chars: hotDocTotalMax, ok: hotDocTotal <= hotDocTotalMax },
  { path: "<skill-entrypoint-total>", kind: "aggregate", chars: skillTotal, max_chars: skillTotalMax, ok: skillTotal <= skillTotalMax },
];
const failures = [...records.filter((record) => !record.ok), ...totals.filter((record) => !record.ok)];

if (json) {
  console.log(JSON.stringify({ ok: failures.length === 0, root, mode: reviewMode ? "review" : "normal", totals, records, failures }, null, 2));
} else {
  console.log("context budget report");
  console.log(`root: ${root}`);
  console.log(`mode: ${reviewMode ? "review" : "normal"}`);
  const docLimit = maxDocCharsProvided ? `${maxDocChars} chars` : "per-file defaults";
  console.log(`limits: skill_entrypoint<=${maxSkillChars} chars, hot_doc<=${docLimit}`);
  console.log(`totals: hot_docs=${hotDocTotal}/${hotDocTotalMax}, skill_entrypoints=${skillTotal}/${skillTotalMax}`);
  console.log("\nTop hot context entrypoints:");
  for (const record of records.slice(0, 12)) {
    const status = record.ok ? "ok" : "FAIL";
    console.log(`- ${status} ${record.path} ${record.chars} chars / ${record.lines} lines (max ${record.max_chars})`);
  }
  if (failures.length === 0) {
    console.log("\nok: context budgets pass");
  } else {
    console.error("\ncontext budget failures:");
    for (const failure of failures) {
      console.error(`- ${failure.path}: ${failure.chars} chars > ${failure.max_chars}`);
    }
  }
}

if (failures.length > 0) process.exit(1);
