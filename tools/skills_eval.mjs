import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const root = resolve(fileURLToPath(new URL("..", import.meta.url)));
const skillRoot = join(root, ".codex", "skills");

const SKILL_CHECKS = [
  {
    name: "task-manager",
    frontmatter: [
      "capturing",
      "refining",
      "decomposing",
      "planning",
      "prioritizing",
      "reporting",
      "add a task",
      "what should we do next",
      "backlog",
      "deferred",
    ],
    body: [
      "tasks/README.md",
      "tools/taskboard/cli.mjs",
      "cli.mjs summary",
      "cli.mjs context",
      "tasks/STATUS.md",
      "Read full",
      "Done when",
      "## Log",
      "evidence line",
      "idea",
      "backlog",
      "status: idea",
      "reporting rules",
    ],
  },
  {
    name: "game-runtime-automation",
    frontmatter: [
      "DevAPI",
      "command.describe",
      "ui.tree",
      "ui.click",
      "frame.wait",
      "smoke tests",
      "screenshots",
      "recordings",
      "native PC validation",
      "visual QA",
      "nonblank",
    ],
    body: [
      "endpoints",
      "command.describe",
      "native desktop/PC",
      "WASM/web",
      "screenshots",
      "recordings",
      "build/logs",
      "Do not enable automation in release builds",
      "Required command metadata",
      "method",
      "params_shape",
      "result_shape",
      "frame_behavior",
      "side_effects",
      "Visual QA",
      "nonblank output",
      "readable UI text",
      "controls respond",
    ],
  },
  {
    name: "primary-gdd-pipeline",
    frontmatter: [
      "game concept",
      "first GDD",
      "visual GDD site",
      "reference pack",
      "fake shots",
      "art bible",
      "runtime asset checklist",
      "implementation handoff",
      "core loop",
      "currencies",
      "UI",
      "game-ready art direction",
    ],
    body: [
      "Definition of Done",
      "reference",
      "fake shot",
      "runtime asset",
      "implementation plan",
      "tmp/",
      "Stop for user review",
      "player verb",
      "first-slice test",
      "living source of truth",
      "visual/runtime evidence",
      "mechanics-depth audit",
      "reference deconstruction",
      "screen grammar",
      "mismatch audit",
      "Reference Intake",
      "Definition of Ready",
      "Reference Digest",
      "Source Ladder",
      "Reference Evidence Board",
      "Parallel reference work",
      "not ready for implementation",
      "max 3",
      "3-7 refs",
      "first playable slice",
      "data/balance.json",
      "data/ui_flow.json",
      "data/asset_manifest.json",
      "data/combat.json",
      "quality-review-playbook.md",
      "skill-eval-playbook.md",
      "design-stewardship.md",
      "Report Shape",
    ],
  },
  {
    name: "game-feature-iteration",
    frontmatter: [
      "playable game feature",
      "gameplay mechanics",
      "vertical slice",
      "build",
      "release",
      "CMake presets",
      "packaging",
    ],
    body: [
      "AGENTS.md",
      "tools/game_context/iteration_context.mjs",
      "tools/ai.mjs context",
      "smallest playable slice",
      "iteration-cycle-playbook",
      "CMakePresets.json",
      "Build, Launch, And Release Tasks",
      "asset-pack generation",
      "smallest affected build",
      "native desktop",
      "reference deconstruction",
      "screen grammar",
      "first-60-seconds",
      "mismatch audit",
      "Definition of Ready",
      "Reference Intake",
      "Reference Digest",
      "Source Ladder",
      "Reference Evidence Board",
      "Parallel reference work",
      "not ready for implementation",
    ],
  },
  {
    name: "game-state-management",
    frontmatter: [
      "schema-first",
      "migrations",
      "DevAPI state commands",
      "fixtures",
      "JSON save/load",
    ],
    body: [
      "state/*.schema.json",
      "generate_state.py",
      "native-debug",
      "game_state_actions",
      "Do not hand-edit",
      "map<string,T>",
      "reserved",
      "Review Checklist",
      "state-contract.md",
    ],
  },
  {
    name: "game-asset-pipeline",
    frontmatter: [
      "textures",
      "atlases",
      "pack builders",
      "runtime asset loading failures",
    ],
    body: [
      "source of truth",
      "generated",
      "reproducible",
      "Fail loudly",
      "project-relative",
      "Do not assume the pack/material path is too slow",
      "smallest pack build",
      "measured failure",
      "slice9 margins",
      "new_art_job.mjs",
      "art job",
    ],
  },
  {
    name: "game-visual-art-direction",
    frontmatter: [
      "game visual direction",
      "art assets",
      "UI kits",
      "fake shots",
      "sprites",
      "generated visuals",
      "child-friendly visual polish",
      "release-quality presentation",
      "placeholder",
    ],
    body: [
      "accepted visual target",
      "reference deconstruction",
      "screen grammar",
      "mismatch audit",
      "Reference Intake",
      "art request packet",
      "art job",
      "candidate policy",
      "reusable kind",
      "must-not-bake",
      "slice9 insets",
      "runtime harness",
      "Produce visual assets before polishing placeholder render code",
      "durable project folders",
      "Inspect generated outputs",
      "runtime asset checklist",
      "primary runtime",
      "Art-First Gate",
      "Reusable UI Gate",
      "slice9-ready",
      "Do not bake labels",
      "Generate icons separately",
      "Generate border, tile, highlight, empty-slot",
      "shape-renderer rectangles",
      "debug buttons",
      "imagegen",
      "chroma-key",
      "raw generated source art",
      "Visual Review Checklist",
      "screenshot evidence path",
      "candidate batch",
      "new_art_job.mjs",
      "Definition of Ready",
      "Reference Intake",
      "Reference Digest",
      "Source Ladder",
      "Reference Evidence Board",
      "Parallel reference work",
    ],
  },
  {
    name: "chat-session-reflection",
    frontmatter: [
      "reflecting",
      "long AI-assisted work session",
      "bottlenecks",
      "mistakes",
      "wasted time",
      "weak tool use",
      "context loss",
      "planning gaps",
      "quality risks",
      "pipeline improvement",
    ],
    body: [
      "AGENTS.md",
      "AI_PIPELINE.md",
      "tasks/STATUS.md",
      "tools/taskboard/cli.mjs context",
      "high-cost context",
      "durable evidence",
      "symptom",
      "cause",
      "faster path",
      "tool use",
      "profiling telemetry",
      "AI_PIPELINE_OBSERVABILITY_TOOLS.md",
      "observability_gate.mjs",
      "time by phase",
      "tmp/session_profiles",
      "tools/ai.mjs",
      "node tools/ai.mjs start <id> <iteration>",
      "node tools/ai.mjs focus <next-iteration>",
      "node tools/ai.mjs context",
      "node tools/ai.mjs context --path <file>",
      "node tools/ai.mjs context -- <command>",
      "node tools/ai.mjs checkpoint \"<intent>\"",
      "node tools/ai.mjs run -- <command>",
      "node tools/ai.mjs validate --change <kind> --risk <risk>",
      "node tools/ai.mjs status",
      "node tools/ai.mjs reflect",
      "thresholded gap checkpoint",
      "run.mjs",
      "status.mjs",
      "prepare_reflection.mjs",
      "missing baseline",
      "current-scope regressions",
      "captured baseline manifests",
      "tmp/session_profiles/baselines",
      "Bundle fresh: yes",
      "historical missing work-item records",
      "current_scope",
      "current-scope missing context",
      "current-scope coverage",
      "test.mjs",
      "node --test tools/ai_profile/test.mjs",
      "closeout.mjs",
      "closeout bundle",
      "review.mjs",
      "validation_run.mjs",
      "--json-output",
      "broad_final_count",
      "followups.mjs",
      "capture_baseline.mjs",
      "compare_reviews.mjs",
      "baseline.review.json",
      "clean baseline review JSON",
      "missing, stale, regressed, or fresh",
      "stale baseline comparison",
      "reflection packet/draft/review freshness",
      "Reflection Artifacts",
      "reflection_packet.mjs",
      "first evidence map",
      "satisfied",
      "reflection_draft.mjs",
      "edit it with judgment",
      "reflection_review.mjs",
      "top next-cycle",
      "tool_use_summary",
      "context_use_summary",
      "current-scope tool/context",
      "current-scope snapshot",
      "Current Scope Readout",
      "coverage-confidence",
      "captured_elapsed",
      "current-scope validation",
      "missing_tool_metadata",
      "recovered_failure_classification",
      "repeated-command evidence",
      "repeated_command_classification",
      "repeated_unbatched_broad_final_occurrences",
      "scoped/preflight guardrail",
      "current-scope regressions",
      "whole-profile deltas",
      "missing context input",
      "work_items",
      "current_scope",
      "Current Scope Findings",
      "Current Scope Actions",
      "Historical Whole-Profile Findings",
      "current_scope.findings",
      "current_scope.suggested_actions",
      "suppressed_historical_findings",
      "historical recovered failures",
      "iteration",
      "wall_clock_coverage",
      "low_profile_coverage",
      "recovered failed records",
      "unresolved failed records",
      "largest gaps",
      "--json-output",
      "--output",
      "context management",
      "planning",
      "product quality",
      "prompt/system changes",
      "10 highest-leverage improvements",
      "AI_PIPELINE_ITERATION_LOG.md",
      "validation waste",
      "tasks/",
    ],
  },
];

function splitFrontmatter(text) {
  const match = text.match(/^---\r?\n([\s\S]*?)\r?\n---\r?\n([\s\S]*)$/);
  if (!match) {
    return { frontmatter: "", body: text };
  }
  return { frontmatter: match[1], body: match[2] };
}

function includesText(haystack, needle) {
  return haystack.toLocaleLowerCase().includes(needle.toLocaleLowerCase());
}

let failures = 0;

// Every skill in .codex/skills must have a check entry; a skill silently
// outside the eval is how regressions slip through.
const checkedNames = new Set(SKILL_CHECKS.map((c) => c.name));
for (const name of readdirSync(skillRoot)) {
  const dir = join(skillRoot, name);
  if (!statSync(dir).isDirectory() || !existsSync(join(dir, "SKILL.md"))) continue;
  if (!checkedNames.has(name)) {
    console.error(`FAIL ${name}: skill exists but has no check entry in tools/skills_eval.mjs`);
    failures += 1;
  }
}

for (const check of SKILL_CHECKS) {
  const file = join(skillRoot, check.name, "SKILL.md");
  if (!existsSync(file)) {
    console.error(`FAIL ${check.name}: missing ${file}`);
    failures += 1;
    continue;
  }

  const { frontmatter, body } = splitFrontmatter(readFileSync(file, "utf8"));
  const missingFrontmatter = check.frontmatter.filter((needle) => !includesText(frontmatter, needle));
  const missingBody = check.body.filter((needle) => !includesText(body, needle));

  if (missingFrontmatter.length === 0 && missingBody.length === 0) {
    console.log(`PASS ${check.name}`);
    continue;
  }

  failures += 1;
  console.error(`FAIL ${check.name}`);
  for (const needle of missingFrontmatter) {
    console.error(`  frontmatter missing: ${needle}`);
  }
  for (const needle of missingBody) {
    console.error(`  body missing: ${needle}`);
  }
}

if (failures > 0) {
  console.error(`\nskill eval failed: ${failures} skill(s) need attention`);
  process.exit(1);
}

console.log(`skill eval passed: ${SKILL_CHECKS.length} skill(s) checked`);
