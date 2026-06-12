import { existsSync, readFileSync } from "node:fs";
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
