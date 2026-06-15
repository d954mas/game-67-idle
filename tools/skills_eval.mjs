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
      "product_gate/review.mjs",
      "close-slice",
      "5-line session contract",
      "mismatch list",
      "Product gate fail blocks",
      "slice_hygiene.mjs",
      "30 changed files",
      "promise push",
      "stale fail audits",
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
      "new_generation_record.mjs",
      "non-empty workflow JSON",
      "no-seed reason",
      "chroma_key_alpha.py",
      "--final-art",
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
      "empty `{}` placeholders",
      "no-seed reason",
      "normalize_source_sheet_chroma.py",
      "audit_source_sheet_intake.py",
      "--final-art",
      "Definition of Ready",
      "Reference Intake",
      "Reference Digest",
      "Source Ladder",
      "Reference Evidence Board",
      "Parallel reference work",
      "product_gate/review.mjs",
      "close-slice",
      "5-line visual session contract",
      "screenshot-vs-target mismatch",
      "named mismatch",
    ],
  },
  {
    name: "generated-game-ui-assets",
    frontmatter: [
      "generating",
      "reusable game UI asset kits",
      "AI art",
      "UI source sheets",
      "slice9 panels",
      "art bibles",
      "crop manifests",
      "runtime manifests",
      "pixel audits",
      "responsive UI layout audits",
      "cropped/fringed generated UI assets",
    ],
    body: [
      "game-visual-art-direction",
      "game-asset-pipeline",
      "game-runtime-automation",
      "art bible",
      "new_art_job.mjs",
      "new_generation_record.mjs",
      "non-empty",
      "no-seed-reason",
      "chroma_key_alpha.py",
      "normalize_source_sheet_chroma.py",
      "blank UI kit sheet",
      "isolated icon sheet",
      "full mockups only as visual targets",
      "baked text",
      "tight gutters",
      "slice9",
      "content",
      "target_preview_sizes",
      "trim padding",
      "component isolation",
      "border-connected key color",
      "must not redraw panels",
      "drawing primitives",
      "contact sheet",
      "validate_art_job.mjs",
      "new_generation_record.mjs",
      "--final-art",
      "audit_source_sheet_intake.py",
      "audit_generated_ui_assets.py",
      "audit_generated_source_derivation.py",
      "source-derived PNGs",
      "product_gate/review.mjs",
      "responsive_layout_audit.mjs",
      "Desktop and portrait are separate compositions",
      "one full-width primary action",
      "Report Shape",
      "5-line session contract",
      "empty crop manifest",
      "pixel audit",
      "runtime integration",
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
      "tools/taskboard/cli.mjs summary",
      "tools/taskboard/cli.mjs context",
      "high-cost context",
      "durable evidence",
      "symptom",
      "cause",
      "faster path",
      "tool use",
      "passive profiling telemetry",
      "AI_PIPELINE_OBSERVABILITY_TOOLS.md",
      "observability_gate.mjs",
      "tmp/session_profiles",
      "tools/ai.mjs",
      "node tools/ai.mjs status",
      "node tools/ai.mjs reflect --deep",
      "node tools/ai.mjs status --verbose",
      "unresolved failures",
      "slowest recorded work",
      "largest context input",
      "long manual/research/review gaps",
      "stale bundles",
      "deep AI-workflow retrospective",
      "context management",
      "planning",
      "product quality",
      "highest-leverage process changes",
      "AI_PIPELINE_ITERATION_LOG.md",
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
