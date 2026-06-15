import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";

function tempRoot(t) {
  const dir = mkdtempSync(join(tmpdir(), "game-context-test-"));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  return dir;
}

function writeStarter(root) {
  mkdirSync(join(root, "tools", "game_context"), { recursive: true });
  mkdirSync(join(root, "tools", "taskboard"), { recursive: true });
  mkdirSync(join(root, "gamedesign", "meme-evolution", "data"), { recursive: true });
  mkdirSync(join(root, "src"), { recursive: true });
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(join(root, "AGENTS.md"), `# AGENTS.md

## Project

- Active game concept: \`67 World\`, child-friendly meme merge/evolution game.

## Direction

- The current product target is a release-quality native PC/mobile game.
- The current runtime surface in \`src/main.c\` is the active native game.
- Reference study is a hard implementation gate. If a user names a reference
  game/style, do not implement gameplay from memory.
- For polished, child-testable, generated-art, UI, or release-quality visual
  work, use the visual art direction skill.

## Validation

- Product target is mobile + PC. Native desktop/PC is the preferred
  development and automation harness once implementation starts.
- Hard gate: for playable game work, do not create, serve, validate, or pivot to
  a web prototype/page/app because it seems faster or prettier. Use the native
  PC build first.
`, "utf8");
  writeFileSync(join(root, "tasks", "STATUS.md"), `# Project Status

## Current Gate

Native PC review gate.

## Next Priorities

1. Run child-test acceptance.

## Blocking Work

- Manual child-test is required.

## Required Validation

\`\`\`powershell
cmake --build --preset native-debug
\`\`\`
`, "utf8");
  writeFileSync(join(root, "tools", "taskboard", "cli.mjs"), `#!/usr/bin/env node
import { readFileSync } from "node:fs";
console.log(readFileSync("tasks/STATUS.md", "utf8"));
`, "utf8");
  writeFileSync(join(root, "CMakePresets.json"), "{}\n", "utf8");
  writeFileSync(join(root, "src", "main.c"), "int main(void){return 0;}\n", "utf8");
  writeFileSync(join(root, "gamedesign", "meme-evolution", "gdd.md"), "# GDD\n", "utf8");
  writeFileSync(join(root, "gamedesign", "meme-evolution", "data", "balance.json"), "{}\n", "utf8");
}

test("game iteration context preserves wrapped hard gates", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeStarter(fixture);
  const json = join(fixture, "tmp", "context.json");
  const result = spawnSync(process.execPath, [
    join(repoRoot, "tools", "game_context", "iteration_context.mjs"),
    "--json-output",
    json,
  ], {
    cwd: fixture,
    encoding: "utf8",
    shell: false,
  });
  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /Game Iteration Context Pack/);
  assert.match(result.stdout, /Prototype Startup Gate/);
  assert.match(result.stdout, /Visual-First Contract/);
  assert.match(result.stdout, /Native desktop\/PC is the preferred development/);
  assert.match(result.stdout, /do not create, serve, validate, or pivot to a web prototype/);
  assert.equal(existsSync(json), true);
  const context = JSON.parse(readFileSync(json, "utf8"));
  assert.equal(context.concept.includes("67 World"), true);
  assert.ok(context.hard_gates.some((gate) => gate.includes("web prototype/page/app")));
  assert.equal(context.prototype_startup_gate.status, "not_ready_for_implementation");
  assert.equal(context.prototype_startup_gate.hard_stop, true);
  assert.ok(context.prototype_startup_gate.missing.includes("visual_product_gate_plan"));
  assert.equal(context.visual_first_contract.status, "required_before_visual_runtime_work");
  assert.ok(context.visual_first_contract.session_contract_fields.some((field) => field.id === "stop_condition"));
  assert.ok(context.visual_first_contract.before_coding_required_evidence.some((item) => item.includes("mismatch list")));
  assert.ok(context.visual_first_contract.generated_ui_runtime_gate.some((item) => item.includes("Non-empty crop manifest")));
  assert.ok(context.runtime_sources.includes("src/main.c"));
  assert.ok(context.design_sources.includes("gamedesign/meme-evolution/gdd.md"));
  assert.ok(context.before_coding_checklist.some((item) => item.includes("prototype_startup_gate.status")));
  assert.ok(context.before_coding_checklist.some((item) => item.includes("5-line visual session contract")));
  assert.ok(context.before_coding_checklist.some((item) => item.includes("runtime harness")));
});

test("game iteration context blocks broad coding without active concept", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  mkdirSync(join(fixture, "tools", "game_context"), { recursive: true });
  mkdirSync(join(fixture, "tools", "taskboard"), { recursive: true });
  mkdirSync(join(fixture, "tasks"), { recursive: true });
  mkdirSync(join(fixture, "src"), { recursive: true });
  writeFileSync(join(fixture, "AGENTS.md"), `# AGENTS.md

## Project

- No active game concept is selected. Treat this repository as a clean template.

## Direction

- Current runtime surface: native seed in \`src/main.c\`.

## Validation

- Native desktop/PC is the preferred development harness.
`, "utf8");
  writeFileSync(join(fixture, "tasks", "STATUS.md"), `# Project Status

## Current Goal

Pipeline cleanup.
`, "utf8");
  writeFileSync(join(fixture, "tools", "taskboard", "cli.mjs"), `#!/usr/bin/env node
import { readFileSync } from "node:fs";
console.log(readFileSync("tasks/STATUS.md", "utf8"));
`, "utf8");
  writeFileSync(join(fixture, "src", "main.c"), "int main(void){return 0;}\n", "utf8");

  const json = join(fixture, "tmp", "context.json");
  const result = spawnSync(process.execPath, [
    join(repoRoot, "tools", "game_context", "iteration_context.mjs"),
    "--json-output",
    json,
  ], {
    cwd: fixture,
    encoding: "utf8",
    shell: false,
  });
  assert.equal(result.status, 0, result.stderr);
  const context = JSON.parse(readFileSync(json, "utf8"));
  assert.equal(context.prototype_startup_gate.status, "not_ready_for_implementation");
  assert.equal(context.prototype_startup_gate.hard_stop, true);
  assert.equal(context.visual_first_contract.status, "inactive_until_active_concept");
  assert.ok(context.prototype_startup_gate.missing.includes("active_concept"));
  assert.ok(context.prototype_startup_gate.missing.includes("active_task"));
  assert.ok(context.prototype_startup_gate.missing.includes("project_wiki"));
  assert.ok(context.prototype_startup_gate.missing.includes("visual_product_gate_plan"));
});

test("game iteration context omits closed project sources without active concept", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  mkdirSync(join(fixture, "tools", "game_context"), { recursive: true });
  mkdirSync(join(fixture, "tools", "taskboard"), { recursive: true });
  mkdirSync(join(fixture, "tasks"), { recursive: true });
  mkdirSync(join(fixture, "src"), { recursive: true });
  mkdirSync(join(fixture, "state"), { recursive: true });
  mkdirSync(join(fixture, "gamedesign", "knowledge"), { recursive: true });
  mkdirSync(join(fixture, "gamedesign", "projects", "roblox-fishing", "data"), { recursive: true });
  mkdirSync(join(fixture, "gamedesign", "projects", "rune-marches", "data"), { recursive: true });
  writeFileSync(join(fixture, "AGENTS.md"), `# AGENTS.md

## Project

- No active game concept is selected. Treat this repository as a clean template.

## Direction

- Current runtime surface: native seed in \`src/clean_seed_main.c\`.

## Validation

- Native desktop/PC is the preferred development harness.
`, "utf8");
  writeFileSync(join(fixture, "tasks", "STATUS.md"), `# Project Status

## Current Goal

Clean template.
`, "utf8");
  writeFileSync(join(fixture, "tools", "taskboard", "cli.mjs"), `#!/usr/bin/env node
import { readFileSync } from "node:fs";
console.log(readFileSync("tasks/STATUS.md", "utf8"));
`, "utf8");
  writeFileSync(join(fixture, "gamedesign", "knowledge", "README.md"), "# Knowledge\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "knowledge", "reference_deconstruction.md"), "# References\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "projects", "roblox-fishing", "gdd.md"), "# Closed Fishing\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "projects", "roblox-fishing", "data", "balance.json"), "{}\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "projects", "rune-marches", "GDD.md"), "# Closed Rune\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "projects", "rune-marches", "data", "balance.json"), "{}\n", "utf8");
  writeFileSync(join(fixture, "src", "clean_seed_main.c"), "int main(void){return 0;}\n", "utf8");
  writeFileSync(join(fixture, "src", "main.c"), "int closed(void){return 0;}\n", "utf8");
  writeFileSync(join(fixture, "state", "game_state.schema.json"), "{}\n", "utf8");
  writeFileSync(join(fixture, "CMakePresets.json"), "{}\n", "utf8");

  const json = join(fixture, "tmp", "context.json");
  const result = spawnSync(process.execPath, [
    join(repoRoot, "tools", "game_context", "iteration_context.mjs"),
    "--json-output",
    json,
  ], {
    cwd: fixture,
    encoding: "utf8",
    shell: false,
  });
  assert.equal(result.status, 0, result.stderr);
  const context = JSON.parse(readFileSync(json, "utf8"));
  assert.equal(context.visual_first_contract.status, "inactive_until_active_concept");
  assert.deepEqual(context.design_sources, [
    "gamedesign/knowledge/README.md",
    "gamedesign/knowledge/reference_deconstruction.md",
  ]);
  assert.ok(context.runtime_sources.includes("src/clean_seed_main.c"));
  assert.ok(context.runtime_sources.includes("state/game_state.schema.json"));
  assert.ok(context.runtime_sources.includes("CMakePresets.json"));
  assert.equal(context.runtime_sources.includes("src/main.c"), false);
  assert.equal(context.design_sources.some((path) => path.includes("roblox-fishing")), false);
  assert.equal(context.design_sources.some((path) => path.includes("rune-marches")), false);
  assert.doesNotMatch(result.stdout, /roblox-fishing|rune-marches|src\/main\.c/);
});

function writeKickoffTemplate(root) {
  mkdirSync(join(root, "tasks", "active"), { recursive: true });
  mkdirSync(join(root, "tasks", "archive"), { recursive: true });
  mkdirSync(join(root, "tasks", "epics"), { recursive: true });
  mkdirSync(join(root, "gamedesign", "projects"), { recursive: true });
  mkdirSync(join(root, "src"), { recursive: true });
  writeFileSync(join(root, "AGENTS.md"), `# AGENTS.md

## Project

- No active game concept is selected. Treat this repository as a clean template.

## Direction

- Current runtime surface: native seed in \`src/main.c\`.

## Validation

- Native desktop/PC is the preferred development harness.
`, "utf8");
  writeFileSync(join(root, "tasks", "STATUS.md"), `# Project Status

## Current Goal

Clean template.
`, "utf8");
  writeFileSync(join(root, "src", "main.c"), "int main(void){return 0;}\n", "utf8");
  writeFileSync(join(root, "CMakePresets.json"), "{}\n", "utf8");
}

test("new prototype kickoff creates startup-ready skeleton", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeKickoffTemplate(fixture);

  const result = spawnSync(process.execPath, [
    join(repoRoot, "tools", "game_context", "new_prototype.mjs"),
    "--root",
    fixture,
    "--game-id",
    "bubble-bay",
    "--title",
    "Bubble Bay",
    "--brief",
    "Bright casual bubble fishing prototype with simple upgrades.",
  ], {
    cwd: repoRoot,
    encoding: "utf8",
    shell: false,
  });

  assert.equal(result.status, 0, result.stderr);
  assert.match(result.stdout, /created project: gamedesign[\\/]projects[\\/]bubble-bay/);
  assert.equal(existsSync(join(fixture, "gamedesign", "projects", "bubble-bay", "README.md")), true);
  assert.equal(existsSync(join(fixture, "gamedesign", "projects", "bubble-bay", "gdd.md")), true);
  assert.equal(existsSync(join(fixture, "gamedesign", "projects", "bubble-bay", "reviews", "first_slice_visual_gate.md")), true);
  assert.match(readFileSync(join(fixture, "AGENTS.md"), "utf8"), /Active game concept: `Bubble Bay`/);
  assert.match(readFileSync(join(fixture, "tasks", "STATUS.md"), "utf8"), /fake shot\/product-read\/native/);
  assert.match(readFileSync(join(fixture, "tasks", "STATUS.md"), "utf8"), /first_slice_visual_gate\.md/);
  assert.match(readFileSync(join(fixture, "tasks", "STATUS.md"), "utf8"), /screenshot-vs-target mismatch list/);
  assert.match(readFileSync(join(fixture, "tasks", "STATUS.md"), "utf8"), /`--visual-strict`/);
  assert.match(readFileSync(join(fixture, "tasks", "STATUS.md"), "utf8"), /Strict visual product gates require six scores/);
  assert.match(readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "gdd.md"), "utf8"), /visual-first session contract/);
  assert.match(readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "gdd.md"), "utf8"), /strict visual product gate using `--visual-strict`/);
  const readme = readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "README.md"), "utf8");
  assert.match(readme, /mismatch list/);
  assert.match(readme, /strict visual product/);
  const visualGate = readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "reviews", "first_slice_visual_gate.md"), "utf8");
  assert.match(visualGate, /Fake shot \/ visual target path:/);
  assert.match(visualGate, /Current native screenshot path or capture plan:/);
  assert.match(visualGate, /Screenshot-vs-target mismatch list:/);
  assert.match(visualGate, /## Visual Critic Packet/);
  assert.match(visualGate, /node tools\/ai\.mjs critic/);
  assert.match(visualGate, /first_slice_visual_critic_packet\.md/);
  assert.match(visualGate, /first_slice_visual_critic_packet\.json/);
  assert.match(visualGate, /Gate command:/);
  assert.match(visualGate, /--visual-strict/);
  assert.match(visualGate, /--visual-score composition=1/);
  assert.match(visualGate, /--visual-score readability=1/);
  assert.match(visualGate, /--visual-score ui_controls=1/);
  assert.match(visualGate, /--visual-score action_direction=1/);
  assert.match(visualGate, /--visual-score art_quality=1/);
  assert.match(visualGate, /--visual-score audience_fit=1/);
  assert.match(visualGate, /visual issues use severity `blocker`, `major`, or `minor`/);
  assert.match(visualGate, /pass requires all six scores >= 4/);
  assert.match(visualGate, /Stop condition:/);
  assert.match(visualGate, /Decision: blocked until filled/);

  const context = JSON.parse(readFileSync(join(fixture, "tmp", "prototype_startup_gate_context.json"), "utf8"));
  assert.equal(context.prototype_startup_gate.status, "ready_for_first_slice");
  assert.equal(context.prototype_startup_gate.hard_stop, false);
  assert.deepEqual(context.prototype_startup_gate.missing, []);
  assert.equal(context.visual_first_contract.status, "required_before_visual_runtime_work");
  assert.ok(context.visual_first_contract.stop_conditions.some((item) => item.includes("Product gate fail blocks")));
  assert.ok(context.design_sources.includes("gamedesign/projects/bubble-bay/reviews/first_slice_visual_gate.md"));
});

test("new prototype kickoff refuses existing project without force", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeKickoffTemplate(fixture);
  mkdirSync(join(fixture, "gamedesign", "projects", "bubble-bay"), { recursive: true });

  const result = spawnSync(process.execPath, [
    join(repoRoot, "tools", "game_context", "new_prototype.mjs"),
    "--root",
    fixture,
    "--game-id",
    "bubble-bay",
    "--title",
    "Bubble Bay",
    "--brief",
    "Bright casual bubble fishing prototype with simple upgrades.",
  ], {
    cwd: repoRoot,
    encoding: "utf8",
    shell: false,
  });

  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /already exists/);
});
