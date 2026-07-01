import test from "node:test";
import assert from "node:assert/strict";
import { existsSync, mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { tmpdir } from "node:os";
import { spawnSync } from "node:child_process";

const removedRootGameFile = `GAME_${"PROJECT.md"}`;

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
  mkdirSync(join(root, "ai_studio", "game_project"), { recursive: true });
  mkdirSync(join(root, "games", "meme-evolution", "design", "data"), { recursive: true });
  mkdirSync(join(root, "games", "meme-evolution", "src"), { recursive: true });
  mkdirSync(join(root, "games", "meme-evolution", "state"), { recursive: true });
  mkdirSync(join(root, "tasks"), { recursive: true });
  writeFileSync(join(root, "AGENTS.md"), `# AGENTS.md

## Direction

- The current product target is a release-quality native PC/mobile game.
- The current runtime surface in \`games/meme-evolution/src/main.c\` is the active native game.
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
  writeFileSync(join(root, "games", "meme-evolution", "CMakeLists.txt"), "# test cmake\n", "utf8");
  writeFileSync(join(root, "games", "meme-evolution", "src", "main.c"), "int main(void){return 0;}\n", "utf8");
  writeFileSync(join(root, "games", "meme-evolution", "state", "game_state.schema.json"), "{}\n", "utf8");
  writeFileSync(join(root, "games", "meme-evolution", "design", "gdd.md"), "# GDD\n", "utf8");
  writeFileSync(join(root, "games", "meme-evolution", "design", "data", "core_loop.json"), "{}\n", "utf8");
}

test("game iteration context preserves wrapped hard gates", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeStarter(fixture);
  const json = join(fixture, "tmp", "context.json");
  const result = spawnSync(process.execPath, [
    join(repoRoot, "ai_studio", "game_project", "iteration_context.mjs"),
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
  assert.ok(context.prototype_startup_gate.missing.includes("visual_review_plan"));
  assert.equal(context.visual_first_contract.status, "required_before_visual_runtime_work");
  assert.ok(context.visual_first_contract.session_contract_fields.some((field) => field.id === "stop_condition"));
  assert.ok(context.visual_first_contract.before_coding_required_evidence.some((item) => item.includes("mismatch list")));
  assert.ok(context.visual_first_contract.generated_ui_runtime_gate.some((item) => item.includes("Non-empty crop plan")));
  assert.ok(context.visual_first_contract.generated_ui_runtime_gate.some((item) => item.includes("Non-empty prepared asset manifest")));
  assert.ok(context.runtime_sources.includes("games/meme-evolution/src/main.c"));
  assert.ok(context.runtime_sources.includes("games/meme-evolution/state/game_state.schema.json"));
  assert.ok(context.runtime_sources.includes("games/meme-evolution/CMakeLists.txt"));
  assert.ok(context.design_sources.includes("games/meme-evolution/design/gdd.md"));
  assert.ok(context.before_coding_checklist.some((item) => item.includes("prototype_startup_gate.status")));
  assert.ok(context.before_coding_checklist.some((item) => item.includes("5-line visual session contract")));
  assert.ok(context.before_coding_checklist.some((item) => item.includes("runtime harness")));
});

test("game iteration context blocks broad coding without active concept", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  mkdirSync(join(fixture, "ai_studio", "game_project"), { recursive: true });
  mkdirSync(join(fixture, "tasks"), { recursive: true });
  mkdirSync(join(fixture, "src"), { recursive: true });
  writeFileSync(join(fixture, "AGENTS.md"), `# AGENTS.md

## Direction

- Current runtime surface: native seed in \`src/main.c\`.

## Validation

- Native desktop/PC is the preferred development harness.
`, "utf8");
  writeFileSync(join(fixture, "src", "main.c"), "int main(void){return 0;}\n", "utf8");

  const json = join(fixture, "tmp", "context.json");
  const result = spawnSync(process.execPath, [
    join(repoRoot, "ai_studio", "game_project", "iteration_context.mjs"),
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
  assert.ok(context.prototype_startup_gate.missing.includes("visual_review_plan"));
  assert.ok(context.prototype_startup_gate.missing.includes("live_state_acceptance_matrix"));
});

test("game iteration context omits closed project sources without active concept", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  mkdirSync(join(fixture, "tasks"), { recursive: true });
  mkdirSync(join(fixture, "src"), { recursive: true });
  mkdirSync(join(fixture, "state"), { recursive: true });
  mkdirSync(join(fixture, "gamedev_knowledge", "knowledge"), { recursive: true });
  mkdirSync(join(fixture, "games", "closed-alpha", "design", "data"), { recursive: true });
  mkdirSync(join(fixture, "games", "closed-beta", "design", "data"), { recursive: true });
  writeFileSync(join(fixture, "AGENTS.md"), `# AGENTS.md

## Project

- No active game concept is selected. Treat this repository as a clean template.

## Direction

- Current runtime surface: native seed in \`src/clean_seed_main.c\`.

## Validation

- Native desktop/PC is the preferred development harness.
`, "utf8");
  writeFileSync(join(fixture, "gamedev_knowledge", "knowledge", "README.md"), "# Knowledge\n", "utf8");
  writeFileSync(join(fixture, "gamedev_knowledge", "knowledge", "reference_deconstruction.md"), "# References\n", "utf8");
  writeFileSync(join(fixture, "games", "closed-alpha", "design", "gdd.md"), "# Closed Alpha\n", "utf8");
  writeFileSync(join(fixture, "games", "closed-alpha", "design", "data", "core_loop.json"), "{}\n", "utf8");
  writeFileSync(join(fixture, "games", "closed-beta", "design", "GDD.md"), "# Closed Beta\n", "utf8");
  writeFileSync(join(fixture, "games", "closed-beta", "design", "data", "core_loop.json"), "{}\n", "utf8");
  writeFileSync(join(fixture, "src", "clean_seed_main.c"), "int main(void){return 0;}\n", "utf8");
  writeFileSync(join(fixture, "src", "main.c"), "int closed(void){return 0;}\n", "utf8");
  writeFileSync(join(fixture, "state", "game_state.schema.json"), "{}\n", "utf8");
  writeFileSync(join(fixture, "CMakePresets.json"), "{}\n", "utf8");

  const json = join(fixture, "tmp", "context.json");
  const result = spawnSync(process.execPath, [
    join(repoRoot, "ai_studio", "game_project", "iteration_context.mjs"),
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
    "gamedev_knowledge/knowledge/README.md",
    "gamedev_knowledge/knowledge/reference_deconstruction.md",
  ]);
  assert.ok(context.runtime_sources.includes("src/clean_seed_main.c"));
  assert.ok(context.runtime_sources.includes("state/game_state.schema.json"));
  assert.ok(context.runtime_sources.includes("CMakePresets.json"));
  assert.equal(context.runtime_sources.includes("src/main.c"), false);
  assert.equal(context.design_sources.some((path) => path.includes("closed-alpha")), false);
  assert.equal(context.design_sources.some((path) => path.includes("closed-beta")), false);
  assert.doesNotMatch(result.stdout, /closed-alpha|closed-beta|src\/main\.c/);
});

function writeKickoffTemplate(root) {
  mkdirSync(join(root, "tasks", "active"), { recursive: true });
  mkdirSync(join(root, "tasks", "archive"), { recursive: true });
  mkdirSync(join(root, "tasks", "epics"), { recursive: true });
  mkdirSync(join(root, "games"), { recursive: true });
  mkdirSync(join(root, "src"), { recursive: true });
  writeFileSync(join(root, "AGENTS.md"), `# AGENTS.md

## Project

- No active game concept is selected. Treat this repository as a clean template.

## Direction

- Current runtime surface: native seed in \`src/main.c\`.

## Validation

- Native desktop/PC is the preferred development harness.
`, "utf8");
  writeFileSync(join(root, "src", "main.c"), "int main(void){return 0;}\n", "utf8");
  writeFileSync(join(root, "CMakePresets.json"), "{}\n", "utf8");
}

test("new prototype kickoff creates startup-ready skeleton", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeKickoffTemplate(fixture);

  const result = spawnSync(process.execPath, [
    join(repoRoot, "ai_studio", "game_project", "new_prototype.mjs"),
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
  assert.match(result.stdout, /created project: games[\\/]bubble-bay[\\/]design/);
  assert.equal(existsSync(join(fixture, "games", "bubble-bay", "design", "README.md")), true);
  assert.equal(existsSync(join(fixture, "games", "bubble-bay", "design", "gdd.md")), true);
  assert.equal(existsSync(join(fixture, "games", "bubble-bay", "design", "reviews", "first_slice_review.md")), true);
  assert.equal(existsSync(join(fixture, "games", "bubble-bay", "design", "visual", "live_state_acceptance_matrix.md")), true);
  assert.equal(existsSync(join(fixture, "games", "bubble-bay", "design", "visual", "live_state_acceptance_matrix.json")), true);
  assert.equal(existsSync(join(fixture, removedRootGameFile)), false);
  assert.match(readFileSync(join(fixture, "games", "bubble-bay", "design", "gdd.md"), "utf8"), /visual-first session contract/);
  assert.match(readFileSync(join(fixture, "games", "bubble-bay", "design", "gdd.md"), "utf8"), /review evidence/);
  assert.match(readFileSync(join(fixture, "games", "bubble-bay", "design", "gdd.md"), "utf8"), /live_state_acceptance_matrix\.json/);
  const readme = readFileSync(join(fixture, "games", "bubble-bay", "design", "README.md"), "utf8");
  assert.match(readme, /mismatch list/);
  assert.doesNotMatch(readme, /ai_studio\/quality|Quality Rules|QCLR|QART/);
  assert.match(readme, /live_state_acceptance_matrix/);
  const stateMatrix = JSON.parse(readFileSync(join(fixture, "games", "bubble-bay", "design", "visual", "live_state_acceptance_matrix.json"), "utf8"));
  assert.equal(stateMatrix.schema, "game.live_state_acceptance_matrix");
  assert.equal(stateMatrix.project, "bubble-bay");
  assert.ok(stateMatrix.required_states.includes("hud_visible"));
  assert.ok(stateMatrix.required_states.includes("primary_action_ready"));
  assert.ok(stateMatrix.required_states.includes("modal_or_choice_open"));
  assert.ok(stateMatrix.required_states.includes("locked_or_disabled_state"));
  assert.ok(stateMatrix.required_states.includes("primary_action_feedback"));
  assert.match(stateMatrix.states.transient_stress_state.proof_prompt, /Stress screenshot/);
  const visualGate = readFileSync(join(fixture, "games", "bubble-bay", "design", "reviews", "first_slice_review.md"), "utf8");
  assert.match(visualGate, /Fake shot \/ visual target path:/);
  assert.match(visualGate, /Current native screenshot path or capture plan:/);
  assert.match(visualGate, /Screenshot-vs-target mismatch list:/);
  assert.match(visualGate, /## Review Evidence/);
  assert.doesNotMatch(visualGate, /Quality Rules|Applied rules|ai_studio\/quality|QCLR_001|QCLR_002|QART_001/);
  assert.match(visualGate, /Stop condition:/);
  assert.match(visualGate, /Decision: blocked until filled/);

  const context = JSON.parse(readFileSync(join(fixture, "tmp", "prototype_startup_gate_context.json"), "utf8"));
  // A freshly scaffolded prototype must create/copy a game runtime and design
  // the core-loop model before it is ready to build.
  assert.equal(context.prototype_startup_gate.status, "not_ready_for_implementation");
  assert.equal(context.prototype_startup_gate.hard_stop, true);
  assert.deepEqual(context.prototype_startup_gate.missing, ["runtime_harness", "visual_review_plan", "core_loop_model"]);
  assert.equal(context.visual_first_contract.status, "required_before_visual_runtime_work");
  assert.ok(context.visual_first_contract.stop_conditions.some((item) => item.includes("Failed review blocks")));
  assert.ok(context.design_sources.includes("games/bubble-bay/design/reviews/first_slice_review.md"));
  assert.ok(context.design_sources.includes("games/bubble-bay/design/visual/live_state_acceptance_matrix.json"));
});

test("new prototype kickoff keeps game routing inside games folder", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeKickoffTemplate(fixture);

  const result = spawnSync(process.execPath, [
    join(repoRoot, "ai_studio", "game_project", "new_prototype.mjs"),
    "--root",
    fixture,
    "--game-id",
    "orb-garden",
    "--title",
    "Orb Garden",
    "--brief",
    "Original merge-3 dragon grove puzzle.",
  ], {
    cwd: repoRoot,
    encoding: "utf8",
    shell: false,
  });

  assert.equal(result.status, 0, result.stderr);
  assert.equal(existsSync(join(fixture, removedRootGameFile)), false);
  assert.equal(existsSync(join(fixture, "games", "orb-garden", "design", "gdd.md")), true);
});

test("new prototype kickoff refuses existing project without force", (t) => {
  const fixture = tempRoot(t);
  const repoRoot = process.cwd();
  writeKickoffTemplate(fixture);
  mkdirSync(join(fixture, "games", "bubble-bay", "design"), { recursive: true });

  const result = spawnSync(process.execPath, [
    join(repoRoot, "ai_studio", "game_project", "new_prototype.mjs"),
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
