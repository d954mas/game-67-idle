#!/usr/bin/env node
import { existsSync, readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, resolve } from "node:path";

function usage() {
  console.error(`usage:
  node tools/assets/audit_project_asset_boundaries.mjs --name <boundary> --file <path> [--file <path> ...] --forbidden-pattern <regex> [--forbidden-pattern <regex> ...] [--json-output <path>]

Fails when project-owned generated files or builders contain another project's ids/imports.`);
  process.exit(2);
}

function parseArgs(argv) {
  const out = { files: [], forbiddenPatterns: [] };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") usage();
    const value = argv[index + 1];
    if (arg === "--name" && value) {
      out.name = value;
      index += 1;
    } else if (arg === "--file" && value) {
      out.files.push(value);
      index += 1;
    } else if (arg === "--forbidden-pattern" && value) {
      out.forbiddenPatterns.push(value);
      index += 1;
    } else if (arg === "--json-output" && value) {
      out.jsonOutput = value;
      index += 1;
    } else {
      usage();
    }
  }
  if (!out.name || out.files.length === 0 || out.forbiddenPatterns.length === 0) usage();
  return out;
}

function lineNumberAt(text, index) {
  return text.slice(0, index).split(/\r?\n/).length;
}

function audit(options) {
  const problems = [];
  const patterns = options.forbiddenPatterns.map((pattern) => new RegExp(pattern, "g"));
  for (const file of options.files) {
    if (!existsSync(file)) {
      problems.push({ file, pattern: "missing_file", line: 0, match: "" });
      continue;
    }
    const text = readFileSync(file, "utf8");
    for (const pattern of patterns) {
      pattern.lastIndex = 0;
      let match = pattern.exec(text);
      while (match) {
        problems.push({
          file,
          pattern: pattern.source,
          line: lineNumberAt(text, match.index),
          match: match[0],
        });
        match = pattern.exec(text);
      }
    }
  }
  return {
    schema: "game.project_asset_boundary_audit",
    version: 1,
    name: options.name,
    verdict: problems.length === 0 ? "pass" : "fail",
    files: options.files,
    forbidden_patterns: options.forbiddenPatterns,
    problems,
  };
}

const options = parseArgs(process.argv.slice(2));
const report = audit(options);
if (options.jsonOutput) {
  const target = resolve(options.jsonOutput);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(report, null, 2)}\n`, "utf8");
}
if (report.verdict === "pass") {
  console.log(`pass: ${report.name} asset boundary clean (${report.files.length} file(s))`);
} else {
  for (const problem of report.problems) {
    console.log(`problem: ${problem.file}:${problem.line} matched ${problem.pattern} (${problem.match})`);
  }
  process.exit(1);
}
