#!/usr/bin/env node
// Guard hot AI context entrypoints from silently growing back into large docs.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, resolve } from "node:path";

const args = process.argv.slice(2);

function usage() {
  console.error(`usage:
  node tools/context_budget.mjs [--root <dir>] [--max-skill-chars <n>] [--max-doc-chars <n>] [--json]

Defaults:
  --max-skill-chars 3000   max chars for each .codex/skills/*/SKILL.md entrypoint
  --max-doc-chars   6500   max chars for hot root/status instruction docs;
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
const maxSkillChars = takeNumber("--max-skill-chars", 3000);
const maxDocCharsProvided = args.includes("--max-doc-chars");
const maxDocChars = takeNumber("--max-doc-chars", 6500);
const json = args.includes("--json");
if (json) args.splice(args.indexOf("--json"), 1);
if (args.includes("--help") || args.includes("-h")) usage();
if (args.length > 0) usage();

const hotDocs = [
  { path: "AGENTS.md", maxChars: 3600 },
  { path: "AI_PIPELINE.md", maxChars: 2200 },
  { path: join("tasks", "STATUS.md"), maxChars: 3200 },
  { path: join("tasks", "README.md"), maxChars: 3300 },
];

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
  ...hotDocs.map((doc) => {
    const limit = maxDocCharsProvided ? maxDocChars : Math.min(maxDocChars, doc.maxChars);
    return measure(doc.path, "hot_doc", limit);
  }).filter(Boolean),
  ...skillEntrypoints().map((path) => measure(path, "skill_entrypoint", maxSkillChars)).filter(Boolean),
].sort((a, b) => b.chars - a.chars || a.path.localeCompare(b.path));

const failures = records.filter((record) => !record.ok);

if (json) {
  console.log(JSON.stringify({ ok: failures.length === 0, root, records, failures }, null, 2));
} else {
  console.log("context budget report");
  console.log(`root: ${root}`);
  const docLimit = maxDocCharsProvided ? `${maxDocChars} chars` : "per-file defaults";
  console.log(`limits: skill_entrypoint<=${maxSkillChars} chars, hot_doc<=${docLimit}`);
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
