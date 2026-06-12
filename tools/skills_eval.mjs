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
      "tasks/STATUS.md",
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
      "smallest playable slice",
      "iteration-cycle-playbook",
      "CMakePresets.json",
      "Build, Launch, And Release Tasks",
      "asset-pack generation",
      "smallest affected build",
      "native desktop",
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
