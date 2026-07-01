#!/usr/bin/env node
// Check local Markdown references in agent-facing docs.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, normalize, resolve } from "node:path";

const args = process.argv.slice(2);

function usage() {
  console.error(`usage:
  node ai_studio/core_harness/validation/doc_reference_check.mjs [--root <dir>]`);
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

const checkedRoots = [
  "AGENTS.md",
  "CLAUDE.md",
  join("ai_studio", "README.md"),
  join("ai_studio", "core_harness"),
];

function walkMarkdown(path) {
  if (!existsSync(path)) return [];
  const stat = statSync(path);
  if (stat.isFile()) return path.endsWith(".md") ? [path] : [];
  if (!stat.isDirectory()) return [];
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

function isPathLikeBacktick(ref) {
  return (
    ref.startsWith("./") ||
    ref.startsWith("../") ||
    ref.startsWith(".codex/") ||
    ref.startsWith("ai_studio/game_design/knowledge_base/") ||
    ref.startsWith("ai_studio/taskboard/items/") ||
    ref.startsWith("ai_studio/") ||
    ref.startsWith("state/") ||
    ref.startsWith("AGENTS.md") ||
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

const files = checkedRoots.flatMap((entry) => walkMarkdown(join(root, entry)));
const problems = [];
for (const file of files) {
  const rawText = readFileSync(file, "utf8");
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
