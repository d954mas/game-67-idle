#!/usr/bin/env node
// Check local Markdown references in agent-facing docs.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, normalize, resolve } from "node:path";

const args = process.argv.slice(2);

function usage() {
  console.error(`usage:
  node tools/doc_reference_check.mjs [--root <dir>]`);
  process.exit(2);
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
if (args.includes("--help") || args.includes("-h")) usage();
if (args.length > 0) usage();

const roots = [
  "AGENTS.md",
  join("ai_studio"),
  join("docs", "ai-pipeline"),
  join("tools", "README.md"),
  join("tasks"),
  join(".codex", "skills"),
];

function walkMarkdown(path) {
  if (!existsSync(path)) return [];
  const stat = statSync(path);
  if (stat.isFile()) return path.endsWith(".md") ? [path] : [];
  if (!stat.isDirectory()) return [];
  const relative = rel(path);
  if (relative === "tasks/archive" || relative.startsWith("tasks/archive/")) return [];
  const out = [];
  for (const name of readdirSync(path)) {
    out.push(...walkMarkdown(join(path, name)));
  }
  return out;
}

function rel(path) {
  return normalize(path).replace(normalize(root), "").replace(/^[\\/]/, "").replaceAll("\\", "/");
}

function stripCodeFences(text) {
  return text.replace(/```[\s\S]*?```/g, "");
}

function isExternal(value) {
  return /^(https?:|mailto:|#)/i.test(value);
}

function cleanReference(value) {
  let ref = value.trim();
  if (!ref || isExternal(ref)) return null;
  ref = ref.split("#", 1)[0].trim();
  if (!/\.(md|mjs|py|sh|json)$/.test(ref)) return null;
  if (/[<>*?]/.test(ref)) return null;
  return ref.replaceAll("\\", "/");
}

// Game-runtime tooling each project regenerates against its own engine/runtime.
// The portable export base (tools/bootstrap/export_base.mjs) intentionally omits
// these whole subsystems, so a reference into one is tolerated ONLY when the
// subsystem dir itself is absent. A missing file inside a present subsystem is
// still a real stale/renamed reference and fails.
const REGENERATED_SUBSYSTEMS = ["tools/devapi", "tools/state_codegen"];

function isOmittedSubsystemReference(ref) {
  for (const prefix of REGENERATED_SUBSYSTEMS) {
    if ((ref === prefix || ref.startsWith(`${prefix}/`)) && !existsSync(join(root, prefix))) {
      return true;
    }
  }
  return false;
}

function isPathLikeBacktick(ref) {
  return (
    ref.startsWith("./") ||
    ref.startsWith("../") ||
    ref.startsWith(".codex/") ||
    ref.startsWith("gamedesign/") ||
    ref.startsWith("tasks/") ||
    ref.startsWith("tools/") ||
    ref.startsWith("ai_studio/") ||
    ref.startsWith("scripts/") ||
    ref.startsWith("state/") ||
    ref.startsWith("AGENTS.md") ||
    ref.startsWith("docs/") ||
    ref.startsWith("README.md")
  );
}

function resolveReference(sourceFile, ref) {
  if (ref.startsWith("./") || ref.startsWith("../")) {
    return resolve(dirname(sourceFile), ref);
  }
  if (existsSync(join(dirname(sourceFile), ref))) {
    return resolve(dirname(sourceFile), ref);
  }
  return resolve(root, ref);
}

const files = roots.flatMap((entry) => walkMarkdown(join(root, entry)));
const problems = [];
const retiredCommandPatterns = [
  {
    pattern: /\bnode\s+tools[\\/]ai\.mjs\b/i,
    message:
      "retired command `node tools/ai.mjs`; call the owning CLI directly",
  },
];
const retiredPhrasePatterns = [
  {
    pattern: /\bcontext pressure\b/i,
    message:
      "retired phrase `context pressure`; use `context/cap review` so normal validation is not implied to be a budget gate",
  },
];

for (const file of files) {
  const rawText = readFileSync(file, "utf8");
  for (const retired of retiredCommandPatterns) {
    if (retired.pattern.test(rawText)) {
      problems.push(`${rel(file)} -> ${retired.message}`);
    }
  }
  for (const retired of retiredPhrasePatterns) {
    if (retired.pattern.test(rawText)) {
      problems.push(`${rel(file)} -> ${retired.message}`);
    }
  }

  const text = stripCodeFences(rawText);
  const candidates = [];
  for (const match of text.matchAll(/\[[^\]]+\]\(([^)]+)\)/g)) {
    candidates.push({ value: match[1], kind: "link" });
  }
  for (const match of text.matchAll(/`([^`]+\.(?:md|mjs|py|sh|json)(?:#[^`]*)?)`/g)) {
    candidates.push({ value: match[1], kind: "backtick" });
  }

  for (const candidate of candidates) {
    const ref = cleanReference(candidate.value);
    if (!ref) continue;
    if (candidate.kind === "backtick" && !isPathLikeBacktick(ref)) continue;
    const target = resolveReference(file, ref);
    if (!target.startsWith(root) || !existsSync(target)) {
      if (isOmittedSubsystemReference(ref)) continue;
      problems.push(`${rel(file)} -> ${candidate.value}`);
    }
  }
}

if (problems.length > 0) {
  console.error("doc reference check failed:");
  for (const problem of problems) console.error(`- ${problem}`);
  process.exit(1);
}

console.log(`ok: ${files.length} markdown file(s) checked for local file references`);
