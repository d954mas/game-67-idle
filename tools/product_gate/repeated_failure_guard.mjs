#!/usr/bin/env node
// Fails when an active task keeps repeating the same strict/product gate FAIL
// without a different path: architecture/tooling/source support task or lead
// acceptance. This catches visual-polish loops while still allowing known debt.

import { relative, resolve } from "node:path";
import { findRoot, listTasks } from "../../ai_studio/taskboard/lib.mjs";

const SUPPORT_WORDS = [
  "architecture",
  "architectural",
  "tooling",
  "pipeline",
  "source",
  "asset",
  "assets",
  "engine",
  "render-target",
  "render target",
  "framebuffer",
  "api",
  "rework",
  "redesign",
];

const STOP_WORDS = new Set([
  "product",
  "strict",
  "gate",
  "fail",
  "failed",
  "failure",
  "next",
  "review",
  "screenshot",
  "desktop",
  "because",
  "needs",
  "need",
  "still",
  "with",
  "from",
  "into",
  "before",
  "after",
  "while",
  "quality",
  "audience",
  "readability",
]);

function usage() {
  console.error(`usage:
  node tools/product_gate/repeated_failure_guard.mjs [--root <repo>] [--max-repeat <n>] [--include-archive]

Default: active tasks only, max-repeat=2. A third same-reason FAIL requires a
support task or explicit lead acceptance.`);
  process.exit(2);
}

function parseArgs(argv) {
  const out = { maxRepeat: 2, includeArchive: false, root: null };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--help" || arg === "-h") usage();
    if (arg === "--include-archive") {
      out.includeArchive = true;
    } else if (arg === "--root") {
      out.root = argv[++i] || usage();
    } else if (arg === "--max-repeat") {
      const parsed = Number.parseInt(argv[++i], 10);
      if (!Number.isInteger(parsed) || parsed < 1) usage();
      out.maxRepeat = parsed;
    } else {
      usage();
    }
  }
  return out;
}

function logEntries(body) {
  const lines = String(body || "").split(/\r?\n/);
  const entries = [];
  let current = null;
  for (const line of lines) {
    if (/^- \d{4}-\d{2}-\d{2}:/.test(line)) {
      if (current) entries.push(current.trim());
      current = line;
    } else if (current) {
      current += `\n${line}`;
    }
  }
  if (current) entries.push(current.trim());
  return entries;
}

function hasGateFail(text) {
  const flat = text.toLowerCase();
  return (
    /\b(product|strict)\s+gate\b[\s\S]{0,180}\bfail\b/i.test(flat) ||
    /\bfail\b[\s\S]{0,180}\b(product|strict)\s+gate\b/i.test(flat) ||
    /\bproduct\s+gate\s+remains\s+fail\b/i.test(flat) ||
    /\[[a-z][a-z0-9-]+\]\s*:\s*fail\b/i.test(flat)
  );
}

// Optional explicit one-line gate verdict: [GATE-ID]: PASS|CONCERNS|FAIL.
function gateVerdict(text) {
  const match = String(text || "").match(/\[([a-z][a-z0-9-]+)\]\s*:\s*(pass|concerns|fail)\b/i);
  return match ? { gate: match[1].toUpperCase(), verdict: match[2].toUpperCase() } : null;
}

function normalizeWords(text) {
  return String(text || "")
    .toLowerCase()
    .replace(/[`*_()[\]{}"'.,;:!?/\\|]+/g, " ")
    .replace(/\s+/g, " ")
    .trim();
}

function axisSignature(text) {
  const flat = normalizeWords(text);
  const axes = [];
  const checks = [
    ["art_quality", /\bart quality\b|\bart_quality\b|\bmaterial\b|\blighting\b/],
    ["audience_fit", /\baudience fit\b|\baudience_fit\b/],
    ["readability", /\breadability\b|\btext\b|\bui\b/],
    ["action_direction", /\baction direction\b|\baction_direction\b/],
    ["performance_control", /\bperformance\b|\bframe\b|\bmouse\b|\bcontrol\b/],
    ["game_loop", /\bcore loop\b|\bgame loop\b|\breplay\b|\bfun\b/],
  ];
  for (const [axis, pattern] of checks) {
    if (pattern.test(flat)) axes.push(axis);
  }
  return axes.length ? axes.join("+") : "";
}

function reasonSignature(text) {
  const axis = axisSignature(text);
  if (axis) return axis;
  const normalized = normalizeWords(text);
  const failFor = normalized.match(/\bfail(?:ed|s|ure)?\s+for\s+(.{6,120}?)(?:\s+next\b|$)/);
  const next = normalized.match(/\bnext\s+(.{6,140})$/);
  const source = failFor?.[1] || next?.[1] || normalized;
  const words = source
    .split(/\s+/)
    .filter((word) => word.length >= 5 && !STOP_WORDS.has(word))
    .slice(0, 8);
  return words.join("_") || "unknown";
}

// Cluster key for a FAIL entry: prefer an explicit gate id + reason so the same
// gate's repeated FAILs group; fall back to the axis/reason heuristic.
function failSignature(text) {
  const reason = reasonSignature(text);
  const verdict = gateVerdict(text);
  return verdict && verdict.verdict === "FAIL" ? `${verdict.gate}:${reason}` : reason;
}

function leadAccepted(text) {
  return /lead (accepted|acceptance|explicitly accept|explicitly accepted)|accepted (debt|known red|known-red|red gate)|known[- ]red|лид.{0,40}(принял|одобрил)|принят.{0,40}(долг|риск)/i.test(
    text
  );
}

function supportTask(doc) {
  const status = String(doc.fields.status || "");
  if (["done", "dropped", "idea"].includes(status)) return false;
  const haystack = normalizeWords([
    doc.fields.title,
    ...(doc.fields.tags || []),
    doc.body,
  ].join(" "));
  return SUPPORT_WORDS.some((word) => haystack.includes(word));
}

function topicWords(doc, entries) {
  const text = normalizeWords([
    doc.fields.title,
    ...(doc.fields.tags || []),
    entries.slice(-3).join(" "),
  ].join(" "));
  return new Set(
    text
      .split(/\s+/)
      .filter((word) => word.length >= 5 && !STOP_WORDS.has(word))
      .filter((word) => !/^\d+$/.test(word))
  );
}

function overlapsSupport(task, entries, supportDocs) {
  const topics = topicWords(task, entries);
  if (topics.size === 0) return false;
  for (const support of supportDocs) {
    if (support.fields.id === task.fields.id) continue;
    const haystack = normalizeWords([
      support.fields.title,
      ...(support.fields.tags || []),
      support.body,
    ].join(" "));
    for (const topic of topics) {
      if (haystack.includes(topic)) return true;
    }
  }
  return false;
}

// Count TOTAL occurrences per signature (not only consecutive runs), so an agent
// that interleaves two failing axes cannot slip a 3rd same-axis FAIL past the
// guard by alternating reasons.
function repeatedFailures(task, maxRepeat) {
  const entries = logEntries(task.body).filter(hasGateFail);
  const bySignature = new Map();
  for (const entry of entries) {
    const signature = failSignature(entry);
    if (!bySignature.has(signature)) bySignature.set(signature, []);
    bySignature.get(signature).push(entry);
  }
  const repeats = [];
  for (const [signature, sigEntries] of bySignature) {
    if (sigEntries.length >= maxRepeat + 1) {
      repeats.push({ signature, entries: sigEntries });
    }
  }
  return repeats;
}

const options = parseArgs(process.argv.slice(2));
const root = resolve(options.root || process.env.TASKBOARD_ROOT || findRoot());
const tasks = listTasks(root, { includeArchive: options.includeArchive });
const activeTasks = tasks.filter((task) => !["done", "dropped", "idea"].includes(String(task.fields.status || "")));
const supportDocs = activeTasks.filter(supportTask);
const problems = [];

for (const task of activeTasks) {
  const allEntriesText = logEntries(task.body).join("\n");
  for (const repeat of repeatedFailures(task, options.maxRepeat)) {
    if (leadAccepted(allEntriesText)) continue;
    if (overlapsSupport(task, repeat.entries, supportDocs)) continue;
    problems.push({ task, repeat });
  }
}

if (problems.length > 0) {
  for (const problem of problems) {
    const taskPath = relative(root, problem.task.file).replace(/\\/g, "/");
    console.error(
      `problem: ${problem.task.fields.id} repeats strict/product FAIL "${problem.repeat.signature}" ${problem.repeat.entries.length} times without architecture/tooling/source support task or lead acceptance (${taskPath})`
    );
  }
  console.error(
    "hint: create/link a support task for the different path, record explicit lead acceptance/known-red debt, or stop polishing the same failing approach."
  );
  process.exit(1);
}

console.log(
  `ok: no repeated strict/product gate FAIL loop above ${options.maxRepeat} without support task or lead acceptance`
);
