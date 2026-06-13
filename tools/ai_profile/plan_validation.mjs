#!/usr/bin/env node
import { mkdirSync, writeFileSync } from "node:fs";
import { basename, dirname, resolve } from "node:path";
import { listArg, parseArgs, stringArg } from "./profile_lib.mjs";

const RISK_LEVELS = ["low", "medium", "high"];

const CHANGE_ALIASES = new Map([
  ["doc", "docs"],
  ["documentation", "docs"],
  ["skill", "skills"],
  ["profile", "profiling"],
  ["profiler", "profiling"],
  ["validation", "pipeline"],
  ["portable", "pipeline"],
  ["tasks", "taskboard"],
  ["task", "taskboard"],
  ["game", "runtime"],
  ["native", "runtime"],
  ["pc", "runtime"],
  ["pack", "release"],
  ["package", "release"],
  ["asset", "assets"],
  ["art", "assets"],
  ["schema", "state"],
  ["save", "state"],
  ["test", "tests"],
]);

const CHANGE_DESCRIPTIONS = new Map([
  ["docs", "documentation or process text"],
  ["skills", "Codex/Claude skill rules"],
  ["taskboard", "tasks, STATUS, or taskboard tooling"],
  ["pipeline", "portable AI workflow, exporter, or pipeline validation"],
  ["profiling", "AI profile collection/review tools"],
  ["runtime", "game/runtime source or native harness"],
  ["assets", "art, textures, manifests, or asset pack flow"],
  ["state", "state schemas, migrations, save/load, or generated state"],
  ["release", "packaging, release audit, or handoff artifacts"],
  ["tests", "test harnesses or validation scripts"],
  ["web", "web/mobile surface explicitly requested by the task"],
]);

const CHECKS = [
  {
    id: "diff-whitespace",
    tier: "preflight",
    changes: ["all"],
    command: "git diff --check",
    why: "catch whitespace and patch artifacts before deeper validation",
    broad: false,
  },
  {
    id: "js-syntax-touched",
    tier: "preflight",
    changes: ["profiling", "pipeline", "taskboard", "tests"],
    command: "node tools/ai_profile/check_touched_js.mjs",
    why: "cheap syntax check for touched JavaScript tools",
    broad: false,
  },
  {
    id: "gap-checkpoint-syntax",
    tier: "preflight",
    changes: ["profiling"],
    command: "node --check tools/ai_profile/gap_checkpoint.mjs",
    why: "cheap syntax check for the gap checkpoint helper",
    broad: false,
  },
  {
    id: "profile-planner-self",
    tier: "scoped",
    changes: ["profiling", "pipeline"],
    command: "node tools/ai_profile/plan_validation.mjs --change profiling --risk medium",
    why: "prove the planner can explain its own validation ladder",
    broad: false,
  },
  {
    id: "profile-review-example",
    tier: "scoped",
    changes: ["profiling"],
    command:
      "node tools/ai_profile/review.mjs tools/ai_profile/session_profile_example.jsonl --output tmp/session_profiles/session_profile_example.review.md",
    why: "prove profile analysis still works on a stable example",
    broad: false,
  },
  {
    id: "ai-profile-tests",
    tier: "scoped",
    changes: ["profiling"],
    command: "node --test tools/ai_profile/test.mjs",
    why: "prove AI profile tooling behavior after profiler changes",
    broad: false,
  },
  {
    id: "taskboard-validate",
    tier: "scoped",
    changes: ["taskboard", "docs", "pipeline", "profiling", "skills"],
    command: "node tools/taskboard/cli.mjs validate",
    why: "prove active tasks and status metadata still parse",
    broad: false,
  },
  {
    id: "taskboard-tests",
    tier: "scoped",
    changes: ["taskboard", "tests"],
    command: "node --test tools/taskboard/test.mjs",
    why: "prove taskboard behavior after task tooling changes",
    broad: false,
  },
  {
    id: "skills-sync",
    tier: "scoped",
    changes: ["skills"],
    command: "node tools/skills_sync.mjs",
    why: "mirror canonical .codex skills into generated skill surfaces",
    broad: false,
  },
  {
    id: "skills-eval",
    tier: "scoped",
    changes: ["skills"],
    command: "node tools/skills_eval.mjs",
    why: "prove reusable skill activation and required process anchors remain intact",
    broad: false,
  },
  {
    id: "state-codegen",
    tier: "scoped",
    changes: ["state"],
    command: "py -3.12 tools/state_codegen/generate_state.py",
    why: "regenerate and verify schema-derived state code",
    broad: false,
  },
  {
    id: "native-build",
    tier: "scoped",
    changes: ["runtime", "state", "assets"],
    command: "cmake --build --preset native-debug",
    why: "prove the primary PC harness still builds",
    broad: false,
  },
  {
    id: "native-scenario",
    tier: "scoped",
    changes: ["runtime", "assets", "state"],
    command: "<smallest native scenario or DevAPI smoke that proves the changed behavior>",
    why: "prove playable/visual behavior in the primary runtime",
    broad: false,
    placeholder: true,
  },
  {
    id: "asset-pack",
    tier: "scoped",
    changes: ["assets"],
    command: "<smallest explicit asset pack/material build for touched assets>",
    why: "prove assets are reproducible without wiring packs into every build",
    broad: false,
    placeholder: true,
  },
  {
    id: "release-smoke",
    tier: "scoped",
    changes: ["release"],
    command: "<package/release smoke command named by the project>",
    why: "prove the package or handoff path that changed",
    broad: false,
    placeholder: true,
  },
  {
    id: "web-check",
    tier: "scoped",
    changes: ["web"],
    command: "<web/mobile build or browser check explicitly requested by the task>",
    why: "prove web/mobile behavior only when that platform is in scope",
    broad: false,
    placeholder: true,
  },
  {
    id: "portable-pipeline",
    tier: "final",
    changes: ["pipeline", "skills", "taskboard"],
    command: "node tools/pipeline_validate.mjs",
    why: "prove the reusable base still exports and validates in a fresh project",
    broad: true,
  },
  {
    id: "release-audit",
    tier: "final",
    changes: ["release"],
    command: "<release candidate audit command named by the project>",
    why: "prove the release gate after scoped smoke passes",
    broad: true,
    placeholder: true,
  },
];

function usage() {
  console.error(`usage:
  node tools/ai_profile/plan_validation.mjs --change <kind> [--change <kind> ...] [--file <path> ...] [--risk low|medium|high] [--include-final] [--json] [--json-output <file>]
  node tools/ai_profile/plan_validation.mjs --list-changes

Prints a validation ladder. It does not run commands.
Broad/final checks are opt-in unless --include-final is present.`);
  process.exit(2);
}

function normalizeChange(raw) {
  const key = String(raw || "").trim().toLowerCase();
  if (!key) return "";
  return CHANGE_ALIASES.get(key) || key;
}

function inferChangesFromFile(file) {
  const normalized = file.replaceAll("\\", "/");
  const changes = new Set();
  if (normalized.startsWith(".codex/skills/") || normalized.startsWith(".claude/skills/")) changes.add("skills");
  if (normalized.startsWith("tasks/") || normalized.startsWith("tools/taskboard/")) changes.add("taskboard");
  if (/^(AI_PIPELINE|AGENTS|CLAUDE)\.md$/.test(basename(normalized)) || normalized.startsWith("tools/bootstrap/")) {
    changes.add("pipeline");
  }
  if (normalized.startsWith("tools/ai_profile/") || basename(normalized) === "AI_PIPELINE_SESSION_PROFILING.md") {
    changes.add("profiling");
  }
  if (normalized.startsWith("src/") || normalized.startsWith("external/")) changes.add("runtime");
  if (normalized.startsWith("state/") || normalized.startsWith("tools/state_codegen/")) changes.add("state");
  if (normalized.startsWith("assets/") || normalized.includes("/asset") || normalized.startsWith("tools/assets/")) changes.add("assets");
  if (normalized.startsWith("tools/release") || normalized.startsWith("tools/package") || normalized.startsWith("build/release")) {
    changes.add("release");
  }
  if (normalized.startsWith("web/") || normalized.startsWith("wasm/")) changes.add("web");
  if (/\.(test|spec)\.(mjs|js|ts)$/.test(normalized)) changes.add("tests");
  if (changes.size === 0 && /\.(md|txt)$/.test(normalized)) changes.add("docs");
  return [...changes];
}

function riskIndex(risk) {
  const index = RISK_LEVELS.indexOf(risk);
  return index === -1 ? RISK_LEVELS.indexOf("medium") : index;
}

function shouldIncludeFinal(check, selectedChanges, risk, includeFinal) {
  if (!check.broad) return true;
  if (!includeFinal) return false;
  const highEnoughRisk = riskIndex(risk) >= riskIndex("medium");
  const explicitlyRelevant = check.changes.some((change) => selectedChanges.has(change));
  return highEnoughRisk && explicitlyRelevant;
}

function selectChecks(selectedChanges, risk, includeFinal) {
  const selected = [];
  const seen = new Set();
  for (const check of CHECKS) {
    const applies = check.changes.includes("all") || check.changes.some((change) => selectedChanges.has(change));
    if (!applies) continue;
    if (!shouldIncludeFinal(check, selectedChanges, risk, includeFinal)) continue;
    if (seen.has(check.id)) continue;
    seen.add(check.id);
    selected.push(check);
  }
  return selected;
}

function renderMarkdown(changes, risk, checks, skippedFinal) {
  const lines = [];
  lines.push(`# Validation Ladder`);
  lines.push("");
  lines.push(`Risk: ${risk}`);
  lines.push(`Change kinds: ${[...changes].sort().join(", ")}`);
  lines.push("");
  lines.push("Rule: run preflight/scoped checks first. Add `--include-final` only for release, portable-base, or shared-behavior gates.");
  for (const tier of ["preflight", "scoped", "final"]) {
    const tierChecks = checks.filter((check) => check.tier === tier);
    if (tierChecks.length === 0) continue;
    lines.push("");
    lines.push(`## ${tier[0].toUpperCase()}${tier.slice(1)} Checks`);
    for (const check of tierChecks) {
      const suffix = check.broad ? " broad/final" : check.placeholder ? " fill project command" : "";
      lines.push(`- \`${check.command}\`${suffix}`);
      lines.push(`  - why: ${check.why}`);
    }
  }
  if (skippedFinal.length > 0) {
    lines.push("");
    lines.push("## Deferred Broad Checks");
    for (const check of skippedFinal) {
      lines.push(`- \`${check.command}\` - ${check.why}`);
    }
    lines.push("");
    lines.push("These are deferred by default. Run them only when the change is portable, release-critical, or touches shared behavior.");
  }
  lines.push("");
  lines.push("For normal work, use `node tools/ai.mjs run -- <command>` only when passive telemetry on slow/failing commands is useful.");
  return `${lines.join("\n")}\n`;
}

function buildPlan(changes, risk, checks, skippedFinal) {
  const sortedChanges = [...changes].sort();
  const byTier = Object.fromEntries(["preflight", "scoped", "final"].map((tier) => [
    tier,
    checks.filter((check) => check.tier === tier),
  ]));
  const finalChecks = byTier.final || [];
  const broadFinalChecks = finalChecks.filter((check) => check.broad);
  return {
    schema_version: 1,
    risk,
    changes: sortedChanges,
    checks,
    checks_by_tier: byTier,
    final_checks: finalChecks,
    broad_final_checks: broadFinalChecks,
    skipped_final: skippedFinal,
    broad_final_count: broadFinalChecks.length,
    deferred_broad_count: skippedFinal.length,
    next_action: broadFinalChecks.length > 0
      ? "Run preflight and scoped checks first; run broad/final checks once at the end of the batch."
      : "Run preflight and scoped checks; broad/final checks are deferred unless the task explicitly needs a release, portable-base, or shared-behavior gate.",
    rule:
      "Broad/final checks should repeat only after a failed gate, changed risk, changed shared behavior, or final batch boundary.",
  };
}

const { values } = parseArgs(process.argv.slice(2));
if (values.help) usage();
if (values["list-changes"]) {
  for (const [name, description] of [...CHANGE_DESCRIPTIONS.entries()].sort()) {
    console.log(`${name}: ${description}`);
  }
  process.exit(0);
}

const risk = stringArg(values, "risk", "medium").toLowerCase();
if (!RISK_LEVELS.includes(risk)) usage();

const changes = new Set();
for (const raw of listArg(values, "change")) {
  for (const part of String(raw).split(",")) {
    const normalized = normalizeChange(part);
    if (normalized) changes.add(normalized);
  }
}
for (const file of listArg(values, "file")) {
  for (const change of inferChangesFromFile(file)) changes.add(change);
}
if (changes.size === 0) changes.add("docs");

const unknown = [...changes].filter((change) => !CHANGE_DESCRIPTIONS.has(change));
if (unknown.length > 0) {
  console.error(`unknown change kind(s): ${unknown.join(", ")}`);
  console.error(`run --list-changes for supported values`);
  process.exit(2);
}

const includeFinal = values["include-final"] === true;
const checks = selectChecks(changes, risk, includeFinal);
const skippedFinal = CHECKS.filter((check) => {
  if (!check.broad) return false;
  const applies = check.changes.some((change) => changes.has(change));
  return applies && !checks.includes(check);
});
const plan = buildPlan(changes, risk, checks, skippedFinal);
const jsonOutput = stringArg(values, "json-output", "");

if (jsonOutput) {
  const target = resolve(jsonOutput);
  mkdirSync(dirname(target), { recursive: true });
  writeFileSync(target, `${JSON.stringify(plan, null, 2)}\n`, "utf8");
}

if (values.json) {
  process.stdout.write(`${JSON.stringify(plan, null, 2)}\n`);
} else {
  process.stdout.write(renderMarkdown(changes, risk, checks, skippedFinal));
}
