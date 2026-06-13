#!/usr/bin/env node
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

function usage() {
  console.error(`usage:
  node tools/game_context/iteration_context.mjs [--json-output <file>] [--status-max-chars <n>]

Builds a compact pre-implementation context pack for playable game work.`);
  process.exit(2);
}

function parseArgs(args) {
  const values = {};
  for (let index = 0; index < args.length; index += 1) {
    const arg = args[index];
    if (arg === "--help" || arg === "-h") usage();
    if (!arg.startsWith("--")) usage();
    const key = arg.slice(2);
    const next = args[index + 1];
    if (next !== undefined && !next.startsWith("--")) {
      values[key] = next;
      index += 1;
    } else {
      values[key] = true;
    }
  }
  return values;
}

function findRoot(start = process.cwd()) {
  let current = resolve(start);
  while (true) {
    if (existsSync(join(current, "AGENTS.md"))) return current;
    const parent = dirname(current);
    if (parent === current) return resolve(start);
    current = parent;
  }
}

function readIfExists(path) {
  try {
    return existsSync(path) ? readFileSync(path, "utf8") : "";
  } catch {
    return "";
  }
}

function sectionText(markdown, title) {
  const pattern = new RegExp(`(?:^|\\r?\\n)## ${escapeRegExp(title)}[ \\t]*\\r?\\n([\\s\\S]*?)(?=\\r?\\n##\\s+|$)`, "i");
  const match = markdown.match(pattern);
  return match ? match[1].trim() : "";
}

function escapeRegExp(value) {
  return String(value).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function bulletContaining(text, patterns, fallback = "") {
  const lines = bulletBlocks(text);
  for (const pattern of patterns) {
    const found = lines.find((line) => pattern.test(line));
    if (found) return found;
  }
  return fallback;
}

function bulletBlocks(text) {
  const blocks = [];
  let current = "";
  for (const rawLine of String(text || "").split(/\r?\n/)) {
    const line = rawLine.trim();
    if (!line) continue;
    if (/^[-*]\s+/.test(line)) {
      if (current) blocks.push(current.trim());
      current = line.replace(/^[-*]\s+/, "");
    } else if (current && !/^#{1,6}\s+/.test(line)) {
      current = `${current} ${line}`;
    }
  }
  if (current) blocks.push(current.trim());
  return blocks;
}

function truncate(text, maxChars) {
  const clean = String(text || "").trim();
  if (clean.length <= maxChars) return clean;
  return `${clean.slice(0, Math.max(0, maxChars - 80)).replace(/\s+$/, "")}\n\n... truncated ${clean.length - maxChars} chars; inspect source file for full context.`;
}

function runTaskContext(root, maxChars) {
  const result = spawnSync(process.execPath, [
    "tools/taskboard/cli.mjs",
    "context",
    "--status-max-chars",
    String(maxChars),
    "--tasks-limit",
    "12",
  ], {
    cwd: root,
    encoding: "utf8",
    shell: false,
  });
  if (result.status !== 0) return "";
  return result.stdout || "";
}

function existing(root, paths) {
  return paths.filter((path) => existsSync(join(root, path)));
}

function buildContext(root, options = {}) {
  const maxStatusChars = Number.isFinite(Number(options.statusMaxChars)) ? Number(options.statusMaxChars) : 5000;
  const agents = readIfExists(join(root, "AGENTS.md"));
  const project = sectionText(agents, "Project");
  const direction = sectionText(agents, "Direction");
  const validation = sectionText(agents, "Validation");
  const taskContext = runTaskContext(root, Math.max(1200, maxStatusChars));

  const concept = bulletContaining(project, [/Active game concept/i, /No active game concept/i, /No concept selected/i], "No active concept found in AGENTS.md.");
  const productTarget = bulletContaining(direction, [/current product target/i, /release-quality/i, /product target/i], "");
  const runtimeSurface = bulletContaining(direction, [/current runtime surface/i, /src\/main\.c/i, /placeholder/i], "");
  const nativeGate = bulletContaining(validation, [/Native desktop\/PC/i, /native PC/i, /preferred development/i], "");
  const webGate = bulletContaining(validation, [/web prototype/i, /web server/i, /localhost/i, /browser\/frontend/i], "");
  const referenceGate = bulletContaining(direction, [/Reference study is a hard implementation gate/i, /Reference Lock/i], "");
  const visualGate = bulletContaining(direction, [/generated-art/i, /visual work/i, /game-visual-art-direction/i], "");

  const designSources = existing(root, [
    "gamedesign/meme-evolution/gdd.md",
    "gamedesign/meme-evolution/cow_evolution_deconstruction_v2.md",
    "gamedesign/meme-evolution/reference_research.md",
    "gamedesign/meme-evolution/asset_pipeline.md",
    "gamedesign/meme-evolution/data/balance.json",
    "gamedesign/meme-evolution/data/ui_flow.json",
    "gamedesign/meme-evolution/data/asset_manifest.json",
    "gamedesign/meme-evolution/art_requests/67-world-reusable-ui-v1.json",
  ]);
  const runtimeSources = existing(root, [
    "src/main.c",
    "CMakePresets.json",
    "tools/devapi",
    "tools/project_67_world/balance/simulate_67_world.py",
    "tools/project_67_world/package_native_release.mjs",
  ]);

  const currentGate = sectionText(taskContext, "Current Gate");
  const nextPriorities = sectionText(taskContext, "Next Priorities");
  const blockers = sectionText(taskContext, "Blocking Work") || sectionText(taskContext, "Blockers");
  const requiredValidation = sectionText(taskContext, "Required Validation");

  const hardGates = [
    nativeGate || "Use the primary native/game runtime first; discover local build/run rules before implementation.",
    webGate || "Do not use web/browser work unless the current user request explicitly allows it.",
    referenceGate || "If a named reference drives gameplay/art/balance, create or update a durable reference deconstruction before implementation.",
    visualGate || "For polished/generated visual work, use an art target/job and reusable runtime assets before code polish.",
  ];

  return {
    schema_version: 1,
    root,
    generated_at: new Date().toISOString(),
    concept,
    product_target: productTarget,
    runtime_surface: runtimeSurface,
    hard_gates: hardGates,
    design_sources: designSources,
    runtime_sources: runtimeSources,
    current_gate: truncate(currentGate, 1600),
    next_priorities: truncate(nextPriorities, 1200),
    blockers: truncate(blockers, 1000),
    required_validation: truncate(requiredValidation, 1400),
    before_coding_checklist: [
      "Name the selected runtime harness and why it is allowed.",
      "If reference-driven, cite the durable deconstruction/digest and next native proof.",
      "If visual/UI work, cite the accepted target or art request and reusable asset strategy.",
      "Name the smallest playable slice and the native screenshot/scenario that will prove it.",
      "Start or set the AI profile scope for the task before substantial commands.",
    ],
  };
}

function renderMarkdown(context) {
  const lines = [];
  lines.push("# Game Iteration Context Pack");
  lines.push("");
  lines.push(`Generated: ${context.generated_at}`);
  lines.push(`Root: ${context.root}`);
  lines.push("");
  lines.push("## Active Direction");
  lines.push(`- concept: ${context.concept}`);
  if (context.product_target) lines.push(`- product target: ${context.product_target}`);
  if (context.runtime_surface) lines.push(`- runtime surface: ${context.runtime_surface}`);
  lines.push("");
  lines.push("## Hard Gates Before Coding");
  for (const gate of context.hard_gates) lines.push(`- ${gate}`);
  lines.push("");
  lines.push("## Current Project Gate");
  lines.push(context.current_gate || "- none found; run `node tools/taskboard/cli.mjs context`.");
  lines.push("");
  lines.push("## Next Priorities");
  lines.push(context.next_priorities || "- none found.");
  lines.push("");
  lines.push("## Blocking Work");
  lines.push(context.blockers || "- none found.");
  lines.push("");
  lines.push("## Required Validation");
  lines.push(context.required_validation || "- none found; use native build plus targeted scenario proof.");
  lines.push("");
  lines.push("## Source Files To Inspect");
  lines.push("- design:");
  for (const path of context.design_sources) lines.push(`  - ${path}`);
  if (context.design_sources.length === 0) lines.push("  - none found");
  lines.push("- runtime:");
  for (const path of context.runtime_sources) lines.push(`  - ${path}`);
  if (context.runtime_sources.length === 0) lines.push("  - none found");
  lines.push("");
  lines.push("## Before Coding Checklist");
  for (const item of context.before_coding_checklist) lines.push(`- ${item}`);
  return `${lines.join("\n")}\n`;
}

const args = parseArgs(process.argv.slice(2));
const root = findRoot();
const context = buildContext(root, { statusMaxChars: args["status-max-chars"] });
const markdown = renderMarkdown(context);

if (args["json-output"]) {
  const target = resolve(String(args["json-output"]));
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(context, null, 2)}\n`, "utf8");
}

process.stdout.write(markdown);
