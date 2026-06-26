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

function writeTaskContext(root, body) {
  mkdirSync(join(root, "tasks", "active"), { recursive: true });
  writeFileSync(join(root, "tasks", "active", "T0001-context.md"), `---
id: T0001
title: Context task
status: doing
priority: P1
tags: [prototype]
created: 2026-06-26
updated: 2026-06-26
---

${body}
`, "utf8");
}

function writeStarter(root) {
  mkdirSync(join(root, "tools", "game_context"), { recursive: true });
  mkdirSync(join(root, "gamedesign", "meme-evolution", "data"), { recursive: true });
  mkdirSync(join(root, "src"), { recursive: true });
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(join(root, "AGENTS.md"), `# AGENTS.md

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
  writeFileSync(join(root, "GAME_PROJECT.md"), `# GAME_PROJECT

## Active Game

Status: active

67 World, child-friendly meme merge/evolution game.

- Game id: \`meme-evolution\`
- Game folder: \`gamedesign/meme-evolution/\`
- Design docs: \`gamedesign/meme-evolution/gdd.md\`, \`gamedesign/meme-evolution/data/core_loop.json\`
- Current milestone: Native PC review gate.
- Hard game-specific constraints:
  - Manual child-test is required.
  - Native PC review gate.
`, "utf8");
  writeTaskContext(root, `# Taskboard Context

## Current Gate

Native PC review gate.

## Next Priorities

1. Run child-test acceptance.

## Required Validation

\`\`\`powershell
cmake --build --preset native-debug
\`\`\`
`, "utf8");
  writeFileSync(join(root, "CMakePresets.json"), "{}\n", "utf8");
  writeFileSync(join(root, "src", "main.c"), "int main(void){return 0;}\n", "utf8");
  writeFileSync(join(root, "gamedesign", "meme-evolution", "gdd.md"), "# GDD\n", "utf8");
  writeFileSync(join(root, "gamedesign", "meme-evolution", "data", "core_loop.json"), "{}\n", "utf8");
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
  assert.equal(context.concept.includes("meme-evolution"), true);
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

## Direction

- Current runtime surface: native seed in \`src/main.c\`.

## Validation

- Native desktop/PC is the preferred development harness.
`, "utf8");
  writeFileSync(join(fixture, "GAME_PROJECT.md"), `# GAME_PROJECT

## Active Game

Status: none

There is no active game concept.
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
  assert.ok(context.prototype_startup_gate.missing.includes("live_state_acceptance_matrix"));
});

test("game iteration context omits closed project sources without active concept", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  mkdirSync(join(fixture, "tools", "game_context"), { recursive: true });
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
  writeFileSync(join(fixture, "GAME_PROJECT.md"), `# GAME_PROJECT

## Active Game

Status: none

There is no active game concept.
`, "utf8");
  writeFileSync(join(fixture, "gamedesign", "knowledge", "README.md"), "# Knowledge\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "knowledge", "reference_deconstruction.md"), "# References\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "projects", "roblox-fishing", "gdd.md"), "# Closed Fishing\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "projects", "roblox-fishing", "data", "core_loop.json"), "{}\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "projects", "rune-marches", "GDD.md"), "# Closed Rune\n", "utf8");
  writeFileSync(join(fixture, "gamedesign", "projects", "rune-marches", "data", "core_loop.json"), "{}\n", "utf8");
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
  writeFileSync(join(root, "GAME_PROJECT.md"), `# GAME_PROJECT

## Active Game

Status: none

There is no active game concept.
`, "utf8");
  writeFileSync(join(root, "src", "main.c"), "int main(void){return 0;}\n", "utf8");
  writeFileSync(join(root, "CMakePresets.json"), "{}\n", "utf8");
}

function writeKickoffTemplateWithActiveNone(root) {
  writeKickoffTemplate(root);
  writeFileSync(join(root, "GAME_PROJECT.md"), `# GAME_PROJECT

## Active Game

Status: none

There is no active game concept. Focus on the reusable AI-first native game seed.
`, "utf8");
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
  assert.equal(existsSync(join(fixture, "gamedesign", "projects", "bubble-bay", "visual", "live_state_acceptance_matrix.md")), true);
  assert.equal(existsSync(join(fixture, "gamedesign", "projects", "bubble-bay", "visual", "live_state_acceptance_matrix.json")), true);
  const gameProject = readFileSync(join(fixture, "GAME_PROJECT.md"), "utf8");
  assert.match(gameProject, /Status: active/);
  assert.match(gameProject, /Game id: `bubble-bay`/);
  assert.match(gameProject, /fake-shot\/product-read\/native proof gate/);
  assert.match(gameProject, /strict visual product gates with state coverage/);
  assert.match(readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "gdd.md"), "utf8"), /visual-first session contract/);
  assert.match(readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "gdd.md"), "utf8"), /strict visual product gate using `--visual-strict`/);
  assert.match(readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "gdd.md"), "utf8"), /live_state_acceptance_matrix\.json/);
  const readme = readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "README.md"), "utf8");
  assert.match(readme, /mismatch list/);
  assert.match(readme, /strict visual product/);
  assert.match(readme, /live_state_acceptance_matrix/);
  const stateMatrix = JSON.parse(readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "visual", "live_state_acceptance_matrix.json"), "utf8"));
  assert.equal(stateMatrix.schema, "game.live_state_acceptance_matrix");
  assert.equal(stateMatrix.project, "bubble-bay");
  assert.ok(stateMatrix.required_states.includes("hud_visible"));
  assert.ok(stateMatrix.required_states.includes("primary_action_ready"));
  assert.ok(stateMatrix.required_states.includes("modal_or_choice_open"));
  assert.ok(stateMatrix.required_states.includes("locked_or_disabled_state"));
  assert.ok(stateMatrix.required_states.includes("primary_action_feedback"));
  assert.match(stateMatrix.states.transient_stress_state.proof_prompt, /Stress screenshot/);
  const visualGate = readFileSync(join(fixture, "gamedesign", "projects", "bubble-bay", "reviews", "first_slice_visual_gate.md"), "utf8");
  assert.match(visualGate, /Fake shot \/ visual target path:/);
  assert.match(visualGate, /Current native screenshot path or capture plan:/);
  assert.match(visualGate, /Screenshot-vs-target mismatch list:/);
  assert.match(visualGate, /## Visual Critic \(vision art-lead\)/);
  assert.match(visualGate, /node tools\/product_gate\/visual_critic_run\.mjs/);
  assert.match(visualGate, /critic_instruction\.md/);
  assert.match(visualGate, /game\.visual_critique/);
  assert.match(visualGate, /Gate command:/);
  assert.match(visualGate, /--visual-strict/);
  assert.match(visualGate, /--state-matrix gamedesign\/projects\/bubble-bay\/visual\/live_state_acceptance_matrix\.json/);
  assert.match(visualGate, /--require-state first_screen/);
  assert.match(visualGate, /--covered-state hud_visible/);
  assert.match(visualGate, /--not-covered-state modal_or_choice_open/);
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
  // A freshly scaffolded prototype must design the core-loop model before it
  // is ready to build (data/core_loop.json) -- design-first forcing function.
  assert.equal(context.prototype_startup_gate.status, "not_ready_for_implementation");
  assert.equal(context.prototype_startup_gate.hard_stop, true);
  assert.deepEqual(context.prototype_startup_gate.missing, ["core_loop_model"]);
  assert.equal(context.visual_first_contract.status, "required_before_visual_runtime_work");
  assert.ok(context.visual_first_contract.stop_conditions.some((item) => item.includes("Product gate fail blocks")));
  assert.ok(context.design_sources.includes("gamedesign/projects/bubble-bay/reviews/first_slice_visual_gate.md"));
  assert.ok(context.design_sources.includes("gamedesign/projects/bubble-bay/visual/live_state_acceptance_matrix.json"));

  const screenshot = join(fixture, "tmp", "first-screen.png");
  mkdirSync(join(fixture, "tmp"), { recursive: true });
  writeFileSync(screenshot, "png", "utf8");
  const gate = spawnSync(process.execPath, [
    join(repoRoot, "tools", "product_gate", "review.mjs"),
    "--project", "bubble-bay",
    "--task", "T0001",
    "--surface", "desktop",
    "--screenshot", screenshot,
    "--verdict", "pass",
    "--strict",
    "--state-matrix", "gamedesign/projects/bubble-bay/visual/live_state_acceptance_matrix.json",
    "--covered-state", "first_screen:tmp/first-screen.png",
    "--covered-state", "hud_visible:tmp/first-screen.png",
    "--covered-state", "primary_action_ready:tmp/first-screen.png",
    "--not-covered-state", "primary_action_feedback:not in startup smoke",
    "--not-covered-state", "reward_active:not in startup smoke",
    "--not-covered-state", "progression_panel_open:not in startup smoke",
    "--not-covered-state", "modal_or_choice_open:not in startup smoke",
    "--not-covered-state", "locked_or_disabled_state:not in startup smoke",
    "--not-covered-state", "resume_or_reentry_state:not in startup smoke",
    "--not-covered-state", "transient_stress_state:not in startup smoke",
    "--where", "A bright first screen.",
    "--action", "Use the primary action.",
    "--response", "The screen responds.",
    "--reward", "The next reward is clear.",
    "--game-look", "Runtime art and UI are visible.",
    "--json-output", "tmp/state-matrix-smoke.json",
  ], {
    cwd: fixture,
    encoding: "utf8",
    shell: false,
  });
  assert.equal(gate.status, 0, gate.stderr);
  const gateReport = JSON.parse(readFileSync(join(fixture, "tmp", "state-matrix-smoke.json"), "utf8"));
  assert.deepEqual(gateReport.state_coverage.required, stateMatrix.required_states);
  assert.equal(gateReport.state_coverage.covered.length, 3);
  assert.equal(gateReport.state_coverage.not_covered.length, 7);
});

test("new prototype kickoff accepts active concept none seed wording", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeKickoffTemplateWithActiveNone(fixture);

  const result = spawnSync(process.execPath, [
    join(repoRoot, "tools", "game_context", "new_prototype.mjs"),
    "--root",
    fixture,
    "--game-id",
    "dragon-grove",
    "--title",
    "Dragon Grove",
    "--brief",
    "Original merge-3 dragon grove puzzle.",
  ], {
    cwd: repoRoot,
    encoding: "utf8",
    shell: false,
  });

  assert.equal(result.status, 0, result.stderr);
  const gameProject = readFileSync(join(fixture, "GAME_PROJECT.md"), "utf8");
  assert.match(gameProject, /Status: active/);
  assert.match(gameProject, /Game id: `dragon-grove`/);
});

test("new prototype kickoff refuses real active concept without force", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeKickoffTemplateWithActiveNone(fixture);
  writeFileSync(join(fixture, "GAME_PROJECT.md"), `# GAME_PROJECT

## Active Game

Status: active

- Game id: \`existing-game\`
- Game folder: \`gamedesign/projects/existing-game/\`
`, "utf8");

  const result = spawnSync(process.execPath, [
    join(repoRoot, "tools", "game_context", "new_prototype.mjs"),
    "--root",
    fixture,
    "--game-id",
    "dragon-grove",
    "--title",
    "Dragon Grove",
    "--brief",
    "Original merge-3 dragon grove puzzle.",
  ], {
    cwd: repoRoot,
    encoding: "utf8",
    shell: false,
  });

  assert.notEqual(result.status, 0);
  assert.match(result.stderr, /GAME_PROJECT\.md already names an active game concept/);
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
