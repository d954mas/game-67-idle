#!/usr/bin/env node
import { resolve } from "node:path";
import { canonicalTaskLogPayloads, findRoot, listTasks } from "../taskboard/store.mjs";

const OUTCOMES = ["pass", "block", "review", "skip", "unverified"];
const GROUPS = {
  QCLR: "player_clarity",
  QART: "art",
  QGDD: "gdd",
  QDES: "game_design",
  QTECH: "technical",
  QASSET: "assets",
};

function usage() {
  console.error(`usage:
  node ai_studio/quality/profile.mjs [--root <repo>] [--include-archive] [--json]

Scans task logs for lines like:
  Quality: QCLR_001=pass; QART_001=block; QTECH_001=unverified; evidence: screenshot + visual target mismatch`);
  process.exit(2);
}

function parseArgs(argv) {
  const out = { root: null, includeArchive: false, json: false };
  for (let index = 0; index < argv.length; index += 1) {
    const arg = argv[index];
    if (arg === "--help" || arg === "-h") usage();
    if (arg === "--include-archive") out.includeArchive = true;
    else if (arg === "--json") out.json = true;
    else if (arg === "--root") {
      const value = argv[++index];
      if (!value || value.startsWith("--")) usage();
      out.root = value;
    } else {
      usage();
    }
  }
  return out;
}

function qualityLines(body) {
  return canonicalTaskLogPayloads(body)
    .filter((line) => /\bQuality:\s*/i.test(line));
}

function parseQualityLine(line) {
  const entries = [];
  const text = line.replace(/^[-*]\s*/, "");
  const pattern = /\b(Q(?:CLR|ART|GDD|DES|TECH|ASSET)_\d{3})\s*(?:=|:|\s)\s*(pass|block|review|skip|unverified)\b/gi;
  let match;
  while ((match = pattern.exec(text))) {
    entries.push({
      rule: match[1].toUpperCase(),
      outcome: match[2].toLowerCase(),
    });
  }
  return entries;
}

function createEmptyRule(rule) {
  const groupKey = rule.replace(/_\d{3}$/, "");
  return {
    rule,
    group: GROUPS[groupKey] || "unknown",
    total: 0,
    outcomes: Object.fromEntries(OUTCOMES.map((outcome) => [outcome, 0])),
    tasks: [],
  };
}

function buildProfile(root, options = {}) {
  const tasks = listTasks(root, { includeArchive: options.includeArchive });
  const byRule = new Map();
  let scannedLines = 0;
  let matchedEntries = 0;

  for (const task of tasks) {
    const taskId = String(task.fields.id || task.name || "");
    for (const line of qualityLines(task.body)) {
      scannedLines += 1;
      for (const entry of parseQualityLine(line)) {
        matchedEntries += 1;
        if (!byRule.has(entry.rule)) byRule.set(entry.rule, createEmptyRule(entry.rule));
        const item = byRule.get(entry.rule);
        item.total += 1;
        item.outcomes[entry.outcome] += 1;
        if (!item.tasks.includes(taskId)) item.tasks.push(taskId);
      }
    }
  }

  const rules = [...byRule.values()].sort((a, b) => a.rule.localeCompare(b.rule));
  return {
    schema: "ai_studio.quality_profile",
    report_kind: "advisory-task-log-summary",
    root,
    include_archive: Boolean(options.includeArchive),
    tasks_scanned: tasks.length,
    quality_lines: scannedLines,
    entries: matchedEntries,
    rules,
  };
}

function renderText(profile) {
  const lines = [
    "# Quality Rule Profile",
    "",
    "Report kind: advisory task-log summary; not enforcement.",
    `Tasks scanned: ${profile.tasks_scanned}`,
    `Quality lines: ${profile.quality_lines}`,
    `Rule entries: ${profile.entries}`,
    "",
  ];
  if (profile.rules.length === 0) {
    lines.push("No quality rule usage found.");
    return `${lines.join("\n")}\n`;
  }
  lines.push("| Rule | Group | Total | Pass | Block | Review | Skip | Unverified | Tasks |");
  lines.push("| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |");
  for (const item of profile.rules) {
    lines.push(
      `| ${item.rule} | ${item.group} | ${item.total} | ${item.outcomes.pass} | ${item.outcomes.block} | ${item.outcomes.review} | ${item.outcomes.skip} | ${item.outcomes.unverified} | ${item.tasks.join(", ")} |`
    );
  }
  return `${lines.join("\n")}\n`;
}

const options = parseArgs(process.argv.slice(2));
const root = resolve(options.root || process.env.TASKBOARD_ROOT || findRoot());
const profile = buildProfile(root, options);

if (options.json) {
  console.log(JSON.stringify(profile, null, 2));
} else {
  process.stdout.write(renderText(profile));
}
